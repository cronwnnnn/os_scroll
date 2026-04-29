#include "elf/elf.h"
#include "common/stdlib.h"

int32_t load_elf(char* content, uint32_t* entry_addr) {
    elf32_ehdr_t* elf_header = (elf32_ehdr_t*)content;

    // Verify magic number
    if (elf_header->e_ident[0] != 0x7f) {
        return -1;
    }
    if (elf_header->e_ident[1] != 'E') {
        return -1;
    }
    if (elf_header->e_ident[2] != 'L') {
        return -1;
    }
    if (elf_header->e_ident[3] != 'F') {
        return -1;
    }

    // Load each section.
    elf32_phdr_t* program_header = (elf32_phdr_t*)(content + elf_header->e_phoff);
    for (uint32_t i = 0; i < elf_header->e_phnum; i++) {
        if(program_header->p_type == PT_LOAD && program_header->p_memsz > 0){
            if (program_header->p_filesz > 0) {
                // 如果文件里有数据，把文件数据拷贝过去
                memcpy((void*)program_header->p_vaddr, content + program_header->p_offset, program_header->p_filesz);
            }

            // 处理bss段
            if (program_header->p_memsz > program_header->p_filesz) {
                memset((void*)(program_header->p_vaddr + program_header->p_filesz), 
                       0, 
                       program_header->p_memsz - program_header->p_filesz);
            }

        }
            program_header = (elf32_phdr_t*)((uint32_t)program_header + elf_header->e_phentsize);
    }

    *entry_addr = elf_header->e_entry;
    return 0;
}
