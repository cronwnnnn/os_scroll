#ifndef MEM_GDT_H
#define MEM_GDT_H

#include "common/types.h"

// ****************************************************************************
#define FLAG_G_4K  (1 << 3)
#define FLAG_D_32  (1 << 2)

#define DESC_P     (1 << 7)

#define DESC_DPL_0   (0 << 5)
#define DESC_DPL_1   (1 << 5)
#define DESC_DPL_2   (2 << 5)
#define DESC_DPL_3   (3 << 5)

#define DESC_S_CODE   (1 << 4)
#define DESC_S_DATA   (1 << 4)
#define DESC_S_SYS    (0 << 4)

#define DESC_TYPE_CODE  0x8   // r/x non-conforming code segment
#define DESC_TYPE_DATA  0x2   // r/w data segment
#define DESC_TYPE_TSS   0x9

// ****************************************************************************

#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TI_GDT 0
#define TI_LDT 1

#define SELECTOR_K_CODE   ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA   ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK  SELECTOR_K_DATA
#define SELECTOR_K_GS     ((3 << 3) + (TI_GDT << 2) + RPL0)  // video segment
#define SELECTOR_U_CODE   ((4 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_DATA   ((5 << 3) + (TI_GDT << 2) + RPL3)

#define GDT_ENTRIES 6

// ****************************************************************************


struct gdt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));
typedef struct gdt_ptr gdt_ptr_t;


struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t attributes;
    uint8_t base_high;
} __attribute__((packed)); //取消编译器添加的对齐填充，十分重要
typedef struct gdt_entry gdt_entry_t;


#endif