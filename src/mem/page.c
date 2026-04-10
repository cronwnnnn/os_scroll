#include "mem/page.h"
#include "interrupt/interrupt.h"
#include "monitor/monitor.h"
#include "common/types.h"
#include "utils/bit_map.h"
#include "common/secure.h"
#include "common/stdlib.h"
#include "utils/utils.h"

void map_page(uint32_t vaddr);
void map_page_with_frame(uint32_t vaddr, int32_t frame);
void map_page_with_frame_impl(uint32_t vaddr, int32_t frame);
void release_pages(uint32_t vaddr, size_t page_count, bool frame_free);
void release_phy_frame(uint32_t frame);

static uint32_t bitarray[PHYSICAL_MEM_SIZE / PAGE_SIZE / 32];
static page_directory_t kernel_page_directory;
static bitmap_t phy_frames_map;
page_directory_t* current_page_directory = 0;

void reload_page_dir(page_directory_t *dir){
    //刷新cr3 页目录，让其重新读取页目录的新增内容
    current_page_directory = dir;
    asm volatile ("mov %0, %%cr3" :: "r"(current_page_directory->page_directory_entries));
}

static void pagefault_handler(isr_params_t regs) {
    uint32_t faulting_address;
    asm volatile ("mov %%cr2, %0": "=r"(faulting_address));
    int32_t err_code = regs.error_code;
    int32_t present = err_code & 0x1;
    /*
    int32_t rw = err_code & 0x2;
    int32_t user = err_code & 0x4; // 页面的访问权限
    */
    // 最开始的4kbframe 不允许访问和存储，防止NULL指针访问以及偏移访问
    if (faulting_address < 0x1000) { 
        Panic("Segmentation Fault! Null pointer dereference or invalid memory access.");
    }

    if(!present){
        //找到是否由于页不存在导致的缺页
        map_page(faulting_address / PAGE_SIZE * PAGE_SIZE);
        reload_page_dir(current_page_directory);
        return;
    }
    monitor_print("Unhandled Page Fault!\n");
    Panic("System halted due to illegal memory access.");
}

void init_page() {

    phy_frames_map = bitmap_create(bitarray, PHYSICAL_MEM_SIZE / PAGE_SIZE);
    
    for(int32_t i = 0; i < 3 * 1024 * 1024 / PAGE_SIZE / 32 ; i++) {
        // 3mb mem has been alloccated
        bitarray[i] = 0xffffffff;
    }

    //最后一页为内核栈的位置，预先占用，防止被分配出去
    bitmap_set_bit(&phy_frames_map, PHYSICAL_MEM_SIZE / PAGE_SIZE - 1); 

    kernel_page_directory.page_directory_entries = KERNEL_PAGE_DIR_PHY;

    current_page_directory = &kernel_page_directory;

    // +1是因为地址是从0开始的，所以最后一个地址是0xffffffff而不是0xffffffff-1，
    // 比如0-9共10个数，存三个字节在最后，应该是7、8、9，即起始地址为7(10 - 3)而非6(9 - 3)
    // 而且0xffffffff是uint32的最大值，必须在计算之后再加1
    // 至于第二个参数不用 + page_size - 1，是因为kernelbinsize是1mb，不会出现余数
    release_pages(0xffffffff - KERNEL_BIN_LOAD_SIZE + 1, KERNEL_BIN_LOAD_SIZE / PAGE_SIZE, true);


    register_interrupt_handler(14, &pagefault_handler);
}

static void release_page(uint32_t vaddr, bool frame_free){
    // frame_free表示是否要把物理内存也释放掉，还是只是解除映射
    uint32_t pte_index = vaddr >> 12;
    pte_t* pt = (pte_t*)PAGE_TABLES_VIRTUAL;
    pte_t* pte = pt + pte_index;
    if(!pte->present){
        return;
    }
    if(frame_free){
        release_phy_frame(pte->frame);
    }
    *((uint32_t*)pte) = 0; //清除pte
    reload_page_dir(current_page_directory); //刷新页目录

}

void release_pages(uint32_t vaddr, size_t page_count, bool frame_free){
    vaddr = vaddr / PAGE_SIZE * PAGE_SIZE; //对齐到页边界
    uint32_t pte_index_start = vaddr >> 12;
    uint32_t pte_index_end = pte_index_start + page_count;

    uint32_t pde_index_start = vaddr >> 22;
    // 使用结束区块索引 = ((排他性结束元素索引 - 1) / 区块大小) + 1
    uint32_t pde_index_end = ((pte_index_end - 1) >> 10) + 1;

    for(size_t i = pde_index_start; i < pde_index_end; i++){
        pde_t* pde = (pde_t*)PAGE_DIR_VIRTUAL + i;
        if(!pde->present)
            continue;
        // 对于每个页目录项，开头要么是这该页目录项所指的页表的第一个页表项，要么是第一个要释放的页表项
        // 最后一个，要么是最后要释放的页表项，要么是该页目录项所指的页表的最后一个页表项
        // 注意计算规则是含头不含尾
        for(size_t j = max(i * 1024, pte_index_start); j < min(i * 1024 + 1024, pte_index_end); j++){
            // 上面的是20位的页表项值，还要加上12位的页内偏移才能得到完整的虚拟地址
            release_page(j * PAGE_SIZE, frame_free);
        }
    }
}

