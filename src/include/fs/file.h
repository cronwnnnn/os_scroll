#ifndef FS_FILE_H
#define FS_FILE_H
#include "common/types.h"

struct file_stat {
  uint32_t size;
  uint8_t acl;
};
typedef struct file_stat file_stat_t;


#endif