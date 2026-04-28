#include "utils/bit_map.h"
#include "common/types.h"
#include "common/secure.h"
#include "mem/kheap.h"



bool bitmap_init(bitmap_t* this, uint32_t* bitarray, size_t totalbits);


// 该函数返回的是bitmap_t类型，在接收时会被拷贝，因此内部ret即使被销毁也没事
//不过是浅拷贝，结构体中的数组必须是malloc或者已有的长期存在的内存，不能是局部变量，否则会导致悬空指针
bitmap_t bitmap_create(uint32_t* bitarray, size_t totalbits) {
    bitmap_t ret;
    bool success = bitmap_init(&ret, bitarray, totalbits);
    Assert(success);
    return ret;
}

void bitmap_clear(bitmap_t* this) {
  for (uint32_t i = 0; i < this->arraysize; i++) {
    this->bitarray[i] = 0;
  }
}


// 只是bitmap_mem的初始化函数，位图的内存由调用者提供，没用自动创建bitmap
bool bitmap_init(bitmap_t* this, uint32_t* bitarray, size_t totalbits) {
    this->arraysize = (totalbits + 31) / 32;  // 每个uint32_t有32位
    this->totalbits = totalbits;

    if(bitarray == NULL){
        bitarray = (uint32_t*) kmalloc(this->arraysize * 4);
        this->alloc_array = true;
    }else{
        this->alloc_array = false;
    }

    if(bitarray == nullptr || totalbits == 0) {
        return false;
    }
    this->bitarray = bitarray;

    for(size_t i = 0; i < this->arraysize; i++) {
        this->bitarray[i] = 0;  // 初始化所有位为0，表示全部未分配
    }
    return true;
}

void bitmap_set_bit(bitmap_t* this, size_t index) {
    if(index >= this->totalbits) {
        return;
    }
    size_t array_index = index / 32;
    size_t bit_index = index % 32;
    this->bitarray[array_index] |= ((uint32_t)1 << bit_index);
}

void bitmap_clear_bit(bitmap_t* this, size_t index) {
    if(index >= this->totalbits) {
        return;
    }
    size_t array_index = index / 32;
    size_t bit_index = index % 32;
    this->bitarray[array_index] &= ~((uint32_t)1 << bit_index);
}

bool bitmap_test_bit(bitmap_t* this, size_t index) {
    if(index >= this->totalbits) {
        return false;
    }
    size_t array_index = index / 32;
    size_t bit_index = index % 32;
    return (this->bitarray[array_index] & ((uint32_t)1 << bit_index)) != 0;
    
}

bool bitmap_find_first_free(bitmap_t* this, uint32_t* bit){
    // * bit 是返回分配的物理地址的位置
    for(size_t i = 0; i < (this->arraysize)-1; i++){
        if(this->bitarray[i] != 0xffffffff)
            for(size_t j = 0; j < 32; j++)
            {
                if(!(this->bitarray[i] & ((uint32_t)1 << j))){
                    *bit = i * 32 + j;
                    return true;
                }
            }
    }
    for(size_t j = 0; j < (this->totalbits % 32); j++){
        if(!(this->bitarray[this->arraysize-1] & ((uint32_t)1 << j))){
            *bit = (this->arraysize-1) * 32 + j;
            return true;
        }
    }
    return false;
}

bool bitmap_alloc_first_free(bitmap_t* this, uint32_t* bit){
    bool success = bitmap_find_first_free(this, bit);
    if (!success)
        return false;
    
    bitmap_set_bit(this, *bit);
    return true;
}

void bitmap_destroy(bitmap_t* this){
    if(this->alloc_array == true){
        kfree(this->bitarray);
    }
}
