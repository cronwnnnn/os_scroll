#ifndef INTERRUPT_PAGE_H
#define INTERRUPT_PAGE_H

#include "common/types.h"

#define PAGE_SHIFT  12 
#define PAGE_SIZE   (1 << PAGE_SHIFT) //4096
// 页掩码 0xFFFFF000 (用于清零低 12 位)
#define PAGE_MASK   (~(PAGE_SIZE - 1)) 

// 将真实的物理地址转换为页框号 PFN (也就是 >> 12)
#define PHYS_TO_PFN(phys_addr)  ((uint32)(phys_addr) >> PAGE_SHIFT)
// 将页框号 PFN 还原为物理首地址 (也就是 << 12)
#define PFN_TO_PHYS(pfn)        ((uint32)(pfn) << PAGE_SHIFT)

// 将任意地址“向下”对齐到页边界 (例如：0x1005 变成 0x1000)
#define PAGE_ALIGN_DOWN(addr)   ((uint32)(addr) & PAGE_MASK)
// 将任意地址“向上”对齐到下一页边界 (例如：0x1005 变成 0x2000)
#define PAGE_ALIGN_UP(addr)     (((uint32)(addr) + PAGE_SIZE - 1) & PAGE_MASK)


// ********************* virtual memory layout *********************************
// 0xC0000000 ... 0xC0100000 ... 0xC0400000  boot & reserverd                4MB
// 0xC0400000 ... 0xC0800000 page tables, 0xC0701000 page directory          4MB
// 0xC0800000 ... 0xC0900000 kernel load                                     1MB
#define PAGE_DIR_VIRTUAL              0xC0701000
#define PAGE_TABLES_VIRTUAL           0xC0400000
#define KERNEL_LOAD_VIRTUAL_ADDR      0xC0800000
#define KERNEL_LOAD_PHYSICAL_ADDR     0x200000
#define KERNEL_SIZE_MAX               (1024 * 1024)

#define KERNEL_PAGE_DIR_PHY  0x00101000

#define PHYSICAL_MEM_SIZE             (32 * 1024 * 1024)

#define KERNEL_BIN_LOAD_SIZE          (1024 * 1024)

#define COPIED_PAGE_DIR_VADDR         0xFFFFD000
#define COPIED_PAGE_TABLE_VADDR       0xFFFFE000
#define COPIED_PAGE_VADDR             0xFFFFF000


typedef struct page_table_entry {
  uint32_t present    : 1;   // 页面是否在内存中
  uint32_t rw         : 1;   // 是否可写
  uint32_t user       : 1;   // 该页面的访问权限，0表示仅内核态可访问，1表示用户态也可访问
  uint32_t accessed   : 1;   // 是否访问过该页面，用于页面替换算法
  uint32_t dirty      : 1;   // 该页面是否被写入过，若被写入，则在替换时要写回磁盘
  uint32_t unused     : 7;   // 留空
  uint32_t frame      : 20;  
} pte_t;

// 页目录项和页表项的结构相同，页目录项其实就是一个页表项
typedef pte_t pde_t;

typedef struct page_directory{
    uint32_t page_directory_entries;
}page_directory_t;



void init_page();
void map_page(uint32_t vaddr);
page_directory_t clone_crt_page_dir();
void reload_page_dir(page_directory_t *dir);

#endif // INTERRUPT_PAGE_H