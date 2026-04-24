#include "interrupt/time.h"
#include "interrupt/interrupt.h"
#include "common/io.h"
#include "monitor/monitor.h"
#include "task/thread.h"
#include "task/scheduler.h"
#include "common/secure.h"






static void protect_fault_handler(isr_params_t regs){
    // 每当时钟中断发生时，tick加1，并且每秒打印一次
    /*
    if(tick % TIMER_FREQUENCY == 0){
        monitor_printf("seconds: %d \n", tick / TIMER_FREQUENCY);
    }*/

    Panic("protect fault!!!!!");
}

void init_protect_fault(){
    register_interrupt_handler(PROTECT_FAULT_NUM, &protect_fault_handler);

}