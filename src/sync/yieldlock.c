#include "sync/yieldlock.h"
#include "task/scheduler.h"

extern uint32_t atomic_exchange(volatile uint32_t* dst, uint32_t src);
void yieldlock_init(yieldlock_t* splock){
    splock->hold = LOCKED_NO;
}

void yieldlock_lock(yieldlock_t* splock){
    while(atomic_exchange(&splock->hold, LOCKED_YES) != LOCKED_NO){
        schedule_thread_yield();
    }
}
bool yieldlock_trylock(yieldlock_t* splock){
    return atomic_exchange(&splock->hold, LOCKED_YES) == LOCKED_NO;
}
void yieldlock_unlock(yieldlock_t* splock){
    splock->hold = LOCKED_NO;
}