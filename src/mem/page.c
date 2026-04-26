#include "mem/page.h"
#include "interrupt/interrupt.h"
#include "monitor/monitor.h"
#include "common/types.h"
#include "utils/bit_map.h"
#include "common/secure.h"
#include "common/stdlib.h"
#include "utils/utils.h"
#include "sync/yieldlock.h"
#include "mem/kheap.h"
#include "utils/hash_table.h"
#include "task/scheduler.h"
#include "task/process.h"

void map_page(uint32_t vaddr);
void map_page_with_frame(uint32_t vaddr, int32_t frame);
void map_page_with_frame_impl(uint32_t vaddr, int32_t frame);
void release_pages(uint32_t vaddr, size_t page_count, bool frame_free);
void release_phy_frame(uint32_t frame);
void handle_cow_fault(uint32_t vaddr);
static inline void inc_cow_ref(uint32_t frame);

// 用于查找frame中，优化每次从头遍历的问题
static uint32_t last_allocated_index = 0;

static uint16_t phy_frames_array[PHYSICAL_MEM_SIZE / PAGE_SIZE];
static yieldlock_t phy_frames_map_lock;

page_directory_t* current_page_directory = 0;
static page_directory_t kernel_page_directory;

static yieldlock_t page_copy_lock;


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
    int32_t rw = err_code & 0x2;    // 是否是写入触发的异常
    int32_t user_mode = err_code & 0x4; // 是否是因为越权访问发生的错误

    // 最开始的4kbframe 不允许访问和存储，防止NULL指针访问以及偏移访问
    if (faulting_address < 0x1000) { 
        Panic("Segmentation Fault! Null pointer dereference at 0x%x\n", faulting_address);
    }

    if (!present) {
        // 找到是否由于页不存在导致的缺页
        map_page(faulting_address / PAGE_SIZE * PAGE_SIZE);
        reload_page_dir(current_page_directory);
        return;
    } 
    
    // 如果页面存在 (present == 1) 且是因为写入 (rw == 1) 导致的中断，这可能是 COW
    if (present && rw) {

        if (user_mode && faulting_address >= 0xC0000000) {
            monitor_printf("Protection Fault! User process trying to write Kernel memory at 0x%x\n", faulting_address);
            Panic("System halted due to Privilege Violation.");
        }

        // 这里稍后填入调用专门处理 COW 的函数
        handle_cow_fault(faulting_address);
        reload_page_dir(current_page_directory);
        return;
    }

    monitor_print("Unhandled Page Fault!\n");
    Panic("System halted due to illegal memory access.");
}

void init_page() {
    // 必须提前初始化锁，因为下面的 release_pages 会用到
    yieldlock_init(&phy_frames_map_lock);
    yieldlock_init(&page_copy_lock);


    memset(phy_frames_array, 0, sizeof(phy_frames_array)); 
    for(int32_t i = 0; i < 3 * 1024 * 1024 / PAGE_SIZE ; i++) {
        // 3mb mem has been alloccated
        phy_frames_array[i] = 1;
    }

    //最后一页为内核栈的位置，预先占用，防止被分配出去
    phy_frames_array[PHYSICAL_MEM_SIZE / PAGE_SIZE - 1] = 1;

    kernel_page_directory.page_directory_entries = KERNEL_PAGE_DIR_PHY;

    current_page_directory = &kernel_page_directory;

    // +1是因为地址是从0开始的，所以最后一个地址是0xffffffff而不是0xffffffff-1，
    // 比如0-9共10个数，存三个字节在最后，应该是7、8、9，即起始地址为7(10 - 3)而非6(9 - 3)
    // 而且0xffffffff是uint32的最大值，必须在计算之后再加1
    // 至于第二个参数不用 + page_size - 1，是因为kernelbinsize是1mb，不会出现余数

    // 删除已经加载的kenel bin 文件，将最后一个参数设为false正式因为之前根本没置0，完全可以当作空的
    release_pages(0xffffffff - KERNEL_BIN_LOAD_SIZE + 1, KERNEL_BIN_LOAD_SIZE / PAGE_SIZE, false);

    register_interrupt_handler(14, &pagefault_handler);
}



