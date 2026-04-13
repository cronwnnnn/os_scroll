#include "sync/cond_var.h"
#include "task/scheduler.h"

extern uint32_t atomic_exchange(volatile uint32_t* dst, uint32_t src);

void cond_var_init(cond_var_t* cv) {
  linked_list_init(&cv->waiting_queue);
}

void cond_var_wait(cond_var_t* cv, yieldlock_t* lock, cv_predicator_func predicator){
    yieldlock_lock(lock);
    while(predicator != NULL && predicator() == false){
        thread_node_t* thread_node = get_crt_thread_node();
        
        // 同样保护 cv 的专用队列不受中断干扰
        disable_interrupt();
        linked_list_append(&cv->waiting_queue, thread_node);
        // 让schedule不要调度运行这个线程，知道被notify恢复
        schedule_mark_thread_block(); 
        enable_interrupt();

        yieldlock_unlock(lock);
        schedule_thread_yield();

        yieldlock_lock(lock);
    }
    // 不在此处解锁，而是让调用者在处理完受保护的资源后再解锁
}



void cond_var_notify(cond_var_t* cv){
    // 保护 wait_queue，使得 notify 成为绝对安全的原子操作
    disable_interrupt();
    if(cv->waiting_queue.size != 0){
        thread_node_t* thread_node = cv->waiting_queue.head;
        linked_list_remove(&cv->waiting_queue, thread_node);
        // 将清理线程加入schedule中，使其可以被调度
        add_thread_node_to_schedule(thread_node);
    }
    enable_interrupt();
}
