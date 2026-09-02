#ifndef MONARCH_KERNEL_SCHEDULER_SPINLOCK_H
#define MONARCH_KERNEL_SCHEDULER_SPINLOCK_H 1

#include "base/api/monarch.h"

struct spinlock {
    void (*lock)(struct spinlock *self);
    void (*unlock)(struct spinlock *self);
    volatile int _locked;
};

void spinlock(struct spinlock *lock);

#endif /* MONARCH_KERNEL_SCHEDULER_SPINLOCK_H */
