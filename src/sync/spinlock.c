#include "sync/spinlock.h"
#include "task/scheduler.h"

extern uint32_t atomic_exchange(volatile uint32_t* dst, uint32_t src);
extern uint32_t get_eflags();


void spinlock_init(spinlock_t* splock){
    splock->hold = LOCKED_NO;
}

void spinlock_lock(spinlock_t* splock){
    // 直接不允许切换线程了？？
    disable_preempt();

    #ifndef SINGLE_PROCESSOR
    while (atomic_exchange(&splock->hold , LOCKED_YES) != LOCKED_NO) {}
    #endif
}

void spinlock_unlock(spinlock_t* splock){
    #ifndef SINGLE_PROCESSOR
    __asm__ __volatile__("":::"memory");
    splock->hold = LOCKED_NO;
    #endif

    enable_preempt();
}

void spin_lock_irqsave(spinlock_t* splock, bool* irq_status){
    disable_preempt();

    // Now disable local interrupt and save interrupt flag bit.
    uint32_t eflags = get_eflags();
    disable_interrupt();
    *irq_status = ((eflags & (1 << 9)) != 0);

    // For multi-processor, competing for CAS is still needed.
    #ifndef SINGLE_PROCESSOR
    while (atomic_exchange(&splock->hold , LOCKED_YES) != LOCKED_NO) {}
    #endif

}


void spin_unlock_irqrestore(spinlock_t* splock, bool irq_status){
    #ifndef SINGLE_PROCESSOR
    __asm__ __volatile__("":::"memory");
    splock->hold = LOCKED_NO;
    #endif


    // Restore interrupt flag bit - If it's previously enabled before locking, re-enable it again.
    if (irq_status) {
        enable_interrupt();
    }

    enable_preempt();

}
