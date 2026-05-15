#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fs/ext2_fs.h"
#include "driver/hard_disk.h"

uint32_t* find_free_blocks(FILE* disk_fp, uint32_t blocks_needed) {
    // 读取 Superblock
    struct superblock sb;
    fseek(disk_fp, 0, SEEK_SET);
    fread(&sb, sizeof(sb), 1, disk_fp);

    // 读取 Data Bitmap
    uint8_t data_bitmap[FS_BLOCK_SIZE];
    fseek(disk_fp, sb.data_bitmap_blk * FS_BLOCK_SIZE, SEEK_SET);
    fread(data_bitmap, FS_BLOCK_SIZE, 1, disk_fp);

    uint32_t* allocated = malloc(blocks_needed * sizeof(uint32_t));
    uint32_t found = 0;

    for (uint32_t i = 0; i < FS_BLOCK_SIZE * 8 && found < blocks_needed; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if ((data_bitmap[byte_idx] & (1 << bit_idx)) == 0) {
            // 找到空闲块，将位图置 1
            data_bitmap[byte_idx] |= (1 << bit_idx);
            allocated[found++] = sb.data_blocks_start + i;
        }
    }

    if (found < blocks_needed) {
        printf("Not enough free blocks!\n");
        free(allocated);
        return NULL;
    }

    // 由于上面对data_bitmap的更改只是在内存中没有写回，因此即使不够也不会真的将位图置为1
    // 写回更新后的 Data Bitmap
    fseek(disk_fp, sb.data_bitmap_blk * FS_BLOCK_SIZE, SEEK_SET);
    fwrite(data_bitmap, FS_BLOCK_SIZE, 1, disk_fp);

    return allocated;
}

uint32_t get_free_inode_num(FILE* disk_fp) {
    struct superblock sb;
    fseek(disk_fp, 0, SEEK_SET);
    fread(&sb, sizeof(sb), 1, disk_fp);

    uint8_t inode_bitmap[FS_BLOCK_SIZE];
    fseek(disk_fp, sb.inode_bitmap_blk * FS_BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, FS_BLOCK_SIZE, 1, disk_fp);

    for (uint32_t i = 0; i < FS_BLOCK_SIZE * 8; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if ((inode_bitmap[byte_idx] & (1 << bit_idx)) == 0) {
            inode_bitmap[byte_idx] |= (1 << bit_idx);
            // 写回更新后的 Inode Bitmap
            fseek(disk_fp, sb.inode_bitmap_blk * FS_BLOCK_SIZE, SEEK_SET);
            fwrite(inode_bitmap, FS_BLOCK_SIZE, 1, disk_fp);
            return i;
        }
    }
    return -1;
}

void write_inode_to_disk(FILE* disk_fp, uint32_t inode_num, struct inode* new_inode) {
    struct superblock sb;
    fseek(disk_fp, 0, SEEK_SET);
    fread(&sb, sizeof(sb), 1, disk_fp);

    uint32_t inode_offset = sb.inode_table_blk * FS_BLOCK_SIZE + inode_num * sizeof(struct inode);
    fseek(disk_fp, inode_offset, SEEK_SET);
    fwrite(new_inode, sizeof(struct inode), 1, disk_fp);
}

