#include "kernel/scheduler/spinlock.h"
#include "arch/x86/cpu.h"

static void lock_impl(struct spinlock *self) {
    while (__sync_lock_test_and_set(&self->_locked, 1)) {
        pause();
    }
}

static void unlock_impl(struct spinlock *self) {
    __sync_lock_release(&self->_locked);
}

void spinlock(struct spinlock *lock) {
    lock->lock = lock_impl;
    lock->unlock = unlock_impl;
    lock->_locked = 0;
}
