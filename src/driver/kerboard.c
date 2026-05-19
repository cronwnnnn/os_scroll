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
// 判断当前某个字符是断的还是通的
static bool key_state[256];

static int32_t read_keyboard_char_impl();
static void keyboard_interrupt_handler();

static int32_t next_queue(int32_t index){
    return (index + 1) % KEYBOARD_BUF_SIZE;
}

static int32_t enqueue(keyboard_event_t event){
    if (keyboard_queue.size == KEYBOARD_BUF_SIZE) {
        return -1;
    }

    keyboard_queue.buffer[keyboard_queue.tail] = event;
    keyboard_queue.size++;
    keyboard_queue.tail = next_queue(keyboard_queue.tail);
    return 0;
}

static bool dequeue(keyboard_event_t* out_event){
    if (keyboard_queue.size == 0) {
        return false;
    }

    *out_event = keyboard_queue.buffer[keyboard_queue.head];
    keyboard_queue.size--;
    keyboard_queue.head = next_queue(keyboard_queue.head);
    return true;
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

    int32_t augchar = process_scancode((int)scancode);

    keyboard_event_t event;
    if (KH_HASDATA(augchar)){
        event.is_make = KH_ISMAKE(augchar);
        event.character = KH_GETCHAR(augchar);
        event.key = KH_GETRAW(augchar);

        spinlock_lock(&keyboard_lock);
        key_state[event.key] = event.is_make;
        enqueue(event);
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
    }
    // 立刻调度
    get_crt_thread()->need_reschedule = true;
}

// 用作系统调用，如果没有读到值则阻塞并加入等待队列，否则返回读到的字符
int32_t read_keyboard_char_wait(){
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


int32_t read_keyboard_char_right_now(keyboard_event_t* event){
    bool init_status;
    spin_lock_irqsave(&keyboard_lock, &init_status);
    if(dequeue(event)){
        spin_unlock_irqrestore(&keyboard_lock, init_status);
        return true;
    }
    spin_unlock_irqrestore(&keyboard_lock, init_status);
    return false;
}

int32_t get_key_state(uint32_t key){
    if(key >= 256){
        return false;
    }
    uint8_t state = key;
    bool status;
    bool make;
    spin_lock_irqsave(&keyboard_lock, &status);
    make = key_state[state];
    spin_unlock_irqrestore(&keyboard_lock, status);
    return make;
}



// 只要读出一个有效字符就返回，当队列为空时返回-1，阻塞当前线程
static int32_t read_keyboard_char_impl() {
    keyboard_event_t event;
    
    while (dequeue(&event)) {
        // 如果按键是按下(Make)状态，就向上传递实际字符。
        // 作为标准终端系统调用，松开按键不用处理。
        if (event.is_make) {
            return event.character;
        }
    }
    
    // 如果队列空了，返回-1让阻塞读进入等待队列。
    return -1;
}
