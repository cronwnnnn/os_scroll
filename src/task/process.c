#include "task/process.h"
#include "task/thread.h"
#include "utils/id_pool.h"
#include "mem/kheap.h"
#include "mem/page.h"
#include "common/stdlib.h"
#include "common/secure.h"
#include "task/scheduler.h"
#include "fs/vfs.h"
#include "monitor/monitor.h"
#include "elf/elf.h"
#include "mem/gdt.h"
#include "utils/string.h"

static id_pool_t process_id_pool;

void add_process_thread(pcb_t* process, tcb_t* new_thread);
void add_child_process(pcb_t* parent, pcb_t* child);
static void release_user_space_pages();
void clear_crt_process_other_thread_locked();

void init_process_manager(){
    id_pool_init(&process_id_pool, PROCESS_NUM, MAX_PROCESS_NUM);
}


pcb_t* create_process(char* name, uint8_t is_kernel_process){
    pcb_t* process = (pcb_t*)kmalloc(sizeof(pcb_t));
    memset(process, 0, sizeof(pcb_t));

    int32_t pid;
    if(!id_pool_allocate_id(&process_id_pool, (uint32_t*)&pid)){
        return NULL;
    }
    process->id = pid;
    if(name != NULL){
        strcpy_with_len(process->name, name);
    }else{
        char buf[32];
        sprintf(buf, "process-%u", pid);
        strcpy_with_len(process->name, buf);
    }


    process->parent = nullptr;

    process->status = PROCESS_NORMAL;

    hash_table_init(&process->threads);

    process->user_thread_stack_indexes = bitmap_create(nullptr, USER_PRCOESS_THREDS_MAX);

    process->is_kernel_process = is_kernel_process;

    process->exit_code = 0;

    hash_table_init(&process->children_processes);

    hash_table_init(&process->exit_children_processes);

    // 看哪些在等我
    process->exit_wait_queue = NULL;
    process->wait_any_exit_queue = NULL;
    process->waiting_threads_count = 0;


    process->page_dir = clone_crt_page_dir();
    yieldlock_init(&process->page_dir_lock);

    yieldlock_init(&process->lock);

    return process;

}

pcb_t* create_and_add_process(char* name, uint8_t is_kernel_process){
    pcb_t* process = create_process(name, is_kernel_process);
    add_new_process(process);

    return process;
}

int32_t process_fork(){
    pcb_t* new_process = create_process(NULL, false);

    pcb_t* parent_process = get_crt_process();
    add_child_process(parent_process, new_process);
    new_process->parent = parent_process;

    tcb_t* new_thread = fork_crt_thread();
    Assert(new_thread != NULL);

    add_process_thread(new_process, new_thread);
    add_new_process(new_process);
    // 同时复制了上一个线程的所有内容，包括用户栈地址，因此在这个进程将该用户栈地址置为已占有，防止覆盖
    bitmap_set_bit(&new_process->user_thread_stack_indexes, new_thread->user_stack_index);
    // 最后才放入调度队列，防止提前调度
    add_thread_node_to_schedule(new_thread->crt_node_ptr);

    // 父进程返回子进程的id
    return new_process->id;
}

