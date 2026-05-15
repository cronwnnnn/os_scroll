// 在 Linux 环境下编写的 mkfs.c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fs/ext2_fs.h"
#include "driver/hard_disk.h"

// 格式化磁盘，只能用一次
int main() {
    FILE *disk = fopen("./file_system.img", "r+b"); // 打开你用 dd 命令创建的 100MB 空文件
    
    if(disk == NULL){
        printf("can't open img\n");
        return 1;
    }

    // 1. 写入 Superblock (占用 Block 0)
    struct superblock sb = {
        .magic = 0x8899AABB,
        .total_blocks = 25600, // 100MB / 4KB
        .inode_bitmap_blk = 1, // 紧跟着超块
        .data_bitmap_blk = 2,
        .inode_table_blk = 3,
        // 假设 Inode 表占用 100 个块，数据块从 103 开始
        .data_blocks_start = 103 
    };
    fseek(disk, 0, SEEK_SET);
    fwrite(&sb, sizeof(sb), 1, disk);

    // 2. 初始化并写入 inode bitmap 和 data bitmap
    uint8_t inode_bitmap[FS_BLOCK_SIZE] = {0};
    uint8_t data_bitmap[FS_BLOCK_SIZE] = {0};

    // 保留 Inode 0 不分给任何人，作为未分配/无效标识
    // 根目录使用第 1 号 inode
    inode_bitmap[0] = 0x03;  // bit 0 set (reserved 0), bit 1 set (for root inode 1)
    
    // 第 0 号数据块依然分给根目录
    data_bitmap[0] = 0x01;   // bit 0 set

    fseek(disk, sb.inode_bitmap_blk * FS_BLOCK_SIZE, SEEK_SET);
    fwrite(&inode_bitmap, FS_BLOCK_SIZE, 1, disk);

    fseek(disk, sb.data_bitmap_blk * FS_BLOCK_SIZE, SEEK_SET);
    fwrite(&data_bitmap, FS_BLOCK_SIZE, 1, disk);

    // 3. 创建根目录的 Inode (我们这里分配 Inode 1)
    struct inode root_inode = {
        .type = FS_DIR, // 目录
        .size = FS_BLOCK_SIZE, // 占用 1 个块来存目录项
        .blocks = {sb.data_blocks_start} // 把第一块数据块分配给根目录
    };
    // 写入 Inode 1 位置 (表首地址 + 1 * sizeof(inode))
    fseek(disk, sb.inode_table_blk * FS_BLOCK_SIZE + sizeof(struct inode), SEEK_SET);
    fwrite(&root_inode, sizeof(root_inode), 1, disk);

    // 清零root块的内容
    // 将整个块刷为 0 以代表目录项的 inode 均为 0 (空闲)
    uint8_t zero_root[FS_BLOCK_SIZE];
    memset(zero_root, 0, sizeof(zero_root));
    fseek(disk, sb.data_blocks_start * FS_BLOCK_SIZE, SEEK_SET);
    fwrite(&zero_root, sizeof(zero_root), 1, disk); 

    // 4. 往根目录的数据块里写入基础目录项 (".", "..")
    struct dirent d1 = {.inode_num = 1, .name = "."};
    struct dirent d2 = {.inode_num = 1, .name = ".."};
    
    fseek(disk, sb.data_blocks_start * FS_BLOCK_SIZE, SEEK_SET);
    fwrite(&d1, sizeof(d1), 1, disk);
    fwrite(&d2, sizeof(d2), 1, disk);

    fclose(disk);
    printf("mkfs finished successfully.\n");
    return 0;
}