/*===================不能直接调用这个函数，而是调用release_pages=================*/
static void release_page(uint32_t vaddr, bool frame_free){
    // 只通过被release_pages调用，因为那里面有pde是否存在的检查
    // 用于在页表中删除某一页表项 或 一并删除对应的物理frame
    // frame_free表示是否要把物理内存也释放掉，还是只是解除映射
    if(multi_task_is_enabled()){
        yieldlock_lock(&get_crt_thread()->process->page_dir_lock);
    }
    
    uint32_t pte_index = vaddr >> 12;
    pte_t* pt = (pte_t*)PAGE_TABLES_VIRTUAL;
    pte_t* pte = pt + pte_index;
    if(!pte->present){
        if(multi_task_is_enabled()){
            yieldlock_unlock(&get_crt_thread()->process->page_dir_lock);
        }
        return;
    }
    if(frame_free){
        // release_phy_frame 既用于释放frame也用于减少cwo引用
        release_phy_frame(pte->frame);
    }
    *((uint32_t*)pte) = 0; //清除pte
    reload_page_dir(current_page_directory); //刷新页目录

    if(multi_task_is_enabled()){
        yieldlock_unlock(&get_crt_thread()->process->page_dir_lock);
    }
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
    yieldlock_lock(&phy_frames_map_lock);
    uint32_t total_frames = PHYSICAL_MEM_SIZE / PAGE_SIZE;
    uint32_t start_index = last_allocated_index;

    for(int32_t i = 0; i < total_frames; i++){
        // 这样保证能循环一遍
        uint32_t current_index = (start_index + i) % total_frames;
        if(phy_frames_array[current_index] == 0){
            phy_frames_array[current_index] = 1;
            last_allocated_index = current_index;
            yieldlock_unlock(&phy_frames_map_lock);
            // frame是20位，有12位为0就没用,正好还可以用-1表示没查到
            return (int32_t)current_index;
        }
    }
    yieldlock_unlock(&phy_frames_map_lock);
    return -1;
}

void release_phy_frame(uint32_t frame){
    // release_phy_frame 既用于释放frame也用于减少cwo引用
    Assert(frame < (PHYSICAL_MEM_SIZE / PAGE_SIZE));
    yieldlock_lock(&phy_frames_map_lock);
    Assert(phy_frames_array[frame] > 0);
    phy_frames_array[frame]--;
    yieldlock_unlock(&phy_frames_map_lock);
}

int32_t get_phy_frame_ref(uint32_t frame){
    Assert(frame < (PHYSICAL_MEM_SIZE / PAGE_SIZE));
    yieldlock_lock(&phy_frames_map_lock);
    int32_t cnt = phy_frames_array[frame];
    yieldlock_unlock(&phy_frames_map_lock);
    return cnt;
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
    if(multi_task_is_enabled()){
        yieldlock_lock(&get_crt_thread()->process->page_dir_lock);
    }
    map_page_with_frame_impl(vaddr, frame);
    if(multi_task_is_enabled()){
        yieldlock_unlock(&get_crt_thread()->process->page_dir_lock);
    }
}

void map_page_with_frame_impl(uint32_t vaddr, int32_t frame){
    // 设置访问权限
    uint8_t is_user_page = (vaddr < 0xC0000000) ? 1 : 0;
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
        pde->user = is_user_page;
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
        Assert(pte->present == 0);
        pte->present = 1;
        pte->user = is_user_page;
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
            pte->user = is_user_page;
            pte->frame = frame;
            
            // 分配完成之后刷新页目录使其能被查到,并清理分配物理地址处的内存，防止访问到之前该物理地址的内容
            reload_page_dir(current_page_directory);
            clear_page(vaddr);
        }else{
            // 这是由于kmalloc后又再次调用了map_page，但是kmalloc实际已经触发page_fault了，不管即可
            return;
        }
        
        
    }
}