int32_t process_exec(char* path, int32_t argc, char* argv[]){
    file_stat_t stat;
    if(stat_file(path, &stat) != 0){
        monitor_printf("file %s not found\n", path);
        return -1;
    }

    // 从磁盘中读出elf文件
    uint32_t size = stat.size;
    Assert(size != 0);
    char* read_buffer = (char*)kmalloc(size);
    if(read_file(path, read_buffer, 0, size) != size){
        monitor_printf("Failed to read file %s\n", path);
        kfree(read_buffer);
        return -1;
    }

    // 在磁盘 I/O 完成后，即将开始不可逆的资源重分配，必须关闭中断
    // 防止在用户空间页表被清空、或旧线程被清理一半时，调度器意外介入
    disable_interrupt();

    pcb_t* crt_process = get_crt_process();
    tcb_t* crt_thread = get_crt_thread();
    //========================= 首先消除当前进程的其他线程，保留当前线程 =====================================
    // 包括消除所有线程的用户栈index???
    yieldlock_lock(&crt_process->lock);
    clear_crt_process_other_thread_locked();
    yieldlock_unlock(&crt_process->lock);
    //========================= 首先消除当前进程的其他线程，保留当前线程 =====================================

    // 将存在用户栈的argv 和 path保存起来，因为之后要release所有用户内存，避免丢失
    char** args = copy_str_array(argc, argv);
    char* path_copy = (char*)kmalloc(strlen(path) + 1);
    strcpy_with_len(path_copy, path);

    // 除去所有线程的用户空间
    release_user_space_pages();

    // ============================== 为当前线程分配新的用户栈 =======================================
    uint32_t stack_index;
    yieldlock_lock(&crt_process->lock);
    if(!bitmap_alloc_first_free(&crt_process->user_thread_stack_indexes, &stack_index)){
        yieldlock_unlock(&crt_process->lock);
        return -1;
    }
    yieldlock_unlock(&crt_process->lock);

    crt_thread->user_stack_index = stack_index;
    uint32_t user_stack_top = USER_STACK_TOP - stack_index * USER_STACK_SIZE;
    crt_thread->user_stack = user_stack_top;

    // 为用户栈直接分配物理地址
    map_page(user_stack_top - PAGE_SIZE);

    uint32_t kernel_stack = crt_thread->kernel_stack;
    crt_thread->kernel_esp = kernel_stack + KERNEL_STACK_SIZE - (sizeof(interrupt_stack_t) + sizeof(switch_stack_t)); 
    prepare_user_stack(crt_thread, user_stack_top, argc, args, (uint32_t)schedule_thread_normal_exit);
    // ============================== 为当前线程分配新的用户栈 =======================================
    

    // 加载elf文件到内存
    uint32_t entry_addr;
    if(load_elf(read_buffer, &entry_addr) == -1){
        Panic("Failed to load elf file");
        monitor_printf("load elf file %s failed\n", path_copy);
        return -1;
    }
    kfree(read_buffer);

    // 为进程和线程分配新名字
    strcpy_with_len(crt_process->name, path_copy);
    strcpy_with_len(crt_thread->name, path_copy);

    // ============================ 修改当前线程的中断栈，让其返回后转至loader file ====================================
    interrupt_stack_t* is = (interrupt_stack_t*)(kernel_stack + KERNEL_STACK_SIZE - sizeof(interrupt_stack_t));
    is->ds = SELECTOR_U_DATA;

    // general regs
    is->edi = 0;
    is->esi = 0;
    is->ebp = 0;
    is->esp = 0;
    is->ebx = 0;
    is->edx = 0;
    is->ecx = 0;
    is->eax = 0;

    // user-level code env
    is->eip = entry_addr;
    is->cs = SELECTOR_U_CODE;
    is->eflags = EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1;

    // user stack
    is->user_ss = SELECTOR_U_DATA;
    // ============================ 修改当前线程的中断栈，让其返回后转至loader file ====================================

    destroy_str_array(argc, args); // 释放临时字符串数组
    kfree(path_copy); // 释放临时路径

    // 接下来直接降为user态并运行该线程
    disable_interrupt();
    
    uint32_t final_esp = (uint32_t)is;

    asm volatile (
        "mov %0, %%esp\n\t"  // 栈顶指回布置好的中断栈
        "jmp interrupt_exit"
        : : "r" (final_esp) : "memory"
    );

    // 永远不会走到这里！
    Panic("exec reached unreachable point!");
    return -1;

}

tcb_t* create_new_kernel_thread(pcb_t* process, char* name, void* function){
    tcb_t* new_thread = init_thread(NULL, name, function, THREAD_DEFAULT_PRIORITY, false);
    add_process_thread(process, new_thread);
    return new_thread;
}

