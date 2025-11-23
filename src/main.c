#include "monitor/monitor.h"
#include "mem/gdt.h"

int main(){
    monitor_clear();
    monitor_print("Hello, World!\n");
    monitor_print("This is a simple OS kernel with scrolling support.\n");
    init_gdt();
    monitor_print("GDT initialized successfully.\n");
    while(1){}
}