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
extern void syscall_fork_exit();


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
        strcpy_with_len(thread->name, name);
    }else{
        char buf[32];
        sprintf(buf, "thread-%u", thread->id);
        strcpy_with_len(thread->name, buf);
    }

    // init thread_note
    thread->crt_node_ptr = &thread->schedule_node;
    thread->schedule_node.prev = NULL;
    thread->schedule_node.next = NULL;
    thread->schedule_node.ptr = thread;
    thread->crt_queue = NULL; 

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
        ss->thread_entry_eip = (uint32_t)switch_to_user_mode;
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

uint32_t prepare_user_stack(
    tcb_t* thread, uint32_t stack_top, int32_t argc, char** argv, uint32_t return_addr){
        uint32_t total_argv_length = 0;

        for(int32_t i = 0; i < argc; i++){
            total_argv_length += strlen(argv[i]) + 1;
        }
        // thread->name的地址也在内核中，也需要拷贝下来
        total_argv_length += strlen(thread->name) + 1;

        // 用于存储参数字符串
        stack_top -= total_argv_length;
        stack_top = stack_top / 4 * 4;

        // 注意这里的args的类型是char**, 是指向char*的数组
        char* args[argc+1];
        char* args_stack_addr = (char*)stack_top;

        int32_t len = strcpy_with_len(args_stack_addr, thread->name);
        args[0] = args_stack_addr;
        args_stack_addr += (len+1);

        // 现在将内核高地址处的字符串参数拷贝到用户栈中
        // 这样在访问时不会造成越权 
        for(int32_t i = 0; i < argc; i++){
            // 先将字符串拷贝到用户栈中，然后args指向该地址
            int32_t len = strcpy_with_len(args_stack_addr, argv[i]);
            args[i+1] = args_stack_addr;
            args_stack_addr += (len+1);
        }


        stack_top -= (uint32_t)(argc+1) * 4;
        uint32_t argv_start = stack_top;
        for(int32_t i = 0; i < argc + 1; i++){
            *((char**)argv_start + i) = args[i];
        }

        // 写入指向指针的指针，argc以及返回地址
        stack_top -= 4;
        *((uint32_t*)stack_top) = argv_start;
        stack_top -= 4;
        *((uint32_t*)stack_top) = (uint32_t)argc + 1;

        stack_top -= 4;
        *((uint32_t*)stack_top) = return_addr;

        interrupt_stack_t* interrupt_stack = (interrupt_stack_t*)((uint32_t)thread->kernel_esp + sizeof(switch_stack_t));
        interrupt_stack->user_esp = stack_top;
        return stack_top;

    }

tcb_t* fork_crt_thread(){
    tcb_t* crt_thread = get_crt_thread();
    Assert(crt_thread != NULL);
    tcb_t* new_thread = (tcb_t*)kmalloc(sizeof(tcb_t));
    Assert(new_thread != NULL);

    memcpy((void*)new_thread, (void*)crt_thread, sizeof(tcb_t));
    new_thread->crt_node_ptr = &new_thread->schedule_node;
    new_thread->schedule_node.prev = NULL;
    new_thread->schedule_node.next = NULL;
    new_thread->schedule_node.ptr = new_thread;
    new_thread->crt_queue = NULL; 

    uint32_t id;
    Assert(id_pool_allocate_id(&thread_id_pool, &id));
    new_thread->id = id;

    char buf[32];
    sprintf(buf, "thread-%u", new_thread->id);
    strcpy_with_len(new_thread->name, buf);

    new_thread->ticks = 0;

    // 需要在当前进程释放吗?不需要，高地址空间是共享的，每个内核都能看到kernel_stack
    uint32_t kernel_stack = (uint32_t)kmalloc_align(KERNEL_STACK_SIZE);
    for(int32_t i = 0; i < KERNEL_STACK_SIZE / PAGE_SIZE; i++){
        map_page(kernel_stack + i * PAGE_SIZE); // <-- 进行了页表映射
    }
    memcpy((void*)kernel_stack, (void*)crt_thread->kernel_stack, KERNEL_STACK_SIZE);

    new_thread->kernel_stack = kernel_stack;
    // 指向应该返回的位置
    new_thread->kernel_esp = kernel_stack + KERNEL_STACK_SIZE - (uint32_t)(sizeof(switch_stack_t) + sizeof(interrupt_stack_t));

    switch_stack_t* ss = (switch_stack_t*)new_thread->kernel_esp; 
    // interrupt_stack_t* is = (interrupt_stack_t*)(new_thread->kernel_esp + sizeof(switch_stack_t)); 

    // 设置resume_thread ret后去到syscall_fork_exit伪装中断出栈，进入到用户态的相同代码处
    ss->thread_entry_eip = (uint32_t)syscall_fork_exit;

    return new_thread;

}
