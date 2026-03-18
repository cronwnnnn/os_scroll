#include "common/secure.h"
#include "monitor/monitor.h"

void Panic(const char* message) {
    monitor_print("KERNEL PANIC: ");
    monitor_print(message);
    monitor_print("\nSystem halted.\n");

    // 停止系统运行
    while (1) {
        __asm__ __volatile__("hlt");
    }
}