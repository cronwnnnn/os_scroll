#include "common/types.h"
#include "interrupt/interrupt.h"   

extern void read_disk(char* buffer, uint32_t start_sector, uint32_t sector_num);

// 硬盘中断简单的空处理函数
static void hd_interrupt_handler(isr_params_t regs) {
}

void init_hard_disk(){
    // 监听 46号 中断（即 IRQ14, Primary ATA 通道操作完成中断）
    register_interrupt_handler(46, &hd_interrupt_handler);
}