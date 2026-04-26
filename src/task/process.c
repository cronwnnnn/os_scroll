#include "task/process.h"
#include "task/thread.h"
#include "utils/id_pool.h"
#include "mem/kheap.h"
#include "mem/page.h"
#include "common/stdlib.h"
#include "common/secure.h"
#include "task/scheduler.h"

static id_pool_t process_id_pool;

void add_process_thread(pcb_t* process, tcb_t* new_thread);
void add_child_process(pcb_t* parent, pcb_t* child);

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
    add_thread_to_schedule(new_thread);

    // 父进程返回子进程的id
    return new_process->id;
}


tcb_t* create_new_kernel_thread(pcb_t* process, char* name, void* function){
    tcb_t* new_thread = init_thread(NULL, name, function, THREAD_DEFAULT_PRIORITY, false);
    add_process_thread(process, new_thread);
    return new_thread;
}

tcb_t* create_new_user_thread(
    pcb_t* process, char* name, void* user_function, uint32_t argc, char** argv){
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
