#include "monitor/monitor.h"
#include "mem/gdt.h"
#include "interrupt/interrupt.h"
#include "interrupt/time.h"

int main(){
    monitor_clear();  //清空屏幕
    monitor_print("Hello, World!\n");
    monitor_print("This is a simple OS kernel with scrolling support.\n");
    init_gdt();
    monitor_print("GDT initialized successfully.\n");
    monitor_printf("i am a kernel, my address is %d\n", 1111);
    init_idt();
    init_timer(TIMER_FREQUENCY);
    monitor_print("IDT and timer initialized successfully.\n");
    while(1){}
}