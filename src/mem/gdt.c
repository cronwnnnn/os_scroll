#include "mem/gdt.h"

static gdt_entry_t gdt_entries[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr_1;

extern void load_gdt(gdt_ptr_t *);

void refresh_gdt(){
    load_gdt(&gdt_ptr_1);
}

static void gdt_set_gate(
    int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
  gdt_entries[num].limit_low = (limit & 0xFFFF);
  gdt_entries[num].base_low = (base & 0xFFFF);
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].access = access;
  gdt_entries[num].attributes = (limit >> 16) & 0x0F;
  gdt_entries[num].attributes |= ((flags << 4) & 0xF0);
  gdt_entries[num].base_high = (base >> 24) & 0xFF;
}


void init_gdt() {
    gdt_ptr_1.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr_1.base = (uint32_t)&gdt_entries;

    // 不做地址计算，因此基地址全部是0，具体的地址计算在虚拟地址部分（vedio也一样，ds:offset中的偏移量才是真正的地址）
    // 注意limit的单位是4kb，粒度位为1时，limit的值是4kb的倍数，因此0xFFFFF表示4gb
    gdt_set_gate(0, 0, 0, 0, 0); // null segment
    // kernel code
    gdt_set_gate(1, 0, 0xFFFFF, DESC_P | DESC_DPL_0 | DESC_S_CODE | DESC_TYPE_CODE, FLAG_G_4K | FLAG_D_32);
    // kernel data
    gdt_set_gate(2, 0, 0xFFFFF, DESC_P | DESC_DPL_0 | DESC_S_DATA | DESC_TYPE_DATA, FLAG_G_4K | FLAG_D_32);
    // video segment: identical to kernel data (flat mode, base=0, limit=4GB) 
    // This allows accessing video memory through virtual address like 0xC00B8000
    gdt_set_gate(3, 0, 0xFFFFF, DESC_P | DESC_DPL_0 | DESC_S_DATA | DESC_TYPE_DATA, FLAG_G_4K | FLAG_D_32);

     // user code
    gdt_set_gate(4, 0, 0xBFFFF, DESC_P | DESC_DPL_3 | DESC_S_CODE | DESC_TYPE_CODE, FLAG_G_4K | FLAG_D_32);
    // user data
    gdt_set_gate(5, 0, 0xBFFFF, DESC_P | DESC_DPL_3 | DESC_S_DATA | DESC_TYPE_DATA, FLAG_G_4K | FLAG_D_32);

    refresh_gdt();
}