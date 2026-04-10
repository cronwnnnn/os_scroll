#ifndef MEM_KHEAP_H
#define MEM_KHEAP_H
#include "utils/ordered_array.h"
#include "common/types.h"

#define KHEAP_START 0xC0C00000
#define KHEAP_MIN_SIZE 0x10000 // 64KB initial
#define KHEAP_MAX 0xE0000000

#define KHEAP_INDEX_NUM      0x1000 // Only keeps 4096 tracks (16KB) to fit inside 64KB
#define KHEAP_MAGIC          0x123060AB



void init_kheap();
void kfree(void* ptr);
void* kmalloc_align(size_t size);
void* kmalloc(size_t size);


struct kheap_block_header{
    uint32_t magic;
    uint8_t is_hole;
    // size是不包括header和footer的大小
    size_t size;
}__attribute__((packed));
typedef struct kheap_block_header kheap_block_header_t;

struct kheap_block_footer{
    uint32_t magic;
    kheap_block_header_t* header;
}__attribute__((packed));
typedef struct kheap_block_footer kheap_block_footer_t;

typedef struct {
    ordered_array_t index;
    uint32_t start_address;
    uint32_t end_address;
    size_t max_size;
    size_t size;
    uint8_t supervisor; //是否只有内核可访问
    uint8_t readonly;
}kheap_t;


#endif