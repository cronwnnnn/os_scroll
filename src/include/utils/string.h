#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include "common/types.h"

// 用于复制和释放一个存 char** 的堆内存
char** copy_str_array(uint32_t num, char* str_array[]);
void destroy_str_array(uint32_t num, char* str_array[]);

#endif
