#include "driver/hard_disk.h"
#include "utils/utils.h"
#include "mem/kheap.h"
#include "common/stdlib.h"
#include "sync/spinlock.h"
#include "task/scheduler.h"

static spinlock_t disk_lock;

void init_hard_disk() {
    spinlock_init(&disk_lock);
}

extern void read_disk(char* buffer, uint32_t start_sector, uint32_t sector_num, uint32_t is_slave);

// is_slave为1表示从盘，即文件系统
static void read_sector(char* buffer, uint32_t sector, uint32_t is_slave) {
    bool status;
    if(multi_task_is_enabled() == true){
        spin_lock_irqsave(&disk_lock, &status);
    }
    read_disk(buffer, sector, 1, is_slave);
    if(multi_task_is_enabled() == true){
        spin_unlock_irqrestore(&disk_lock, status);
    }
}

// 根据块号，读取整个 4KB 数据块
void read_block(char* buffer, uint32_t block_num, uint32_t is_slave) {
    // 换算成 LBA 扇区号 (1 个 Block = 8 个 Sector)
    uint32_t sector_start = block_num * 8;
    
    for (int i = 0; i < 8; i++) {
        read_sector(buffer + i * 512, sector_start + i, is_slave);
    }
}

// 为任意字节对齐设计的磁盘读取接口
void read_hard_disk(char* buffer, uint32_t start, uint32_t length, uint32_t is_slave){
    uint32_t end = start + length;

    uint32_t start_sector = start / SECTOR_SIZE;
    uint32_t end_sector = (end + SECTOR_SIZE - 1) / SECTOR_SIZE;

    char* sector_buffer = (char*)kmalloc(SECTOR_SIZE);

    for (uint32_t i = start_sector; i < end_sector; i++) {
        read_sector(sector_buffer, i, is_slave);

        uint32_t copy_start_addr = max(i * SECTOR_SIZE, start);
        uint32_t copy_end_addr = min((i + 1) * SECTOR_SIZE, end);
        uint32_t copy_size = copy_end_addr - copy_start_addr;
        memcpy(buffer, sector_buffer + copy_start_addr - i * SECTOR_SIZE, copy_size);
        buffer += (copy_size);
    }

    kfree(sector_buffer);
}