#ifndef INTERRUPT_INTERRUPT_H
#define INTERRUPT_INTERRUPT_H

#include "common/types.h"

#define IDT_GATE_P     1
#define IDT_GATE_DPL0  0
#define IDT_GATE_DPL3  3
#define IDT_GATE_32_TYPE  0xE

#define IDT_GATE_ATTR_DPL0 \
  ((IDT_GATE_P << 7) + (IDT_GATE_DPL0 << 5) + IDT_GATE_32_TYPE)

#define IDT_GATE_ATTR_DPL3 \
  ((IDT_GATE_P << 7) + (IDT_GATE_DPL3 << 5) + IDT_GATE_32_TYPE)


// ******************************** exceptions ****************************************
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

// ******************************** interrupts ****************************************
#define IRQ0_INT_NUM 32
#define IRQ1_INT_NUM 33
#define IRQ2_INT_NUM 34
#define IRQ3_INT_NUM 35
#define IRQ4_INT_NUM 36
#define IRQ5_INT_NUM 37
#define IRQ6_INT_NUM 38
#define IRQ7_INT_NUM 39
#define IRQ8_INT_NUM 40
#define IRQ9_INT_NUM 41
#define IRQ10_INT_NUM 42
#define IRQ11_INT_NUM 43
#define IRQ12_INT_NUM 44
#define IRQ13_INT_NUM 45
#define IRQ14_INT_NUM 46
#define IRQ15_INT_NUM 47

#define SYSCALL_INT_NUM 0x80

extern void isr32();
extern void isr33();
extern void isr34();
extern void isr35();
extern void isr36();
extern void isr37();
extern void isr38();
extern void isr39();
extern void isr40();
extern void isr41();
extern void isr42();
extern void isr43();
extern void isr44();
extern void isr45();
extern void isr46();
extern void isr47();





struct idt_entry_struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));
typedef struct idt_entry_struct idt_entry_t;

struct idt_ptr_struct {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));
typedef struct idt_ptr_struct idt_ptr_t;



typedef struct isr_params{
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_num, error_code;
    uint32_t eip, cs, eflags, user_esp, user_ss;
}isr_params_t;

typedef void (*isr_t)(isr_params_t);

void isr_handler(isr_params_t regs);
void register_interrupt_handler(uint32_t int_num, isr_t handler);
void init_idt();
void disable_interrupt();
void enable_interrupt();

#endif