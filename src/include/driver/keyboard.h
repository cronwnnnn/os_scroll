#ifndef DRIVER_KEYBOARD_H
#define DRIVER_KEYBOARD_H

#include "common/types.h"
#define KEYBOARD_BUF_SIZE 1024
typedef struct {
  uint8_t buffer[KEYBOARD_BUF_SIZE];
  int head;
  int tail;
  int size;
} buffer_queue_t;

void init_keyboard();
int32_t read_keyboard_char();
#endif
