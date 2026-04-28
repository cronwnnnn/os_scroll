#include "driver/hard_disk.h"
#include "utils/utils.h"
#include "mem/kheap.h"
#include "common/stdlib.h"

extern void read_disk(char* buffer, uint32_t start_sector, uint32_t sector_num);


static void read_sector(char* buffer, uint32_t sector) {
  read_disk(buffer, sector, 1);
}


void read_hard_disk(char* buffer, uint32_t start, uint32_t length){
    uint32_t end = start + length;

    uint32_t start_sector = start / SECTOR_SIZE;
    uint32_t end_sector = (end + SECTOR_SIZE - 1) / SECTOR_SIZE;

    char* sector_buffer = (char*)kmalloc(SECTOR_SIZE);

    for (uint32_t i = start_sector; i < end_sector; i++) {
        read_sector(sector_buffer, i);

        uint32_t copy_start_addr = max(i * SECTOR_SIZE, start);
        uint32_t copy_end_addr = min((i + 1) * SECTOR_SIZE, end);
        uint32_t copy_size = copy_end_addr - copy_start_addr;
        memcpy(buffer, sector_buffer + copy_start_addr - i * SECTOR_SIZE, copy_size);
        buffer += (copy_size);
    }

    kfree(sector_buffer);


}