int32_t alloc_phy_frame(){
    //使用int 为了返回错误码-1
    // 注意返回的是第几个frame，不是地址，要*4kb才是物理地址
    uint32_t frame = 0;
    if(!bitmap_alloc_first_free(&phy_frames_map, &frame))
        return -1;
    return (int32_t)frame;
}

void release_phy_frame(uint32_t frame){
    bitmap_clear_bit(&phy_frames_map, frame);
}

// clear是清零，release 才是释放
void clear_page(uint32_t vaddr){
    uint32_t addr = vaddr / PAGE_SIZE * PAGE_SIZE;
    for(size_t i = 0; i < PAGE_SIZE / 4; i++) //一次删除4byte
        *((uint32_t*)addr + i) = 0;
}


void map_page(uint32_t vaddr){
    //用于分配一个页表，放在任意物理地址都可以
    map_page_with_frame(vaddr, -1);
}

void map_page_with_frame(uint32_t vaddr, int32_t frame){
    // 可指定分配到哪个物理地址中
    map_page_with_frame_impl(vaddr, frame);
}

void map_page_with_frame_impl(uint32_t vaddr, int32_t frame){
    // 先查看当前页目录中要分配的页表是否存在，如果不在页目录中，则先为页表分配4kb内存
    uint32_t pd_index = vaddr >> 22;
    pde_t* pd = (pde_t*)PAGE_DIR_VIRTUAL;

    //十分精妙的查找方式,由于pd现在是一个结构体类型的指针，那么加pd_index时是每次加一个struct的大小
    // 因而可以直接相加便得到最终的pde
    pde_t* pde = pd + pd_index; 

    // 如果页目录中没有这个页表，说明还未分配，先为其分配物理内存
    if(!pde->present){
        int32_t addr = alloc_phy_frame();
        if(addr < 0){
            monitor_printf("couldn't alloc frame for page table on %d\n", pd_index);
            Panic("couldn't alloc frame for page table");
        }
        pde->present = 1;
        pde->rw = 1;
        pde->user = 1;
        pde->frame = addr;
        
        // 分配完成之后刷新页目录使其能被查到,并清理页表处的物理地址，防止错误映射
        reload_page_dir(current_page_directory);
        clear_page(PAGE_TABLES_VIRTUAL + pd_index * PAGE_SIZE);
    }

    // 页表有物理内存了，再去映射4kb的物理地址，即处理真正的page_fault
    // 注意一次只会映射4kb，即一个页表项
    // 同上面一样，先找到相应的pagetable
    uint32_t pte_index = vaddr >> 12;
    pte_t* kernel_page_table_virtual = (pte_t*) PAGE_TABLES_VIRTUAL;
    pte_t* pte = kernel_page_table_virtual + pte_index;
    // 若是frame为正，说明指定了物理内存，就不需要分配了，直接映射即可
    // 用于fork这种方式
    if(frame > 0){
        pte->present = 1;
        pte->user = 1;
        pte->rw = 1;
        pte->frame = frame;
        reload_page_dir(current_page_directory);
    }
    // 若没有指定frame，则需要分配物理内存并映射，但是有两种情况
    // 对于present为0，表示要访问的物理地址不存在，需要分配并且清零
    // 对于rw为0，是fork这种，使用写时复制，需要分配物理地址且拷贝之前物理地址的内容过来
    else{
        if(!pte->present){
            frame = alloc_phy_frame();
            if(frame < 0){
                monitor_printf("couldn't alloc frame for page table on %d\n", pd_index);
                Panic("couldn't alloc frame for page");
            }
            pte->present = 1;
            pte->rw = 1;
            pte->user = 1;
            pte->frame = frame;
            
            // 分配完成之后刷新页目录使其能被查到,并清理分配物理地址处的内存，防止访问到之前该物理地址的内容
            reload_page_dir(current_page_directory);
            clear_page(vaddr);
        }
        else if (!pte->rw)
        {
            Panic("cow is not complete");
            /*
            frame = alloc_phy_frame();
            if(frame < 0){
                monitor_printf("couldn't alloc frame for page table on %d\n", pd_index);
                Panic("111");
            }
            pte->present = 1;
            pte->rw = 1;
            pte->user = 1;
            pte->frame = frame;
            
            // 分配完成之后刷新页目录使其能被查到,并清理页表处的物理地址，防止错误映射
            reload_page_dir(current_page_directory);
        }
        */}
        
        
    }
}