tcb_t* create_new_user_thread(
    pcb_t* process, char* name, void* user_function, int32_t argc, char** argv){
    tcb_t* new_thread = init_thread(NULL, name, user_function, THREAD_DEFAULT_PRIORITY, true);
    add_process_thread(process, new_thread);

    // 为新的线程分配用户栈空间，内核栈空间不用分配，在init_thread时自动malloc了
    uint32_t stack_index;
    yieldlock_lock(&process->lock);
    if(!bitmap_alloc_first_free(&process->user_thread_stack_indexes, &stack_index)){
        yieldlock_unlock(&process->lock);
        return NULL;
    }
    yieldlock_unlock(&process->lock);

    new_thread->user_stack_index = stack_index;
    uint32_t user_stack_top = USER_STACK_TOP - stack_index * USER_STACK_SIZE;
    new_thread->user_stack = user_stack_top;

    // 为用户栈直接分配物理地址,先分配一页，后面的用缺页来扩展
    map_page(user_stack_top - PAGE_SIZE);

    prepare_user_stack(new_thread, user_stack_top, argc, argv, (uint32_t)schedule_thread_normal_exit);

    return new_thread;
}

void add_process_thread(pcb_t* process, tcb_t* new_thread){
    new_thread->process = process;
    yieldlock_lock(&process->lock);
    hash_table_put(&process->threads, new_thread->id, new_thread);
    yieldlock_unlock(&process->lock);
}

void remove_process_thread(pcb_t* process, tcb_t* new_thread){
    yieldlock_lock(&process->lock);
    tcb_t* remove_thread = hash_table_remove(&process->threads, new_thread->id);
    Assert(remove_thread == new_thread);
    new_thread->process = NULL;
    if(new_thread->user_stack_index >= 0){
        bitmap_clear_bit(&process->user_thread_stack_indexes, new_thread->user_stack_index);
    }
    yieldlock_unlock(&process->lock);
}

void add_child_process(pcb_t* parent, pcb_t* child) {
  yieldlock_lock(&parent->lock);
  hash_table_put(&parent->children_processes, child->id, child);
  yieldlock_unlock(&parent->lock);
}

static void release_user_space_pages() {
  // User virtual space is 4MB - 3G, totally 1024 * 3/4 - 1 = 767 page dir entries.
  release_pages(4 * 1024 * 1024, 767 * 1024, true);
  release_page_tables(1, 767);
}

// 消除当前进程的全部资源，但不包括自身以及页目录
void release_crt_process_resources_locked(){
    pcb_t* process = get_crt_process();
    // 消除当前进程申请的所有数据结构
    hash_table_destroy(&process->threads);
    hash_table_destroy(&process->exit_children_processes);
    // ?? 这个不能销毁，需要给父进程？？？
    hash_table_destroy(&process->children_processes);
    
    bitmap_destroy(&process->user_thread_stack_indexes);
    release_user_space_pages();
}

void destroy_process(pcb_t* process){
    // 在父进程中释放掉死亡进程的所有不能自己释放的资源
    // 注意page_directory_entries是* PAGE_SIZE之后的值，release前要 /PAGE_SIZE
    uint32_t frame = process->page_dir.page_directory_entries / PAGE_SIZE;
    release_phy_frame(frame);
    id_pool_free_id(&process_id_pool,process->id);
    kfree(process);
}

