#include "common/secure.h"
#include "monitor/monitor.h"

extern void monitor_printf_args(const char *format, char *arg_ptr);

void Panic(const char* message, ...) {
    monitor_print("KERNEL PANIC: ");
    
    char *arg = (char *)(&message);
    arg += 4;
    monitor_printf_args(message, arg);
    
    monitor_print("\nSystem halted.\n");

    // 停止系统运行
    while (1) {
        __asm__ __volatile__("hlt");
    }
}