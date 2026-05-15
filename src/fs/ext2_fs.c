#include "fs/ext2_fs.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "driver/hard_disk.h"
#include "common/stdlib.h"
#include "common/secure.h"
#include "mem/kheap.h"
#include "utils/utils.h"
#include "monitor/monitor.h"

fs_t ext2_file_system;
superblock_t current_superblock;
uint32_t root_dir_inode = 1; // 根目录默认为1
uint32_t current_dir_inode = 1; // 默认为根目录

fs_t* get_ext2_fs(){
    return &ext2_file_system;
}


// 根据 Inode 编号读取 Inode 结构体
static void ext2_read_inode(uint32_t inode_num, inode_t* out_inode) {
    uint32_t offset = current_superblock.inode_table_blk * FS_BLOCK_SIZE + inode_num * sizeof(inode_t);
    read_hard_disk((char*)out_inode, offset, sizeof(inode_t), 1); // 1 = 文件系统
}

//  在目录中查找文件并返回 Inode 编号, -1错误，0正常
static int32_t ext2_find_file_in_dir(uint32_t dir_inode_num, char* filename, uint32_t* out_inode_num) {
    inode_t dir_inode;
    ext2_read_inode(dir_inode_num, &dir_inode);
    
    if (dir_inode.type != FS_DIR) return -1; // 2 = 目录
    
    char* block_buf = (char*)kmalloc(4096);
    for (int i = 0; i < 12; i++) { // 暂时只遍历前12个直接块
        if (dir_inode.blocks[i] == 0) continue;
        read_block(block_buf, dir_inode.blocks[i], 1);
        
        dirent_t* entries = (dirent_t*)block_buf;
        uint32_t entry_count = 4096 / sizeof(dirent_t);
        for (uint32_t j = 0; j < entry_count; j++) {
            if (entries[j].inode_num != 0 && strcmp(entries[j].name, filename) == 0) {
                *out_inode_num = entries[j].inode_num;
                kfree(block_buf);
                return 0;
            }
        }
    }
    kfree(block_buf);
    return -1;
}

static int32_t ext2_fs_stat_file(char* filename, file_stat_t* stat){
    uint32_t inode_num;
    if (ext2_find_file_in_dir(current_dir_inode, filename, &inode_num) == 0) {
        inode_t file_inode;
        ext2_read_inode(inode_num, &file_inode);
        stat->size = file_inode.size;
        return 0;
    }
    return -1;
}

static int32_t ext2_fs_list_dir(char* dir){
    inode_t dir_inode;
    ext2_read_inode(current_dir_inode, &dir_inode);
    
    char* block_buf = (char*)kmalloc(4096);
    monitor_printf("Name\t\tSize\n");
    for (int i = 0; i < 12; i++) {
        if (dir_inode.blocks[i] == 0) continue;
        read_block(block_buf, dir_inode.blocks[i], 1);
        
        dirent_t* entries = (dirent_t*)block_buf;
        uint32_t entry_count = 4096 / sizeof(dirent_t);
        for (uint32_t j = 0; j < entry_count; j++) {
            if (entries[j].inode_num != 0) {
                inode_t file_inode;
                ext2_read_inode(entries[j].inode_num, &file_inode);
                monitor_printf("%s\t\t%d\n", entries[j].name, file_inode.size);
            }
        }
    }
    kfree(block_buf);
    return 0;
}

// 返回读的总长度，用于确定正常读完了
static int32_t ext2_fs_read_data(char* filename, char* buffer, uint32_t start, uint32_t length){
    uint32_t inode_num;
    if (ext2_find_file_in_dir(current_dir_inode, filename, &inode_num) != 0) {
        return -1; // 找不到该文件
    }
    
    inode_t file_inode;
    ext2_read_inode(inode_num, &file_inode);
    
    // 判断文件长度是否正确
    if (start >= file_inode.size) return 0;
    if (start + length > file_inode.size) length = file_inode.size - start;
    
    // bytes_read为当前已经读了多少
    uint32_t bytes_read = 0;
    char* block_buf = (char*)kmalloc(4096);
    
    uint32_t start_blk_idx = start / 4096;
    uint32_t offset_in_blk = start % 4096;
    
    // 以块为单位开始读取文件内容
    while (bytes_read < length && start_blk_idx < 12) {
        uint32_t blk = file_inode.blocks[start_blk_idx];
        if (blk == 0) break;
        
        read_block(block_buf, blk, 1);
        
        // 这个块中要读的长度，只用于第一次读时，后面设为0
        uint32_t to_read = 4096 - offset_in_blk;
        if (to_read > length - bytes_read) to_read = length - bytes_read;
        
        memcpy(buffer + bytes_read, block_buf + offset_in_blk, to_read);
        
        bytes_read += to_read;
        offset_in_blk = 0;
        start_blk_idx++;
    }
    
    kfree(block_buf);
    return bytes_read;
}

static int32_t ext2_fs_write_data(char* filename, char* buffer, uint32_t start, uint32_t length) {
    // 写入文件系统数据块较为复杂，涉及请求空闲块和操作Data Bitmap
    return -1;
}

void init_ext2_fs(){
    ext2_file_system.type = EXT2;
    // 假设新文件系统挂载的偏移在扇区层面处理或为0
    ext2_file_system.partition.offset = 0;
    
    ext2_file_system.stat_file = ext2_fs_stat_file;
    ext2_file_system.read_data = ext2_fs_read_data;
    ext2_file_system.write_data = ext2_fs_write_data;
    ext2_file_system.list_dir = ext2_fs_list_dir;

    monitor_printf("Initializing EXT2/Simple FS...\n");

    // 1. 读取超级块，加入 is_slave = 1 的参数
    read_hard_disk((char*)&current_superblock, 0, sizeof(superblock_t), 1);
    
    if(current_superblock.magic == 0x8899AABB) {
        monitor_printf("Found SimpleFS Superblock, Magic: %x\n", current_superblock.magic);
    } else {
        monitor_printf("Superblock magic invalid!\n");
    }

}