int32_t process_wait(int32_t pid, uint32_t* status){
    Assert(pid >= 0);

    pcb_t* process = get_crt_process();
    thread_node_t* thread_node = get_crt_thread_node();

    yieldlock_lock(&process->lock);

    // 用于子进程在退出时判断需不需要唤醒父进程，若是父进程的pid则唤醒, 0则表示根本没有等待
    wait_node_t my_node;
    my_node.next = NULL;
    my_node.target_pid = pid;
    // 指向自己，表示是自己正在等
    my_node.task = thread_node;

    pcb_t* child = NULL;

    bool counted_waiter = false;
    bool in_queue = false;
    while(1){
        // 根本没有子进程，等个蛋
        if(process->children_processes.size == 0){
            if(counted_waiter){
                Assert(process->waiting_threads_count > 0);
                process->waiting_threads_count--;
            }
            yieldlock_unlock(&process->lock);
            return -1;
        }
        // 子进程根本就没有这个pid的，直接退出
        if (pid > 0 && hash_table_get(&process->children_processes, pid) == nullptr) {
            if(counted_waiter){
                Assert(process->waiting_threads_count > 0);
                process->waiting_threads_count--;
            }
            yieldlock_unlock(&process->lock);
            return -1;
        }
        hash_table_t* exit_children = &process->exit_children_processes;
        if(exit_children->size > 0){
            // 如果等任意一个child死亡
            if(pid == 0){
                child = hash_table_get_first(exit_children);
                Assert(child != NULL);
                child = hash_table_remove(exit_children, child->id);
                Assert(child != NULL);
                break;
            }else{
                // 等特定进程死亡
                child = hash_table_remove(exit_children, pid);
                if(child != NULL){
                    break;
                }
            }
        }
        // 如果要找到进程现在没有退出，就设为阻塞并等待唤醒
        // 当前只能一个进程下有一个线程等待，不允许多个！！！

        if(in_queue == false){
            if(!counted_waiter){
                process->waiting_threads_count++;
                counted_waiter = true;
            }
            if(pid == 0){
                // 等待任何一个，就加入自己的等全部队列
                my_node.next = process->wait_any_exit_queue;
                process->wait_any_exit_queue = &my_node;
            }else if(pid > 0){
                // 等特定的，就加要等的进程的队列
                pcb_t* wait_child = hash_table_get(&process->children_processes, pid);
                yieldlock_lock(&wait_child->lock);
                my_node.next = wait_child->exit_wait_queue;
                wait_child->exit_wait_queue = &my_node;
                yieldlock_unlock(&wait_child->lock);
            }
            in_queue = true;
        }
        schedule_mark_thread_block();
        yieldlock_unlock(&process->lock);
        schedule_thread_yield();
        // 回来之后继续上锁
        yieldlock_lock(&process->lock);
        // 被唤醒代表被提出等待队列了，需要重新加入
        in_queue = false;

    }

    // 找到要等的进程了
    // 也需要把children_processes中删去child
    Assert(child != NULL);
    pcb_t* removed = hash_table_remove(&process->children_processes, child->id);
    Assert(removed == child);
    if (status != nullptr) {
        *status = child->exit_code;
    }
    if(counted_waiter){
        Assert(process->waiting_threads_count > 0);
        process->waiting_threads_count--;
    }

    yieldlock_unlock(&process->lock);
    // 等clean线程清除他
    add_dead_process(child);

    return 0;
}

