#ifndef MONARCH_KERNEL_SCHEDULER_MUTEX_H
#define MONARCH_KERNEL_SCHEDULER_MUTEX_H 1

#include "kernel/scheduler/spinlock.h"

struct mutex {
    void (*lock)(struct mutex *self);
    void (*unlock)(struct mutex *self);
    struct spinlock _guard;
};

void mutex(struct mutex *m);

#endif /* MONARCH_KERNEL_SCHEDULER_MUTEX_H */
