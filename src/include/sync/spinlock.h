#ifndef SYNC_SPINLOCK_H
#define SYNC_SPINLOCK_H
#include "common/types.h"

#define LOCKED_YES 1
#define LOCKED_NO 0


typedef struct spinlock {
  volatile uint32_t hold;
} spinlock_t;

void spinlock_init(spinlock_t* splock);
void spinlock_lock(spinlock_t* splock);
void spinlock_unlock(spinlock_t* splock);
void spin_lock_irqsave(spinlock_t* splock, bool* irq_status);
void spin_unlock_irqrestore(spinlock_t* splock, bool irq_status);

#endif