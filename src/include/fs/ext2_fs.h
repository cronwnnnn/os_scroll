#ifndef FS_EXT2_FS_H
#define FS_EXT2_FS_H

#include "common/types.h"

#define FS_FILE 1
#define FS_DIR 2

struct file_system; 
typedef struct file_system fs_t;

// 超级块
struct superblock {
    uint32_t magic;         // 用于识别文件系统
    uint32_t total_blocks;
    uint32_t inode_bitmap_blk;
    uint32_t data_bitmap_blk;
    uint32_t inode_table_blk;
    uint32_t data_blocks_start;
} __attribute__((packed));
typedef struct superblock superblock_t; 

struct inode {
    uint16_t type;          // 文件或者目录
    uint32_t size;          // 字节数
    uint32_t blocks[14];    // 指向数据块的指针,对于12是一级间接指针，4mb，13是2级间接指针，4gb
} __attribute__((packed));
typedef struct inode inode_t; 

struct dirent {
    uint32_t inode_num;     // Inode 编号
    char name[28];          // 文件名
} __attribute__((packed));
typedef struct dirent dirent_t; 

void init_ext2_fs();
fs_t* get_ext2_fs();

#endif