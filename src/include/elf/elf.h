#ifndef ELF_ELF_H
#define ELF_ELF_H

#include "common/types.h"

struct elf32_ehdr {
  uint8_t  e_ident[16];    // Magic number
  uint16_t e_type;         // Object file type
  uint16_t e_machine;      // Architecture
  uint32_t e_version;      // Object file version
  uint32_t e_entry;        // Entry point virtual address
  uint32_t e_phoff;        // Program header table file offset
  uint32_t e_shoff;        // Section header table file offset
  uint32_t e_flags;        // Processor-specific flags
  uint16_t e_ehsize;       // ELF header size in bytes
  uint16_t e_phentsize;    // Program header table entry size
  uint16_t e_phnum;        // Program header table entry count
  uint16_t e_shentsize;    // Section header table entry size
  uint16_t e_shnum;        // Section header table entry count
  uint16_t e_shstrndx;     // Section header string table index
};
typedef struct elf32_ehdr elf32_ehdr_t;

struct elf32_phdr {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
};
typedef struct elf32_phdr elf32_phdr_t;


// ****************************************************************************
int32_t load_elf(char* content, uint32_t* entry_addr);

#endif
