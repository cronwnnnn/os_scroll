#ifndef DRIVER_HARD_DISK_H
#define DRIVER_HARD_DISK_H
#include "common/types.h"

#define SECTOR_SIZE 512
#define FS_BLOCK_SIZE 4096

void read_hard_disk(char* buffer, uint32_t start, uint32_t length, uint32_t is_slave);
void read_block(char* buffer, uint32_t block_num, uint32_t is_slave);
void init_hard_disk();

#endif