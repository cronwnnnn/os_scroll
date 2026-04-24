#include "task/scheduler.h"
#include "common/types.h"
#include "task/thread.h"
#include "utils/linked_list.h"
#include "interrupt/interrupt.h"
#include "common/secure.h"
#include "monitor/monitor.h"
#include "utils/hash_table.h"
#include "sync/yieldlock.h"
#include "mem/kheap.h"
#include "sync/cond_var.h"
#include "mem/gdt.h"
#include "mem/page.h"


extern void context_switch(tcb_t* old_thread, tcb_t* new_thread);
extern void cpu_idle();
extern void resume_thread();
void do_context_switch();
static void kernel_main_thread();
static void kernel_clean_thread();
static bool has_dead_resource();
void process_switch(pcb_t* process);



thread_node_t* get_crt_thread_node();
tcb_t* get_crt_thread();


//static pcb_t* main_process;

static thread_node_t* crt_thread_node = NULL;
static thread_node_t* main_thread_node;
static thread_node_t* kernel_clean_node;

static hash_table_t processes_map;
static yieldlock_t processes_map_lock;
static hash_table_t threads_map;
static yieldlock_t threads_map_lock;

static linked_list_t ready_tasks;
static linked_list_t dead_tasks;
static linked_list_t dead_processes;
static yieldlock_t dead_resource_lock;
static cond_var_t dead_cv;

static bool main_thread_in_ready_queue = false;
static bool multi_task_enabled = false;


void test_thread();
void test_thread2();

bool multi_task_is_enabled() {
  return multi_task_enabled;
}

void init_scheduler(){
    disable_interrupt();
    linked_list_init(&ready_tasks);

    linked_list_init(&dead_tasks);
    linked_list_init(&dead_processes);
    yieldlock_init(&dead_resource_lock);
    cond_var_init(&dead_cv);

    hash_table_init(&processes_map);
    yieldlock_init(&processes_map_lock);
    yieldlock_init(&threads_map_lock);
    hash_table_init(&threads_map);

    //main_process = create_process("kernel_main_process", true);
    tcb_t* main_thread = init_thread(NULL, "main_thread", kernel_main_thread, THREAD_DEFAULT_PRIORITY, false);
    main_thread_node = (thread_node_t*)kmalloc(sizeof(thread_node_t));
    main_thread_node->ptr = main_thread;
    crt_thread_node = main_thread_node;

    asm volatile (
   "movl %0, %%esp; \
    jmp resume_thread": : "g" (main_thread->kernel_esp) : "memory");

    Panic("thread wrong, shoundn't be here");

}

void test_thread(){
    //int32_t i = 100;
    while (1)
    {
        monitor_print("thread1111 running!\n");
        // 增加空循环或者主动释放cpu，防止一直占用 monitor_lock 导致线程2饥饿
        int delay = 100000;
        while(delay--);
    }
}

void test_thread2(){
    //int32_t i = 100;
    while (1)
    {
        monitor_print("thread2222 running!\n");
        // 增加空循环或者主动释放cpu，防止一直占用 monitor_lock 导致线程1饥饿
        int delay = 100000;
        while(delay--);
    }
}

void user_mode_test_program() {
    // 此时我们期望自己已经身处 Ring 3 
    // 不能调用 monitor_print! 否则会触发 Page Fault (缺页/权限越界)
    
    // 故意执行一条只有 Ring 0 才能执行的特权指令：关闭中断
    asm volatile ("cli"); 

    // 如果能走到这里，说明在 Ring 0，测试失败。
    while(1);
}

/*
void scheduler_thread(){
    first_thread = init_thread(NULL, "test1", test_thread, THREAD_DEFAULT_PRIORITY, false);
    linked_list_append(ready_tasks,)
}
*/

