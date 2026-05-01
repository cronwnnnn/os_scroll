#include "driver/keyboard.h"
#include "interrupt/interrupt.h"
#include "sync/spinlock.h"
#include "utils/linked_list.h"
#include "common/stdlib.h"
#include "common/io.h"
#include "task/thread.h"
#include "task/scheduler.h"
#include "driver/keyboard_read.h"

static spinlock_t keyboard_lock;
static buffer_queue_t keyboard_queue;
// 存储因等待键盘输入而阻塞的线程
static linked_list_t wait_tasks;

static int32_t read_keyboard_char_impl();
static void keyboard_interrupt_handler();

static int32_t next_queue(int32_t index){
    return (index + 1) % KEYBOARD_BUF_SIZE;
}

static int32_t enqueue(uint8_t scancode){
    if (keyboard_queue.size == KEYBOARD_BUF_SIZE) {
    return -1;
    }

    keyboard_queue.buffer[keyboard_queue.tail] = scancode;
    keyboard_queue.size++;
    keyboard_queue.tail = next_queue(keyboard_queue.tail);
    return 0;
}

static uint8_t dequeue(){

    uint8_t code = keyboard_queue.buffer[keyboard_queue.head];
    keyboard_queue.size--;
    keyboard_queue.head = next_queue(keyboard_queue.head);
    return code;
}

void init_keyboard(){
    spinlock_init(&keyboard_lock);
    linked_list_init(&wait_tasks);

    memset(&keyboard_queue, 0, sizeof(keyboard_queue));
    keyboard_queue.head = 0;
    keyboard_queue.tail = 0;
    keyboard_queue.size = 0;

    register_interrupt_handler(IRQ1_INT_NUM, &keyboard_interrupt_handler);
}

static void keyboard_interrupt_handler(){
    uint8_t scancode = inb(0x60);

    spinlock_lock(&keyboard_lock);
    enqueue(scancode);
    linked_list_t get_wait_tasks;
    linked_list_move(&get_wait_tasks, &wait_tasks);
    spinlock_unlock(&keyboard_lock);

    thread_node_t* node = get_wait_tasks.head;
    while(node != NULL){
        thread_node_t* next_node = node->next;
        linked_list_remove(&get_wait_tasks, node);
        add_thread_node_to_schedule_head(node);
        node = next_node;
    }
    // 立刻调度
    get_crt_thread()->need_reschedule = true;
}

// 用作系统调用，如果没有读到值则阻塞并加入等待队列，否则返回读到的字符
int32_t read_keyboard_char(){
    bool init_status;
    spin_lock_irqsave(&keyboard_lock, &init_status);
    int32_t c;
    while (1){
        c = read_keyboard_char_impl();
        if(c != -1){ 
            break;
        }
        thread_node_t* node = get_crt_thread_node();
        tcb_t* thread = (tcb_t*)node->ptr;
        thread->crt_queue = &wait_tasks;
        linked_list_append(&wait_tasks, node);
        schedule_mark_thread_block();
        // 否则加入到阻塞队列，等待被唤醒之后
        spin_unlock_irqrestore(&keyboard_lock, init_status);
        
        // 之后将当前线程退出
        schedule_thread_yield();
        // 等再次被调度时仍要上锁才行
        spin_lock_irqsave(&keyboard_lock, &init_status);
    }
    spin_unlock_irqrestore(&keyboard_lock, init_status);
    return c;
}


// 只要读出一个有效字符就返回，当队列为空时返回-1，阻塞当前线程
static int32_t read_keyboard_char_impl() {
  if (keyboard_queue.size == 0) {
    return -1;
  }

  int32_t augchar = process_scancode((int)dequeue());
  // 是一个有效字符并且是一个通码，就可以停止循环并返回这个结果了
  // 否则一直尝试读取，直到将队列读空，就退出
  while (!(KH_HASDATA(augchar) && KH_ISMAKE(augchar))) {
    if (keyboard_queue.size == 0) {
      return -1;
    }
    augchar = process_scancode((int)dequeue());
  }
  return KH_GETCHAR(augchar);
}
