#ifndef DRIVER_HARD_DISK_H
#define DRIVER_HARD_DISK_H
#include "common/types.h"

#define SECTOR_SIZE 512

void read_hard_disk(char* buffer, uint32_t start, uint32_t length);

#endif