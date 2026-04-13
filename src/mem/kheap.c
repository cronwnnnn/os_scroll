#include "mem/kheap.h" 
#include "mem/page.h"
#include "utils/ordered_array.h"
#include "common/secure.h"
#include "monitor/monitor.h"
#include "sync/yieldlock.h"

static kheap_t kheap;
static yieldlock_t kheap_lock;

#define BLOCK_HEADER_SIZE sizeof(kheap_block_header_t)
#define BLOCK_FOOTER_SIZE sizeof(kheap_block_footer_t)
#define BLOCK_META_SIZE (BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE)

#define IS_HOLE 1
#define NOT_HOLE 0

kheap_t create_kheap(uint32_t start, uint32_t end, uint32_t max, uint8_t supervisor, uint8_t readonly);
static kheap_block_header_t* make_block(uint32_t start, uint32_t size, uint8_t is_hole);


static uint32_t align_to_page(uint32_t size){
    return (size + 0xfff) & 0xfffff000;
}


static int32_t kheap_block_comparator(type_t a, type_t b){
    uint32_t size_a = ((kheap_block_header_t*)a)->size;
    uint32_t size_b = ((kheap_block_header_t*)b)->size;
    return size_a < size_b ? -1 : (size_a > size_b ? 1 : 0);
}

void init_kheap(){
    yieldlock_init(&kheap_lock);
    kheap = create_kheap(KHEAP_START, KHEAP_START + KHEAP_MIN_SIZE, KHEAP_MAX, 0, 0);
}

kheap_t create_kheap(uint32_t start, uint32_t end, uint32_t max, uint8_t supervisor, uint8_t readonly){
    Assert((start & 0xFFF) == 0);
    Assert((end & 0xFFF) == 0);
    
    kheap_t heap;

    // 由于heap被分为两个区域，kernel_index_num是用于存ordered_array的最大size，且在最开头，必须提前规划
    heap.index = ordered_array_create((type_t*)start, KHEAP_INDEX_NUM, kheap_block_comparator);

    start += KHEAP_INDEX_NUM * sizeof(type_t); //预留出ordered_array的空间

    // 防止加上index之后直接跳出当前kheap最初的容量限制了，导致出现bug
    Assert(start < end);
    // 如果不加判断的话，start比end大，size = end - start变得无比巨大，make_block的也无比巨大

    heap.start_address = start;
    heap.end_address = end;
    heap.max_size = max;
    heap.supervisor = supervisor;
    heap.readonly = readonly;
    heap.size = end - start;

    // 创建一个初始的大块，大小为end-start-block_meta_size，标记为hole
    make_block(start, end - start - BLOCK_META_SIZE, IS_HOLE);
    ordered_array_insert(&heap.index, (type_t)start);

    return heap;
}

// size 是用户请求的大小，不包含header和footer的大小
static kheap_block_header_t* make_block(uint32_t start, uint32_t size, uint8_t is_hole){
    uint32_t end = start + size + BLOCK_META_SIZE;

    kheap_block_header_t* header = (kheap_block_header_t*)start;
    header->magic = KHEAP_MAGIC;
    header->is_hole = is_hole;
    header->size = size;

    kheap_block_footer_t* footer = (kheap_block_footer_t*)(end - BLOCK_FOOTER_SIZE);
    footer->magic = KHEAP_MAGIC;
    footer->header = header;

    return header;
}

int32_t kheap_find_hole(kheap_t *this, uint32_t size, uint8_t align, uint32_t* alloc_pos){
    for(uint32_t i = 0; i < this->index.size; i++){
        kheap_block_header_t* header = (kheap_block_header_t*)ordered_array_get(&this->index, i);
        uint32_t start = (uint32_t)header + BLOCK_HEADER_SIZE;
        
        if(align){
            //对于需要分配的地址是页的整数倍的，先找到当前块中第一个和4kb对其的
            uint32_t end = start + header->size;
            uint32_t next_page_align = align_to_page(start);
            // 对于每一个对其的地址，如果装不下size也不行，直接转到下一个块中查找
            while(next_page_align + size <= end){
                // 就算能容下，从start到next_page_align之间的大小也必须能切出一个合法的新空闲块(>META_SIZE)，或者它一开始就完美对齐(=0)
                if((next_page_align - start) == 0 || (next_page_align - start) > BLOCK_META_SIZE){
                    *alloc_pos = next_page_align;
                    return i;
                }
                // 容不下就在当前块中找下一个对其的地址
                next_page_align += PAGE_SIZE;
            }
        }else{
            if(header->size >= size){
                *alloc_pos = start;
                return i;
            }
        }
    }
    return -1;
}

static uint32_t kheap_expand(kheap_t* this, uint32_t size){
    //monitor_printf("kheap expand size: %d\n", size);
    // 扩展的大小必须是整页 
    size = align_to_page(size);

    uint32_t new_end_addr = this->end_address + size;
    Assert(new_end_addr <= this->max_size);
    this->end_address = new_end_addr;
    this->size += size;
    return size;
}

