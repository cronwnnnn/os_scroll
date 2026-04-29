#ifndef TASK_THREAD_H
#define TASK_THREAD_H
#include "common/types.h"
#include "interrupt/interrupt.h"
#include "utils/linked_list.h"


typedef void thread_func();

#define KERNEL_STACK_SIZE 8192
#define THREAD_DEFAULT_PRIORITY 10

#define EFLAGS_MBS    (1 << 1)
#define EFLAGS_IF_0   (0 << 9)
#define EFLAGS_IF_1   (1 << 9)
#define EFLAGS_IOPL_0 (0 << 12)
#define EFLAGS_IOPL_3 (3 << 12)

struct  process_struct;
typedef struct process_struct pcb_t;

typedef isr_params_t interrupt_stack_t;
typedef linked_list_node_t thread_node_t;


enum task_status {
  TASK_RUNNING,
  TASK_READY,
  TASK_BLOCKED,
  TASK_WAITING,
  TASK_HANGING,
  TASK_DEAD
};


struct task_control_block{
    uint32_t kernel_esp;    // 指向栈的当前esp处
    uint32_t kernel_stack;  // 指向栈的最低地址,防止栈越界

    uint32_t id;
    char name[32];
    enum task_status status;
    uint8_t priority;

    uint32_t ticks;  // 当前线程跑了多久，用于线程切换
    
    uint32_t user_stack;
    int32_t user_stack_index;

    // need reschedule flag
    bool need_reschedule;
    pcb_t* process;

    int32_t preempt_count;
    linked_list_t*  crt_queue;        // 它正在挂在哪个队列下面 (ready?, waiting?,dead?)
    thread_node_t  schedule_node;     // 它对应的thread_node
    thread_node_t*  crt_node_ptr;     // 它对应的thread_node

};

typedef struct task_control_block tcb_t;


// 用于切换线程用的结构体，根据中断入栈的顺序定义了所有寄存器值和返回地址，
// 用于切换过来时将这些值出栈，从而达到换线程
struct switch_stack {
  // Switch context.
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;

  // For thread first run.
  uint32_t thread_entry_eip;

  void (*unused_retaddr);
  thread_func* function;
};
typedef struct switch_stack switch_stack_t;

tcb_t* init_thread(tcb_t* thread, char* name, thread_func* function, uint32_t priority, uint8_t is_user_thread);
void destroy_thread(tcb_t* thread);
void init_task_manager();
uint32_t prepare_user_stack(tcb_t* thread, uint32_t stack_top, int32_t argc, char** argv, uint32_t return_addr);
tcb_t* fork_crt_thread();









#endif
