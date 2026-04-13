#include "common/secure.h"
#include "monitor/monitor.h"
#include "interrupt/interrupt.h"


void Panic(const char* message, ...) {
    // 1. 立刻立刻关中断，防止被时钟中断切走
    disable_interrupt();

    monitor_print_panic("KERNEL PANIC: ");
    
    char *arg = (char *)(&message);
    arg += 4;
    monitor_printf_args_nolock(message, arg);
    
    monitor_print_panic("\nSystem halted.\n");

    // 停止系统运行
    while (1) {
        __asm__ __volatile__("hlt");
    }
}


void panic_spin(char* filename, int line, const char* func, const char* condition) {
    // 1. 案发现场第一步：立刻关中断！防止别的线程进来破坏现场
    disable_interrupt();

    // 2. 打印死亡宣告
    monitor_printf_panic("\n\n!!!!! KERNEL PANIC !!!!!\n");
    monitor_printf_panic("Assertion failed: %s\n", condition);
    monitor_printf_panic("File: %s\n", filename);
    monitor_printf_panic("Line: %d\n", line);
    monitor_printf_panic("Function: %s\n", func);
    
    // 3. 彻底停机死循环
    while (1) {
        // 让 CPU 休眠，降低功耗
        asm volatile("hlt"); 
    }
}