static void* alloc(kheap_t* this, size_t size, uint8_t align){
    uint32_t alloc_pos;
    int32_t index = kheap_find_hole(this, size, align, &alloc_pos);
    // 没有满足大小的空闲块，扩展
    if(index < 0){
        uint32_t old_end_address = this->end_address;
        uint32_t extension_size = kheap_expand(this, size + BLOCK_META_SIZE);

        // 由于在最后扩容了，要改变最后一块
        kheap_block_footer_t* last_footer = (kheap_block_footer_t*)(old_end_address - BLOCK_FOOTER_SIZE);
        kheap_block_header_t* last_header = last_footer->header;
        // 最后一块是空闲块，为其更改大小和footer
        if(last_header->is_hole){
            make_block((uint32_t)last_header, last_header->size + extension_size, IS_HOLE);
            // 注意是按大小排序的有序的数组，要使用函数来移除，地址为最后的空闲块不一定在数组的最后
            int32_t remove = ordered_array_remove_element(&this->index, (type_t) last_header);
            Assert(remove);
            ordered_array_insert(&this->index, (type_t)last_header);
        }else{
            // 最后一块已分配，需要单独开一个block并分配
            kheap_block_header_t* new_last_header =  make_block(
                old_end_address, extension_size - BLOCK_META_SIZE, NOT_HOLE);
            ordered_array_insert(&this->index, (type_t)new_last_header);
        }
        // 已创建了更大的空闲块，分配试试
        return alloc(this, size, align);
    }
    // 对空闲块进行分配
    kheap_block_header_t* header = (kheap_block_header_t*)ordered_array_get(&this->index, index);
    Assert(header->magic == KHEAP_MAGIC);
    // block_size 是当前指向的块的有效载荷总大小
    size_t block_size = header->size;

    // 先移除当前空闲块
    ordered_array_remove(&this->index, index);

    // 对于需要对齐分配的情况
    if(align){
        kheap_block_header_t* alloc_block_header = (kheap_block_header_t*)(alloc_pos - BLOCK_HEADER_SIZE);
        if(alloc_block_header > header){
            // 看前面是否能切
            uint32_t cut_block_size = (uint32_t)alloc_block_header - (uint32_t)header;
            Assert(cut_block_size > BLOCK_META_SIZE);

            // 先切出前面的那个作为空闲块插入
            make_block((uint32_t)header, cut_block_size - BLOCK_META_SIZE, IS_HOLE);
            ordered_array_insert(&this->index, (type_t)header);
            // 相当于一并减去了新分配的块的header大小，与旧的header大小相互抵消，仍是有效载荷大小
            block_size -= cut_block_size;
            // 接下来处理要分配的块
            header = alloc_block_header;
        }
    }
        
    Assert(block_size >= size);
    // 除去size之后还剩的大小
    uint32_t remain_size = block_size - size;
    // 剩余空间太小则一并合入分配的块中
    if(remain_size <= BLOCK_META_SIZE){
        size += remain_size;
        remain_size = 0;
    }
    // 接下来将分配的块正式写入header和footer并返回
    make_block((uint32_t)header, size, NOT_HOLE);

    // 如果后面还能分，也分出一个空闲块并插入
    if(remain_size != 0){
        Assert(remain_size > BLOCK_META_SIZE);
        kheap_block_header_t* remain_header = make_block((uint32_t)header + size + BLOCK_META_SIZE, remain_size - BLOCK_META_SIZE, IS_HOLE);
        ordered_array_insert(&this->index, (type_t)remain_header);
    }
    return (void*) alloc_pos;

}

void free(kheap_t* this, void* addr){
    if(addr == NULL)
        return;

    kheap_block_header_t* header = (kheap_block_header_t*)((uint32_t)addr - BLOCK_HEADER_SIZE);
    kheap_block_footer_t* footer = (kheap_block_footer_t*)((uint32_t)addr + header->size);
    Assert(header->magic == KHEAP_MAGIC);
    Assert(footer->magic == KHEAP_MAGIC);
    header->is_hole = 1;
    kheap_block_header_t* new_header = header;

    // 要先合并后面的再合并前面的,否则在make_block时需要判断前面的空闲块是否合并
    kheap_block_header_t* next_header = (kheap_block_header_t*)((uint32_t)footer + BLOCK_FOOTER_SIZE);
    // 在访问后面的块时要保证地址不超过end_address
    // 合并后面的空闲块
    if((uint32_t)next_header < this->end_address && next_header->magic == KHEAP_MAGIC && next_header->is_hole == 1){
        make_block((uint32_t)header, header->size + next_header->size + BLOCK_META_SIZE, IS_HOLE);
        int32_t remove = ordered_array_remove_element(&this->index, (type_t)next_header);
        Assert(remove);
    }

    kheap_block_footer_t* last_footer = (kheap_block_footer_t*)((uint32_t)header - BLOCK_FOOTER_SIZE);
    // 访问前面的保证不超过start
    // 合并前面的空闲块
    if((uint32_t)last_footer >= this->start_address && last_footer->magic == KHEAP_MAGIC && last_footer->header->is_hole == 1){
        kheap_block_header_t* last_header = last_footer->header;
        make_block((uint32_t)last_header, last_header->size + header->size + BLOCK_META_SIZE, IS_HOLE);
        int32_t remove = ordered_array_remove_element(&this->index, (type_t)last_header);
        Assert(remove);
        new_header = last_header;
    }
    
    ordered_array_insert(&this->index, (type_t)new_header);
    

}

void* kmalloc_impl(size_t size, uint8_t align){
    if(size == 0)
        return 0;
    void* ptr = alloc(&kheap, size, align);
    if(ptr == NULL)
        Panic("kheap wrong, can't alloc mem");
    return ptr;
}

void* kmalloc(size_t size){
    yieldlock_lock(&kheap_lock);
    void* ptr = kmalloc_impl(size, 0);
    yieldlock_unlock(&kheap_lock);
    return ptr;
}

void* kmalloc_align(size_t size){
    yieldlock_lock(&kheap_lock);
    void* ptr = kmalloc_impl(size, 1);
    yieldlock_unlock(&kheap_lock);
    return ptr;
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    yieldlock_lock(&kheap_lock);
    free(&kheap, ptr);
    yieldlock_unlock(&kheap_lock);
}

