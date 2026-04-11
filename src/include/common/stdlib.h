#ifndef COMMON_STDLIB_H
#define COMMON_STDLIB_H

#include "common/types.h"

void memset(void* dest, uint8_t val, uint32_t len);
void memcpy(void* dest, const void* src, uint32_t len);
void sprintf(char* dst, const char* format, ...);
size_t strlen(const char* src);
int32_t strcmp(const char* src1, const char* src2);
int32_t strcpy(char* dest, const char* src);



#endif