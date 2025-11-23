#include "mem/gdt.h"

static gdt_entry_t gdt_entries[GDT_ENTRIES];
gdt_ptr_t gdt_ptr;

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
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint32_t)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0); // null segment
// kernel code
  gdt_set_gate(1, 0, 0xFFFFF, DESC_P | DESC_DPL_0 | DESC_S_CODE | DESC_TYPE_CODE, FLAG_G_4K | FLAG_D_32);
  // kernel data
  gdt_set_gate(2, 0, 0xFFFFF, DESC_P | DESC_DPL_0 | DESC_S_DATA | DESC_TYPE_DATA, FLAG_G_4K | FLAG_D_32);
  // video: only 8 pages
  gdt_set_gate(3, 0, 7, DESC_P | DESC_DPL_0 | DESC_S_DATA | DESC_TYPE_DATA, FLAG_G_4K | FLAG_D_32);

  // user code
  gdt_set_gate(4, 0, 0xBFFFF, DESC_P | DESC_DPL_3 | DESC_S_CODE | DESC_TYPE_CODE, FLAG_G_4K | FLAG_D_32);
  // user data
  gdt_set_gate(5, 0, 0xBFFFF, DESC_P | DESC_DPL_3 | DESC_S_DATA | DESC_TYPE_DATA, FLAG_G_4K | FLAG_D_32);
}