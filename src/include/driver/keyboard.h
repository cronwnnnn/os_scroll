#ifndef DRIVER_KEYBOARD_H
#define DRIVER_KEYBOARD_H

#include "common/types.h"
#define KEYBOARD_BUF_SIZE 1024

typedef struct {
    uint8_t key;       // 物理物理键码 raw key
    uint8_t character; // 解析出的有效字符（如 'a', 'A', '\n'）或控制键码
    bool is_make;      // true 表示按下，false 表示松开
} keyboard_event_t;


typedef struct {
  keyboard_event_t buffer[KEYBOARD_BUF_SIZE];
  int head;
  int tail;
  int size;
} buffer_queue_t;


void init_keyboard();
int32_t read_keyboard_char_right_now(keyboard_event_t* event);
int32_t get_key_state(uint32_t key);
int32_t read_keyboard_char_wait();
#endif