void handle_cow_fault(uint32_t vaddr){
    monitor_print("use cow!!!!!!!!!");

    uint32_t pd_index = vaddr >> 22;
    pde_t* pd = (pde_t*)PAGE_DIR_VIRTUAL;
    pde_t* pde = pd + pd_index; 
    Assert(pde->present);

    uint32_t pte_index = vaddr >> 12;
    pte_t* kernel_page_table_virtual = (pte_t*) PAGE_TABLES_VIRTUAL;
    pte_t* pte = kernel_page_table_virtual + pte_index;
    Assert(pte->present);

    int32_t frame = pte->frame;  

    // 释放后一个进程都不指向它了
    Assert(get_phy_frame_ref(frame) >= 1);
    // 若-1之后变为了0，即没有页引用它
    if(get_phy_frame_ref(frame) == 1){
        // 就剩一个进程用它了，直接允许即可
        pte->rw = 1;
        reload_page_dir(current_page_directory);
    }else{
        int32_t cow_frame = alloc_phy_frame();
        if(cow_frame == -1){
            Panic("couldn't alloc frame for addr in cow");
        }
        // 仍然有多个进程指向该frame
        // 不能使用kmalloc来分配，因为可能导致另一个page_fault,不能在一个page_fault中触发另一个
        yieldlock_lock(&page_copy_lock);
        uint32_t copy_page = COPIED_PAGE_VADDR;
        // 不能用map_page 会被锁住的，直接死锁，只能用impl
        map_page_with_frame_impl(copy_page, cow_frame);
        memcpy((void*)copy_page, (void*)(vaddr / PAGE_SIZE * PAGE_SIZE), PAGE_SIZE);
        yieldlock_unlock(&page_copy_lock);

        if(multi_task_is_enabled()){
            yieldlock_lock(&get_crt_thread()->process->page_dir_lock);
        }
        pte->frame = cow_frame;
        // 当前的改了，之后如果是2另外一个怎么办，再触发一次page_fault吗，memcpy的pte和源代码的写法相同吗
        pte->rw = 1;
        if(multi_task_is_enabled()){
            yieldlock_unlock(&get_crt_thread()->process->page_dir_lock);
        }        


        release_pages(copy_page, 1, false);

        // 减去一个ref，如果等于1就不用减去，因为只剩一个了
        release_phy_frame(frame);
        
    }
}


