#ifndef UTILS_BIT_MAP_H
#define UTILS_BIT_MAP_H

#include "common/types.h"

typedef struct {
    uint32_t* bitarray;
    uint8_t alloc_array; // 用于表示是否可以free bitarray指向的数组
    size_t arraysize;  // 位图的大小，以位为单位
    size_t totalbits;

}bitmap_t;

bitmap_t bitmap_create(uint32_t* bitarray, size_t totalbits);
void bitmap_set_bit(bitmap_t* this, size_t index);
bool bitmap_alloc_first_free(bitmap_t* this, uint32_t* bit);
void bitmap_clear_bit(bitmap_t* this, size_t index);
void bitmap_destroy(bitmap_t* this);
bool bitmap_init(bitmap_t* this, uint32_t* bitarray, size_t totalbits);
void bitmap_clear(bitmap_t* this);

#endif  // UTILS_BIT_MAP_H