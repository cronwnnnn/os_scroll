#include "common/io.h"
void outb(uint16_t port, uint8_t val){
    asm volatile ( "outb %[val1], %[port1]" : :[val1] "a"(val), [port1] "dN"(port));
}


uint8_t inb(uint16_t port){
    uint8_t ret;
    asm volatile ( "inb %[port1], %[ret1]" : [ret1] "=a"(ret) : [port1] "dN"(port));
    return ret;
}


uint16_t inw(uint16_t port){
    uint16_t ret;
    asm volatile ( "inw %[port1], %[ret1]" : [ret1] "=a"(ret) : [port1] "dN"(port));
    return ret;
}