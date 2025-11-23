#include "monitor/monitor.h"

int main(){
    monitor_clear();
    monitor_print("Hello, World!\n");
    monitor_print("This is a simple OS kernel with scrolling support.\n");
    while(1){}
}