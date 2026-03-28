#ifndef INTERRUPT_TIME_H
#define INTERRUPT_TIME_H

#include <common/types.h>

// 每秒触发50次clock 中断
#define TIMER_FREQUENCY 50

void init_timer(uint32_t frequency);
uint32_t getTick();

#endif // INTERRUPT_TIME_H