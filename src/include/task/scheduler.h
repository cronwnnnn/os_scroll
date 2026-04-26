#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H
#include "utils/linked_list.h"
#include "task/thread.h"
#include "common/types.h"

tcb_t* get_crt_thread();
thread_node_t* get_crt_thread_node();


void init_scheduler();
void schedule_thread_yield();

// 在中断处理块结束时调用
void schedule();

void schedule_thread_exit();

bool is_kernel_main_thread();

// 设置当前线程的状态为waitting，
void schedule_mark_thread_block();

void add_thread_to_schedule(tcb_t* thread);
void add_thread_node_to_schedule(thread_node_t* thread_node);
void add_thread_node_to_schedule_head(thread_node_t* thread_node);

void schedule_thread_normal_exit();
void add_new_process(pcb_t* process);
pcb_t* get_crt_process();

bool multi_task_is_enabled();

#endif