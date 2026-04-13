#include "monitor/monitor.h"
#include "mem/gdt.h"
#include "interrupt/interrupt.h"
#include "interrupt/time.h"
#include "mem/page.h"
#include "common/types.h"
#include "common/secure.h"
#include "mem/kheap.h" 
#include "task/thread.h"
#include "task/scheduler.h"

void run_all_heap_tests();



int main(){
    monitor_clear();  //清空屏幕
    monitor_print("Hello, World!\n");
    monitor_print("This is a simple OS kernel with scrolling support.\n");
    init_gdt();
    monitor_print("GDT initialized successfully.\n");
    monitor_printf("i am a kernel, my address is %d\n", 1111);
    init_idt();
    init_timer(TIMER_FREQUENCY);
    monitor_print("IDT and timer initialized successfully.\n");
    init_page();
    monitor_print("page stage1 initialized successfully.\n");
    init_kheap();
    init_page_stage2();
    monitor_print("successfully init kheap\n");
    
    //run_all_heap_tests();
    init_task_manager();
    init_scheduler();


    Panic("main function cloudn't be here");
}

// 引入你的 monitor_printf 和 panic 等库

// ==========================================
// 测试 1：基础分配与读写测试 (Basic Sanity Check)
// ==========================================
void test_heap_basic() {
    monitor_printf("[Test 1] Basic Allocation...\n");
    
    uint32_t *ptr1 = (uint32_t*)kmalloc(sizeof(uint32_t) * 100); // 申请 400 字节
    uint32_t *ptr2 = (uint32_t*)kmalloc(sizeof(uint32_t) * 100); // 再申请 400 字节

    if (ptr1 == NULL || ptr2 == NULL) Panic("Basic alloc failed!");
    if (ptr1 == ptr2) Panic("Heap returned same address for two allocs!");

    // 测试读写是否正常（没有触发 Page Fault）
    for (int i = 0; i < 100; i++) {
        ptr1[i] = 0xAAAA;
        ptr2[i] = 0xBBBB;
    }

    for (int i = 0; i < 100; i++) {
        if (ptr1[i] != 0xAAAA || ptr2[i] != 0xBBBB) {
            Panic("Data corruption detected in basic test!");
        }
    }

    kfree(ptr1);
    kfree(ptr2);
    monitor_printf("[Test 1] PASS\n");
}

// ==========================================
// 测试 2：页对齐测试 (Page Alignment Test)
// ==========================================
void test_heap_alignment() {
    monitor_printf("[Test 2] Page Alignment...\n");
    
    // 故意先分配一个不对齐的碎片，打乱堆的布局
    void *junk = kmalloc(13); 

    // 申请页对齐的内存
    uint32_t aligned_ptr = (uint32_t)kmalloc_align(256); 

    if (aligned_ptr == 0) Panic("Aligned alloc failed!");

    // 验证地址的低 12 位必须全为 0！
    if ((aligned_ptr & 0xFFF) != 0) {
        Panic("Alignment test failed! Address returned: 0x%x", aligned_ptr);
    }

    kfree(junk);
    kfree((void*)aligned_ptr);
    monitor_printf("[Test 2] PASS\n");
}

// ==========================================
// 测试 3：“瑞士奶酪”与合并测试 (Coalescing Test)
// 极其重要！测试 kfree 是否正确合并了相邻的碎片
// ==========================================
void test_heap_coalescing() {
    monitor_printf("[Test 3] Coalescing (Merging Holes)...\n");
    
    // 1. 连续分配 A, B, C 三个块
    void *a = kmalloc(1000);
    void *b = kmalloc(1000);
    void *c = kmalloc(1000);
    
    // 2. 此时堆里是 [A][B][C]
    // 我们释放 A 和 C，保留 B。此时堆里是 [空闲A][B][空闲C]，这是典型的“瑞士奶酪”碎片化
    kfree(a);
    kfree(c);
    
    // 尝试分配一个 2500 字节的大块。因为最大空闲块只有 1000，如果不扩容，应该分配在别处
    // (具体行为取决于你是否写了堆扩张)
    
    // 3. 核心测试：释放 B！
    // 此时 [空闲A], [被释放的B], [空闲C] 物理相邻。
    // 如果你的 kfree 逻辑正确，它们会被瞬间合并成一个 > 3000 字节的巨大空闲块！
    kfree(b);
    
    // 4. 验证合并：此时我们尝试申请 2800 字节
    // 如果没有合并，找遍空闲链表只有三个 1000 左右的块，堆会被迫向 VMM 申请新页（扩容）
    // 如果合并成功，这个巨大的块能完美复用之前的空间，返回的地址应该和原来 A 的地址完全一样！
    void *huge_block = kmalloc(2800);
    if (huge_block != a) {
        monitor_printf("WARNING: Coalescing might have failed, or heap expanded unnecessarily.\n");
        monitor_printf("Expected addr: 0x%x, Got: 0x%x\n", (uint32_t)a, (uint32_t)huge_block);
    }

    kfree(huge_block);
    monitor_printf("[Test 3] PASS\n");
}

// ==========================================
// 测试 4：堆自动扩张与压力测试 (Heap Expansion & Stress Test)
// ==========================================
void test_heap_expansion() {
    monitor_printf("[Test 4] Heap Expansion & Stress...\n");
    
    // 假设你的堆初始大小是 16KB (4页)
    // 我们疯狂申请内存，逼迫它越过边界，触发 kheap_expand 缺页分配
    
    void *blocks[50];
    for (int i = 0; i < 50; i++) {
        // 每次申请 4096 字节（1页）
        // 50 次就是 200KB，绝对超出了初始容量
        blocks[i] = kmalloc(4096);
        if (blocks[i] == NULL) {
            Panic("Heap failed to expand at iteration %d!", i);
        }
        
        // 确保能写入物理内存
        *(uint32_t*)blocks[i] = 0xDEADBEEF; 
    }

    // 全部释放，测试扩容后的大量内存释放会不会崩
    for (int i = 0; i < 50; i++) {
        kfree(blocks[i]);
    }
    monitor_printf("[Test 4] PASS\n");
}

// ==========================================
// 总测试执行器
// ==========================================
void run_all_heap_tests() {
    monitor_printf("\n--- STARTING KERNEL HEAP TESTS ---\n");
    
    test_heap_basic();
    test_heap_alignment();
    test_heap_coalescing();
    test_heap_expansion();
    
    monitor_printf("--- ALL HEAP TESTS PASSED SUCCESSFULLY! ---\n\n");
}