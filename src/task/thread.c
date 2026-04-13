#include "task/thread.h"
#include "common/types.h"
#include "mem/kheap.h"
#include "common/stdlib.h"
#include "common/secure.h"
#include "mem/page.h"
#include "utils/id_pool.h"
#include "mem/gdt.h"
#include "task/scheduler.h"

extern void switch_to_user_mode();


static id_pool_t thread_id_pool;

void init_task_manager() {
  id_pool_init(&thread_id_pool, 2048, 32768);
}

static void kernel_thread(thread_func* function) {
  function();
  schedule_thread_exit();
}

tcb_t* init_thread(tcb_t* thread, char* name, thread_func* function, uint32_t priority, uint8_t is_user_thread){
    if(thread == NULL){
        thread = (tcb_t*)kmalloc(sizeof(tcb_t));
        memset(thread, 0, sizeof(tcb_t));
    }

    uint32_t id;
    if(!id_pool_allocate_id(&thread_id_pool, &id)){
        return NULL;
    }
    thread->id = id;


    if(name != NULL){
        strcpy(thread->name, name);
    }else{
        char buf[32];
        sprintf(buf, "thread-%u", thread->id);
        strcpy(thread->name, buf);
    }

    thread->status = TASK_READY;
    thread->ticks = 0;
    thread->priority = priority;
    thread->user_stack_index = -1;
    thread->need_reschedule = false;

    uint32_t kernel_stack = (uint32_t)kmalloc_align(KERNEL_STACK_SIZE);
    Assert(kernel_stack != 0);

    for(int32_t i = 0; i < KERNEL_STACK_SIZE / PAGE_SIZE; i++){
        map_page(kernel_stack + i * PAGE_SIZE);
    }

    memset((void*)kernel_stack, 0, KERNEL_STACK_SIZE);
    thread->kernel_stack = kernel_stack;
    thread->kernel_esp = kernel_stack + KERNEL_STACK_SIZE - (sizeof(interrupt_stack_t) + sizeof(switch_stack_t));
    switch_stack_t* ss = (switch_stack_t*) thread->kernel_esp;

    ss->edi = 0;
    ss->esi = 0;
    ss->ebp = 0;
    ss->ebx = 0;
    ss->edx = 0;
    ss->ecx = 0;
    ss->eax = 0;

    // function 是工作函数
    // entry是thread要运行的函数，除了function还需要调度线程
    ss->thread_entry_eip = (uint32_t)kernel_thread;
    ss->function = function;

    if(is_user_thread){
        interrupt_stack_t* interrupt_stack = (interrupt_stack_t*)((uint32_t)thread->kernel_esp + sizeof(switch_stack_t));
        // data segemnts
        interrupt_stack->ds = SELECTOR_U_DATA;

        // general regs
        interrupt_stack->edi = 0;
        interrupt_stack->esi = 0;
        interrupt_stack->ebp = 0;
        interrupt_stack->esp = 0;
        interrupt_stack->ebx = 0;
        interrupt_stack->edx = 0;
        interrupt_stack->ecx = 0;
        interrupt_stack->eax = 0;

        // user-level code env
        interrupt_stack->eip = (uint32_t)function;
        interrupt_stack->cs = SELECTOR_U_CODE;
        interrupt_stack->eflags = EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1;

        // user stack
        interrupt_stack->user_ss = SELECTOR_U_DATA;

    }

    return thread;

}

void destroy_thread(tcb_t* thread){
    id_pool_free_id(&thread_id_pool, thread->id);
    kfree((void*)thread->kernel_stack);
    kfree(thread);
}