void process_exit(int32_t exit_code){
    pcb_t* process = get_crt_process();
    tcb_t* thread = get_crt_thread();
    pcb_t* parent = process->parent;
    Assert(parent != NULL);
    Assert(process != get_init_process());

    // 对于结束进程这样涉及大量资源改变的底层操作，需要整个过程不可被中断切割
    uint32_t init_interrupt_status = interrupt_disable();

    yieldlock_lock(&process->lock);
    if(process->waiting_threads_count > 0){
        Panic("process_exit: other thread is waiting in process_wait");
    }
    // 除了当前线程，还有其他线程在运行
    if(process->threads.size > 1){
        clear_crt_process_other_thread_locked();
    }
    yieldlock_unlock(&process->lock);

// ================================= 若当前进程还有子进程，扔给init进程============================================
    pcb_t* init_process = get_init_process();
    yieldlock_lock(&init_process->lock);
    yieldlock_lock(&process->lock);
    // 先移动子进程
    while (process->children_processes.size > 0) {
        pcb_t* child = hash_table_get_first(&process->children_processes);
        Assert(child != NULL);

        child = hash_table_remove(&process->children_processes, child->id);
        Assert(child != NULL);

        child->parent = init_process;
        hash_table_put(&init_process->children_processes, child->id, child);
    }
    // 在把已退出的放到init进程中
    while (process->exit_children_processes.size > 0) {
        pcb_t* child = hash_table_get_first(&process->exit_children_processes);
        Assert(child != NULL);

        child = hash_table_remove(&process->exit_children_processes, child->id);
        Assert(child != NULL);

        child->parent = init_process;
        hash_table_put(&init_process->children_processes, child->id, child);
        hash_table_put(&init_process->exit_children_processes, child->id, child);
    }
    // 由于把已退出的放入了，需要再唤醒init，让其清理
    while (init_process->wait_any_exit_queue != NULL) {
        Assert(init_process->wait_any_exit_queue->target_pid == 0);
        add_thread_node_to_schedule(init_process->wait_any_exit_queue->task);
        init_process->wait_any_exit_queue = init_process->wait_any_exit_queue->next;
    }
    yieldlock_unlock(&process->lock);
    yieldlock_unlock(&init_process->lock);
// ================================= 若当前进程还有子进程，扔给init进程============================================

    yieldlock_lock(&process->lock);
    tcb_t* removed_thread = hash_table_remove(&process->threads, thread->id);
    Assert(removed_thread == thread);

    process->exit_code = exit_code;
    process->status = PROCESS_EXIT;
    release_crt_process_resources_locked();
    // 在release_crt_process_resources_locked之后再将thread的process指向null
    // 否则会直接assert报错，因为release_crt_process_resources_locked 找不到当前process了
    thread->process = nullptr;


    // 保证是先上锁父进程再上锁子进程，保持和wait代码的一致
    yieldlock_unlock(&process->lock);
    yieldlock_lock(&parent->lock);
    yieldlock_lock(&process->lock);
    // 把当前进程加入父母的退出进程队列，且如果进程正在等待自己或者任何一个进程结束，则唤醒他
    hash_table_put(&parent->exit_children_processes, process->id, process);
    // 父进程真的在wait，才会唤醒线程

    // 将这两个队列中的wait线程都唤醒
    while (process->exit_wait_queue != NULL){
        Assert(process->exit_wait_queue->target_pid == process->id);
        add_thread_node_to_schedule(process->exit_wait_queue->task);
        process->exit_wait_queue = process->exit_wait_queue->next;
    }
    while (parent->wait_any_exit_queue != NULL){
        Assert(parent->wait_any_exit_queue->target_pid == 0);
        add_thread_node_to_schedule(parent->wait_any_exit_queue->task);
        parent->wait_any_exit_queue = parent->wait_any_exit_queue->next;
    }
    
    yieldlock_unlock(&process->lock);
    yieldlock_unlock(&parent->lock);

    // 此时仍然是关中断状态，可以直接调用 exit，它内部又会 disable 一次(安全重复)并 context_switch 不再回来
    schedule_thread_exit();
    
    // 这里永远不会执行到了
    interrupt_restore(init_interrupt_status);
}


// 清除当前进程除了这个线程的所有其他线程，用于process_exec和process_exit
// 还会消除所有线程的用户栈index，慎用，否则可能导致两个thread栈冲突
void clear_crt_process_other_thread_locked(){
    pcb_t* crt_process = get_crt_process();
    tcb_t* crt_thread = get_crt_thread();
    
    for (int32_t i = 0; i < crt_process->threads.buckets_num; i++) {
        linked_list_t* bucket = &crt_process->threads.buckets[i];
        linked_list_node_t* kv_node = bucket->head;
        while (kv_node != nullptr) {
            hash_table_kv_t* kv = (hash_table_kv_t*)kv_node->ptr;
            tcb_t* iter_thread = (tcb_t*)kv->v_ptr;
            
            // 找出属于当前进程的，但不是自己这个执行 exec 的线程
            if (iter_thread != crt_thread) {
                // 1. 从当前所处的调度队列或阻塞队列中强行摘下
                remove_thread_from_current_queue(iter_thread);
                
                // 2. 将它交给垃圾回收线程去自动 kfree
                add_dead_tasks(iter_thread->crt_node_ptr);

                // 3. 标记状态为死亡
                iter_thread->status = TASK_DEAD;
                
                // 4. 清除它在这个进程内部占用的位图等信息（相当于原地 remove_process_thread）
                // 在外部一次性清空
            }
            kv_node = kv_node->next;
        }
    }
    
    // 把进程哈希表中的所有线程彻底清空（其实通过上面的循环可以安全删，但为了防止破坏链表遍历，通常遍历完后再重置）
    hash_table_destroy(&crt_process->threads); 
    hash_table_init(&crt_process->threads); // 重新初始化为一个空的 map
    
    // 把自己（crt_thread）重新加回进程管理中
    hash_table_put(&crt_process->threads, crt_thread->id, crt_thread);

    bitmap_clear(&crt_process->user_thread_stack_indexes);
}