void add_dirent_to_root(FILE* disk_fp, const char* name, uint32_t inode_num) {
    struct superblock sb;
    fseek(disk_fp, 0, SEEK_SET);
    fread(&sb, sizeof(sb), 1, disk_fp);

    // 读取根目录的 inode (Root Is Inode 1)
    struct inode root_inode;
    uint32_t root_inode_offset = sb.inode_table_blk * FS_BLOCK_SIZE + 1 * sizeof(struct inode);
    fseek(disk_fp, root_inode_offset, SEEK_SET);
    fread(&root_inode, sizeof(struct inode), 1, disk_fp);

    // 假设根目录数据在它的第一个块中
    uint32_t root_data_blk = root_inode.blocks[0];
    
    // 遍历该块中的所有目录项，找到一个空项来写入
    struct dirent entry;
    uint32_t entries_per_block = FS_BLOCK_SIZE / sizeof(struct dirent);
    
    fseek(disk_fp, root_data_blk * FS_BLOCK_SIZE, SEEK_SET);
    for (uint32_t i = 0; i < entries_per_block; i++) {
        fread(&entry, sizeof(struct dirent), 1, disk_fp);
        // inode_num 为 0 代表未分配/空闲项
        if (entry.inode_num == 0) {
            entry.inode_num = inode_num;
            strncpy(entry.name, name, sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            
            fseek(disk_fp, root_data_blk * FS_BLOCK_SIZE + i * sizeof(struct dirent), SEEK_SET);
            fwrite(&entry, sizeof(struct dirent), 1, disk_fp);
            return;
        }
    }
    printf("Root directory is full!\n");
}

void inject_file(FILE *disk_fp, const char *external_path, const char *internal_name) {
    // 1. 在宿主机读取 ELF 文件的原始字节
    FILE *ext_fp = fopen(external_path, "rb");
    if (!ext_fp) {
        printf("Failed to open external file %s\n", external_path);
        return;
    }
    
    fseek(ext_fp, 0, SEEK_END);
    uint32_t file_size = ftell(ext_fp);
    fseek(ext_fp, 0, SEEK_SET);
    uint8_t *buffer = malloc(file_size);
    fread(buffer, 1, file_size, ext_fp);
    fclose(ext_fp);

    // 2. 在磁盘镜像中寻找足够的空闲块 (通过读取镜像的 Data Bitmap)
    uint32_t blocks_needed = (file_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    if (blocks_needed > 12) {
        printf("File too large for direct blocks only!\n");
        free(buffer);
        return;
    }
    
    uint32_t *allocated_blocks = find_free_blocks(disk_fp, blocks_needed);
    if (!allocated_blocks) {
        free(buffer);
        return;
    }

    // 3. 将字节流写入镜像对应的偏移位置
    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint32_t disk_offset = allocated_blocks[i] * FS_BLOCK_SIZE;
        fseek(disk_fp, disk_offset, SEEK_SET);
        // 处理最后一块不足一个 FS_BLOCK_SIZE 的情况
        uint32_t write_size = (i == blocks_needed - 1 && file_size % FS_BLOCK_SIZE != 0) 
                              ? (file_size % FS_BLOCK_SIZE) : FS_BLOCK_SIZE;
        fwrite(buffer + i * FS_BLOCK_SIZE, 1, write_size, disk_fp);
    }

    // 4. 创建 Inode 并写入 Inode Table 区域
    struct inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.type = FS_FILE;
    new_inode.size = file_size;
    memcpy(new_inode.blocks, allocated_blocks, blocks_needed * sizeof(uint32_t));
    
    uint32_t new_inode_num = get_free_inode_num(disk_fp);
    write_inode_to_disk(disk_fp, new_inode_num, &new_inode);

    // 5. 更新目录项...
    add_dirent_to_root(disk_fp, internal_name, new_inode_num);
    
    free(buffer);
    free(allocated_blocks);
}

void print_files_in_root(FILE* disk_img){
    struct superblock sb;
    fseek(disk_img, 0, SEEK_SET);
    fread(&sb, sizeof(sb), 1, disk_img);

    // 找到根目录的数据块 (Root Is Inode 1)
    struct inode root_inode;
    fseek(disk_img, sb.inode_table_blk * FS_BLOCK_SIZE + 1 * sizeof(struct inode), SEEK_SET);
    fread(&root_inode, sizeof(struct inode), 1, disk_img);
    
    char* block = malloc(FS_BLOCK_SIZE);
    fseek(disk_img, root_inode.blocks[0] * FS_BLOCK_SIZE, SEEK_SET); // 修正为 fseek
    fread(block, FS_BLOCK_SIZE, 1, disk_img);                       // 修正了 fread 参数

    dirent_t* entry = (dirent_t*) block;
    int count = 0;
    
    printf("Files injected in root directory:\n");
    for(int i = 0; i < (FS_BLOCK_SIZE/sizeof(dirent_t)); i++){
        if (entry[i].inode_num == 0) {
            continue;
        }
        printf("- %s (inode: %d)\n", entry[i].name, entry[i].inode_num);
        count++;
    }
    printf("\nTotal files: %d\n", count);
    
    free(block);
}