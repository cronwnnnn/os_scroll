
#include "common/types.h"

void memset(void* dest, uint8_t val, size_t len){
    for(size_t i = 0; i < len; i++)
        *((uint8_t*)dest + i) = val;
}

void memcpy(void* dest, const void* src, size_t len){
    for(size_t i = 0; i < len; i++)
        *((uint8_t*)dest + i) = *((uint8_t*)src + i);
}