page_directory_t clone_crt_page_dir(){
    // 用于创建新进程时clone当前进程的页表和页目录，为cow做准备

    int32_t new_pd_frame = alloc_phy_frame();
    if(new_pd_frame < 0){
        Panic("couldn't alloc frame for copied page dir\n");
    }

    // copid_page_dir 是临时分配的一块虚拟地址，用于将当前页目录复制到新进程的页目录，复制完就kfree
    // 表明当前进程不能访问另一个进程的页目录
    uint32_t copied_page_dir = (uint32_t)COPIED_PAGE_DIR_VADDR;
    
    // 获取全局拷贝锁，防止多个进程同时fork使用同个临时虚拟地址
    yieldlock_lock(&page_copy_lock);
    map_page_with_frame(copied_page_dir, new_pd_frame);
    reload_page_dir(current_page_directory);
    // 将申请的一页内存值清空为0
    clear_page(copied_page_dir);

    pde_t* new_pd = (pde_t*) copied_page_dir;
    pde_t* crt_pd = (pde_t*) PAGE_DIR_VIRTUAL;
    // 对于第一个pde，是共有的，即4mb低地址处的值
    *new_pd = *crt_pd;

    for(int32_t i = 768; i < 1024; i++){
        pde_t* new_pde = new_pd + i;
        // 对于存放页目录的位置，直接写入新的页目录的物理地址，只有这一项与原页目录不同
        // 其他的完全复制原本的页目录的内容即可
        if(i == 769){
            new_pde->present = 1;
            new_pde->rw = 1;
            new_pde->user = 0;
            new_pde->frame = new_pd_frame;
        }else{
            *new_pde = *(crt_pd + i);
        }
    }

    // copid_page_dir 是临时分配的一块虚拟地址，用于将当前所有页表复制到新进程的页目录，复制完就kfree
    // 表明当前进程不能访问另一个进程的页表
    uint32_t copied_page_table = (uint32_t)COPIED_PAGE_TABLE_VADDR;
    // 低4mb处已经写入了？？ 是共有的？？
    for(int32_t i = 1; i < 768; i++){
        pde_t* crt_pde = crt_pd + i; 

        if(crt_pde->present == 0){
            continue;
        }

        // --- 开始对当前进程页表加锁，防止克隆过程中页表被其他线程修改 ---
        if(multi_task_is_enabled()){
            yieldlock_lock(&get_crt_thread()->process->page_dir_lock);
        }

        // 开始多轮复制页表
        int32_t new_pt_frame = alloc_phy_frame();
        if(new_pt_frame < 0){ // 修复：这里原来拼错成了 new_pd_frame
            Panic("couldn't alloc frame for copied page dir\n");
        }
        
        // 修复死锁：已经加了 page_dir_lock，绝对不能调用外层的 map_page_with_frame，否则会同一把锁加两次导致死锁！
        map_page_with_frame_impl(copied_page_table, new_pt_frame);
        reload_page_dir(current_page_directory);
        
        // 修复严重Bug：原代码是把 crt_pde 拷贝给 copied_page_dir。
        // 这会导致把原页目录项及后续 4KB 垃圾数据覆盖到新页目录。
        // 应当是：把原进程当前的“页表”（PAGE_TABLES_VIRTUAL + i * PAGE_SIZE）完整拷贝到新的“页表”（copied_page_table） 
        memcpy((void*)copied_page_table, (void*)(PAGE_TABLES_VIRTUAL + i * PAGE_SIZE), PAGE_SIZE); 
        
        // 开始给每个有效表项增加引用并设置为只读(COW 核心)
        for(int32_t j = 0; j < 1024; j++){ // 注意：这里原代码是 i<1024，这是一个低级bug，已修正为 j<1024
            pte_t* crt_pte = (pte_t*)(PAGE_TABLES_VIRTUAL + i * PAGE_SIZE) + j;
            pte_t* new_pte = (pte_t*)copied_page_table + j; // 同上，已修正
            if(!new_pte->present){
                continue;
            }
            crt_pte->rw = 0;
            new_pte->rw = 0;
            inc_cow_ref(new_pte->frame);
        }
        reload_page_dir(current_page_directory);

        if(multi_task_is_enabled()){
            yieldlock_unlock(&get_crt_thread()->process->page_dir_lock);
        }
        // --- 结束加锁 ---

        // 将新进程的页目录指向的值改为新进程的页表，而非之前进程的页表
        pde_t* new_pde = new_pd + i;
        new_pde->present = 1;
        new_pde->rw = 1;
        new_pde->user = 1;
        new_pde->frame = new_pt_frame;

        // 本轮用的页表拷贝虚拟地址已完成，立刻解除映射！
        // 如果不释放，下一轮循环 map_page_with_frame_impl 中 assert(pte->present == 0) 100% 崩溃！
        release_pages(copied_page_table, 1, false);
    }


    // 现在已经创建了新进程的页目录和页表了，删除映射 (因为每轮内部已释放 PT，所以这里只释放 PD 即可)
    release_pages(copied_page_dir, 1, false);
    
    // 释放临时页专属拷贝锁
    yieldlock_unlock(&page_copy_lock);

    // release_page中已经reload_page_dir了，不用再调用

    page_directory_t new_page_dir;
    // 指向新进程的页目录的物理地址
    new_page_dir.page_directory_entries = new_pd_frame * PAGE_SIZE;

    return new_page_dir;
}


// 让引用加一
static inline void inc_cow_ref(uint32_t frame) {
    Assert(frame < (PHYSICAL_MEM_SIZE / PAGE_SIZE));
    yieldlock_lock(&phy_frames_map_lock);
    phy_frames_array[frame]++;
    yieldlock_unlock(&phy_frames_map_lock);
}
