#ifndef FS_SIMPLE_FS_H
#define FS_SIMPLE_FS_H
#include "common/types.h"

struct file_system; 
typedef struct file_system fs_t;

typedef struct simple_file_meta{
    char filename[64];
    uint32_t size;
    uint32_t offset;
}simple_file_meta_t;

void init_simple_fs();
fs_t* get_simple_fs();

#endif