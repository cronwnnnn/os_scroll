#include "fs/vfs.h"
#include "fs/simple_fs.h"
#include "common/secure.h"


static fs_t* get_fs(char* path) {
  // This is the only fs we have mount :)
  return get_simple_fs();
}

void init_file_system() {
    init_simple_fs();
}

int32_t stat_file(char* filename, file_stat_t* stat) {
    fs_t* fs = get_fs(filename);
    return fs->stat_file(filename, stat);
}

int32_t list_dir(char* dir) {
    fs_t* fs = get_fs(dir);
    return fs->list_dir(dir);
}

int32_t read_file(char* filename, char* buffer, uint32_t start, uint32_t length) {
    fs_t* fs = get_fs(filename);
    return fs->read_data(filename, buffer, start, length);
}

int32_t write_file(char* filename, char* buffer, uint32_t start, uint32_t length) {
    Panic("write file not complete\n");
    return -1;
}