static void kernel_main_thread(){
    tcb_t* clean_thread = init_thread(NULL, "clean_thread", kernel_clean_thread, THREAD_DEFAULT_PRIORITY, false);
    kernel_clean_node = (thread_node_t*)kmalloc(sizeof(thread_node_t));
    kernel_clean_node->ptr = clean_thread;
    add_thread_node_to_schedule(kernel_clean_node);

    // 追加测试线程，以便让它们能被真正执行到并打印
    tcb_t* first_t = init_thread(NULL, "test1", test_thread, THREAD_DEFAULT_PRIORITY, false);
    tcb_t* second_t = init_thread(NULL, "test2", test_thread2, THREAD_DEFAULT_PRIORITY, false);
    add_thread_to_schedule(first_t);
    add_thread_to_schedule(second_t);

    tcb_t* user_t = init_thread(NULL, "user_test", user_mode_test_program, THREAD_DEFAULT_PRIORITY, true);
    uint32_t fake_user_stack = (uint32_t)kmalloc_align(PAGE_SIZE);
    map_page(fake_user_stack);
    
    // 伪造一点参数压栈
    char* argv[] = {"test_prog"};
    prepare_user_stack(user_t, fake_user_stack + PAGE_SIZE, 1, argv, 0);

    // 把这个用户线程加入调度队列
    add_thread_to_schedule(user_t);


    enable_interrupt();
    multi_task_enabled = true;

    while (true) {
        cpu_idle();
    }

}

// 用于处理结束的线程
static void kernel_clean_thread(){
    while(1){
        cond_var_wait(&dead_cv, &dead_resource_lock, has_dead_resource);
        
        // 此时我们仍然持有 dead_resource_lock (由 cond_var_wait 保证)
        linked_list_t clearing_tasks;
        linked_list_move(&clearing_tasks, &dead_tasks);
        
        // 我们安全地完成了对 dead_tasks 的提取，释放锁，让其他线程可以继续追加 dead_tasks
        yieldlock_unlock(&dead_resource_lock);

        // 对于每个node，要释放其在链表中指向的地址，还要释放node中存着的thread的内容以及其指向的thread_kernel内容
        // 最后释放掉当前node
    // 注意：`clearing_tasks` 是你从 `dead_tasks` 里 move 出来的新链表
        while (clearing_tasks.size > 0)
        {
            monitor_print("clean 1 thread\n");
            linked_list_node_t* head = clearing_tasks.head;
            linked_list_remove(&clearing_tasks, head);
            tcb_t* thread = (tcb_t*)head->ptr;
            destroy_thread(thread);
            kfree(head);
        }
         
        // 清理完退出
        schedule_thread_yield();

    }
    
}




void do_context_switch(){

    if (ready_tasks.size == 0) {
        linked_list_append(&ready_tasks, main_thread_node);
        main_thread_in_ready_queue = 1;
    }

    tcb_t* old_thread = get_crt_thread();
    thread_node_t* old_thread_node = get_crt_thread_node();
    // 只有在当前线程是running状态且不是空闲线程时才会被重新放入队列
    // 如果是waiting状态，不能放入，要等待cond_var_notify设其为ready之后
    // 如果是dead，只能被清理，不会进入
    // 如果是空闲进程main_thread，则也不能主动进入队列，只有队列中无任何线程才会加入并运行
    if(old_thread->status == TASK_RUNNING && old_thread_node != main_thread_node){
        old_thread->status = TASK_READY;
        // 把马上被挂起的旧任务的 node 放回队列(目前只有 first/second)
        linked_list_append(&ready_tasks, old_thread_node);
    }
    old_thread->ticks = 0;
    old_thread->need_reschedule = false;

    thread_node_t* next_thread_node = ready_tasks.head;
    tcb_t* next_thread = (tcb_t*)(next_thread_node->ptr);
    
    // 在每次切换线程时都更改tss
    // 由于tss只用于用户线程，而对于用户来说唯一要用到内核栈的时候就是中断
    // 而每次中断结束之后用户线程的内核栈都变成空的了，因此esp指向最高处即可，即每次调用都是空的
    update_tss_esp(next_thread->kernel_stack + KERNEL_STACK_SIZE);

    //如果下一个是空闲线程，就设为0，表示不在队列中，保证队列中最多一个空闲线程
    // 防止每个线程退出都送一个空闲线程进入队列
    next_thread->status = TASK_RUNNING;
    if(next_thread_node == main_thread_node){
        main_thread_in_ready_queue = 0;
    }

    if(next_thread->process != old_thread->process){
        process_switch(next_thread->process);
    }
    
    // 把下一个任务的 node 从从队列中拿出来
    linked_list_remove(&ready_tasks, next_thread_node);  
    
    /*
    // Setup env for next thread (and maybe a different process)
    update_tss_esp(next_thread->kernel_stack + KERNEL_STACK_SIZE);
    */

    crt_thread_node = next_thread_node;
    context_switch(old_thread, next_thread);

}

