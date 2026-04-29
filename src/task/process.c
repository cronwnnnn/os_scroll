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

void init_process_manager(){
    id_pool_init(&process_id_pool, PROCESS_NUM, MAX_PROCESS_NUM);
}


pcb_t* create_process(char* name, uint8_t is_kernel_process){
    pcb_t* process = (pcb_t*)kmalloc(sizeof(pcb_t));
    memset(process, 0, sizeof(pcb_t));

    uint32_t pid;
    if(!id_pool_allocate_id(&process_id_pool, &pid)){
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

    process->waiting_child_pid = 0;

    process->waiting_thread_node = nullptr;

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
    add_thread_node_to_schedule(new_thread->crt_node_ptr);

    // 父进程返回子进程的id
    return new_process->id;
}

int32_t process_exec(char* path, int32_t argc, char* argv[]){
    file_stat_t stat;
    if(stat_file(path, &stat) != 0){
        monitor_printf("Command %s not found\n", path);
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

    //========================= 首先消除当前进程的其他线程，保留当前线程 =====================================
    pcb_t* crt_process = get_crt_process();
    tcb_t* crt_thread = get_crt_thread();
    yieldlock_lock(&crt_process->lock);
    
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
                
                // 2. 标记状态为死亡
                iter_thread->status = TASK_DEAD;
                
                // 3. 将它交给垃圾回收线程去自动 kfree
                add_dead_tasks(iter_thread->crt_node_ptr);
                
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
    yieldlock_unlock(&crt_process->lock);

    // 将存在用户栈的argv 和 path保存起来，因为之后要release所有用户内存，避免丢失
    char** args = copy_str_array(argc, argv);
    char* path_copy = (char*)kmalloc(strlen(path) + 1);
    strcpy_with_len(path_copy, path);

    // 除去所有线程的用户空间
    release_user_space_pages();
    //========================= 首先消除当前进程的其他线程，保留当前线程 =====================================

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

    // 为用户栈直接分配物理地址
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

void destroy_process(pcb_t* process){
    Panic("not complete process destroy!!!");
}