#include "interrupt/time.h"
#include "interrupt/interrupt.h"
#include "common/io.h"
#include "monitor/monitor.h"
#include "task/thread.h"
#include "task/scheduler.h"


uint32_t tick = 0;

extern void context_switch(tcb_t* old_thread, tcb_t* new_thread);


uint32_t getTick() {
  return tick;
}

static void timer_callback(isr_params_t regs){
    // 每当时钟中断发生时，tick加1，并且每秒打印一次
    /*
    if(tick % TIMER_FREQUENCY == 0){
        monitor_printf("seconds: %d \n", tick / TIMER_FREQUENCY);
    }*/
    tick++;

    if(multi_task_is_enabled()){
        tcb_t* crt_thread = get_crt_thread();
        crt_thread->ticks++;
        // Current thread has run out of time slice.
        if (crt_thread->ticks >= crt_thread->priority) {
            crt_thread->need_reschedule = true;
        }
    }



}

void init_timer(uint32_t frequency){
    register_interrupt_handler(IRQ0_INT_NUM, &timer_callback);

    // 告诉PIC，每震动多少次发送一次中断，实际上就是1s内发送frequency次中断
    uint32_t divisor = 1193180 / frequency;

    // 与PIC通信，接下来要设置中断频率
    outb(0x43, 0x36);

    // 传输频率分频器的值，分为高8位和低8位
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor>>8) & 0xFF);

    // 进行传输
    outb(0x40, l);
    outb(0x40, h);

}