void schedule(){
    if(!multi_task_enabled){
        return;
    }
    
    disable_interrupt();
    tcb_t* thread = get_crt_thread();
    if(thread->preempt_count > 0){
        // 注意：如果在中断中调用 schedule，且因为抢占计数退出了，
        // 这里千万不能随意打开中断，否则接下来的中断返回上下文会被破坏。
        return;
    }

    bool need_switch = false;
    if(ready_tasks.size > 0){
        need_switch = thread->need_reschedule;
    }

    if(need_switch){
        do_context_switch();
    }
}

void process_switch(pcb_t* process) {
    // 将当前cr3指向要转换的进程的页表并刷新tlb
    reload_page_dir(&process->page_dir);
}

void add_thread_to_schedule(tcb_t* thread){
    thread_node_t* thread_node = (thread_node_t*)kmalloc(sizeof(thread_node_t));
    thread_node->ptr = thread;
    add_thread_node_to_schedule(thread_node);
}

void add_thread_node_to_schedule(thread_node_t* thread_node){
    disable_interrupt();
    tcb_t* thread = thread_node->ptr; 
    if(thread->status != TASK_DEAD){
        thread->status = TASK_READY;
    }
    linked_list_append(&ready_tasks, thread_node);
    enable_interrupt();
}

void add_thread_node_to_schedule_head(thread_node_t* thread_node) {
    disable_interrupt();
    tcb_t* thread = (tcb_t*)thread_node->ptr;
    if(thread->status != TASK_DEAD){
        thread->status = TASK_READY;
    }
    linked_list_insert_to_head(&ready_tasks, thread_node);
    enable_interrupt();
}

void add_dead_tasks(thread_node_t* thread){
    yieldlock_lock(&dead_resource_lock);
    linked_list_append(&dead_tasks, thread);
    cond_var_notify(&dead_cv);
    yieldlock_unlock(&dead_resource_lock);
}


void schedule_thread_yield(){
    disable_interrupt();
    
    // 内部已经自动判断ready_task中是否有值了
    do_context_switch();
}

void schedule_thread_exit(){
    thread_node_t* threadnode = get_crt_thread_node();
    Assert(threadnode != NULL);
    tcb_t* thread = (tcb_t*)threadnode->ptr;
    thread->status = TASK_DEAD;
    add_dead_tasks(threadnode);
    disable_interrupt();
    do_context_switch();
}

void schedule_thread_normal_exit(){
    Panic("should not be here!!! in normal exit");
}

void schedule_mark_thread_block() {
  tcb_t* thread = (tcb_t*)crt_thread_node->ptr;
  thread->status = TASK_WAITING;
}


static bool has_dead_resource() {
  return  dead_processes.size > 0 || dead_tasks.size > 0;
}

tcb_t* get_crt_thread(){
    Assert(crt_thread_node != NULL);
    Assert(crt_thread_node->ptr != NULL);
    return (tcb_t*)crt_thread_node->ptr;
}

thread_node_t* get_crt_thread_node(){
    Assert(crt_thread_node != NULL);
    return crt_thread_node;
}

void add_new_process(pcb_t* process) {
  yieldlock_lock(&processes_map_lock);
  hash_table_put(&processes_map, process->id, process);
  yieldlock_unlock(&processes_map_lock);
}


