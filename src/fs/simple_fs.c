#include "fs/simple_fs.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "driver/hard_disk.h"
#include "common/stdlib.h"
#include "common/secure.h"
#include "mem/kheap.h"
#include "utils/utils.h"
#include "monitor/monitor.h"

fs_t simple_file_system;
uint32_t file_nums;
// 每个文件的meta数据
simple_file_meta_t* file_metas;


fs_t* get_simple_fs(){
    return &simple_file_system;
}

static int32_t simple_fs_stat_file(char* filename, file_stat_t* stat){
    for(int32_t i = 0; i < file_nums; i++){
        simple_file_meta_t* ft = file_metas + i;
        if(strcmp(ft->filename, filename) == 0){
            stat->size = ft->size;
            return 0;
        }
    }
    return -1;
}

static int32_t simple_fs_list_dir(char* dir){
    uint32_t* size_length = (uint32_t*)kmalloc(file_nums * sizeof(uint32_t));
    uint32_t max_length = 0;
    for (uint32_t i = 0; i < file_nums; i++) {
        simple_file_meta_t* meta = file_metas + i;
        uint32_t size = meta->size;
        uint32_t length = 1;
        while ((size /= 10) > 0) {
        length++;
        }
        size_length[i] = length;
        max_length = max(max_length, length);
    }

    for (uint32_t i = 0; i < file_nums; i++) {
        simple_file_meta_t* meta = file_metas + i;
        monitor_printf("root  ");
        for (uint32_t j = 0; j < max_length - size_length[i]; j++) {
        monitor_printf(" ");
        }
        monitor_printf("%d  %s\n", meta->size, meta->filename);
    }
    kfree(size_length);
    return 0;

}

static int32_t simple_fs_read_data(char* filename, char* buffer, uint32_t start, uint32_t length){
    simple_file_meta_t* meta = NULL;
    for(int32_t i = 0; i < file_nums; i++){
        simple_file_meta_t* ft = file_metas + i;
        if(strcmp(ft->filename, filename) == 0){
            meta = ft;
        }
    }
    if(meta == NULL){
        return -1;
    }
    // offset 是文件系统的起始地址 + 该文件在文件系统的offset + 该文件的哪一处
    uint32_t offset = simple_file_system.partition.offset + meta->offset + start;
    uint32_t size = meta->size;
    if (length > size) {
        length = size;
    }

    read_hard_disk(buffer, offset, length);
    return length;
}

static int32_t simple_fs_write_data(char* filename, char* buffer, uint32_t start, uint32_t length) {
    return -1;
}


void init_simple_fs(){
    simple_file_system.type = SIMPLE;
    // 存放的文件在disk中的起始偏移量
    simple_file_system.partition.offset = 2057 * SECTOR_SIZE;
    
    simple_file_system.stat_file = simple_fs_stat_file;
    simple_file_system.read_data = simple_fs_read_data;
    simple_file_system.write_data = simple_fs_write_data;
    simple_file_system.list_dir = simple_fs_list_dir;

    // 获取每个文件的参数
    // 读有多少个file
    read_hard_disk((char*)&file_nums, 0 + simple_file_system.partition.offset, sizeof(uint32_t));
    Assert(file_nums > 0);
    if (file_nums > 1024) {  // 或者你允许的最大文件数量
        Panic("too many files, use dir!!!");
    }

    uint32_t meta_size = file_nums * sizeof(simple_file_meta_t);
    file_metas = (simple_file_meta_t*)kmalloc(meta_size);
    read_hard_disk((char*)file_metas, 4 + simple_file_system.partition.offset, meta_size);
    for (int i = 0; i < file_nums; i++) {
        simple_file_meta_t* meta = file_metas + i;
        monitor_printf(" - %s, offset = %d, size = %d\n", meta->filename, meta->offset, meta->size);
    }

}