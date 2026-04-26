#include "interrupt/interrupt.h"
#include "common/secure.h"
#include "monitor/monitor.h"
#include "mem/gdt.h"
#include "common/stdlib.h"
#include "common/types.h"
#include "common/io.h"

static void set_idt_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t attrs);
static void init_pic();
void isr_handler(isr_params_t regs);
void enable_interrupt();
void disable_interrupt();
bool is_in_irq_context();
extern void reload_idt(uint32_t);
extern void syscall_entry();


// entries 是每个表项的值，cpu根据信号去找响应entry并访问地址中的汇编处理函数
static idt_entry_t idt_entries[256];
// 这个存的是具体的每个中断的处理函数，上面那个是cpu访问的，还需要经过汇编函数与统一的处理函数isr_handler进行转换
static isr_t interrupt_handlers[256];
idt_ptr_t idt_ptr;

// 防止中断时被调度？
bool in_irq_context = false;

void init_idt(){
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base = (uint32_t)&idt_entries;

    memset(idt_entries, 0, sizeof(idt_entry_t) * 256);
    memset(interrupt_handlers, 0, sizeof(isr_t) * 256);
    // 初始化idt_entries

    // exceptions
    set_idt_gate(0, (uint32_t)isr0 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(1, (uint32_t)isr1 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(2, (uint32_t)isr2 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(3, (uint32_t)isr3 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(4, (uint32_t)isr4 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(5, (uint32_t)isr5 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(6, (uint32_t)isr6 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(7, (uint32_t)isr7 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(8, (uint32_t)isr8 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(9, (uint32_t)isr9 , SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(10, (uint32_t)isr10, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(11, (uint32_t)isr11, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(12, (uint32_t)isr12, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(13, (uint32_t)isr13, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(14, (uint32_t)isr14, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(15, (uint32_t)isr15, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(16, (uint32_t)isr16, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(17, (uint32_t)isr17, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(18, (uint32_t)isr18, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(19, (uint32_t)isr19, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(20, (uint32_t)isr20, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(21, (uint32_t)isr21, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(22, (uint32_t)isr22, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(23, (uint32_t)isr23, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(24, (uint32_t)isr24, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(25, (uint32_t)isr25, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(26, (uint32_t)isr26, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(27, (uint32_t)isr27, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(28, (uint32_t)isr28, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(29, (uint32_t)isr29, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(30, (uint32_t)isr30, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(31, (uint32_t)isr31, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);

    // interrupts
    set_idt_gate(32, (uint32_t)isr32, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(33, (uint32_t)isr33, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(34, (uint32_t)isr34, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(35, (uint32_t)isr35, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(36, (uint32_t)isr36, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(37, (uint32_t)isr37, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(38, (uint32_t)isr38, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(39, (uint32_t)isr39, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(40, (uint32_t)isr40, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(41, (uint32_t)isr41, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(42, (uint32_t)isr42, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(43, (uint32_t)isr43, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(44, (uint32_t)isr44, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(45, (uint32_t)isr45, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(46, (uint32_t)isr46, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);
    set_idt_gate(47, (uint32_t)isr47, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);

    // soft int
    // 在int0x80之后跳转至syscall_entry函数处理，而非isr这种，因为要对某些寄存器特殊处理
    set_idt_gate(SYSCALL_INT_NUM, (uint32_t)syscall_entry, SELECTOR_K_CODE, IDT_GATE_ATTR_DPL3);

    // refresh idt,加载idt
    reload_idt((uint32_t)&idt_ptr);

    init_pic();

    __asm__ __volatile__("sti");
}

static void set_idt_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t attrs) {
  idt_entries[num].base_low = base & 0xFFFF;
  idt_entries[num].base_high = (base >> 16) & 0xFFFF;
  idt_entries[num].sel = sel;
  idt_entries[num].always0 = 0;
  idt_entries[num].flags = attrs;
}


void isr_handler(isr_params_t regs){
    int32_t int_num = regs.int_num;

    // 对于硬中断，需要在处理完中断后发送EOI信号给PIC，PIC分为slave和master两级
    // 如果是slave发出的中断，需要同时给slave和master发送EOI信号
    if (int_num >= 32 && int_num < SYSCALL_INT_NUM) {
        if (int_num >= 40) {
        // send reset signal to slave
        outb(0xA0, 0x20);
        }
        // send reset signal to master
        outb(0x20, 0x20);
        in_irq_context = true;
    } else {
        // Not a hardware interrupt, enable interrupt as quickly as possible.
        enable_interrupt();
    }
    if (interrupt_handlers[int_num] != 0) {
        interrupt_handlers[int_num](regs);
    }
    else{
        monitor_printf("Unknown interrupt: %d", int_num);
        Panic("unknown interrupt");
    }
    if (in_irq_context) {
        in_irq_context = false;
        enable_interrupt();
    }

}

void register_interrupt_handler(uint32_t int_num, isr_t handler){
    interrupt_handlers[int_num] = handler;
}

void enable_interrupt() {
// 允许接收中断信号
  asm volatile ("sti");
}

void disable_interrupt() {
// 禁止接收中断信号，防止在处理中断时被调度
  asm volatile ("cli");
}

bool is_in_irq_context() {
    return in_irq_context;
}

void init_pic() {
  // master
  outb(0x20, 0x11);
  outb(0x21, 0x20);
  outb(0x21, 0x04);
  outb(0x21, 0x01);
  // slave
  outb(0xA0, 0x11);
  outb(0xA1, 0x28);
  outb(0xA1, 0x02);
  outb(0xA1, 0x01);
  // unmask all irqs
  outb(0x21, 0x0);
  outb(0xA1, 0x0);
}

