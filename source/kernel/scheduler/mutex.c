#include "kernel/scheduler/mutex.h"

static void lock_impl(struct mutex *self) {
    self->_guard.lock(&self->_guard);
}

static void unlock_impl(struct mutex *self) {
    self->_guard.unlock(&self->_guard);
}

void mutex(struct mutex *m) {
    spinlock(&m->_guard);
    m->lock = lock_impl;
    m->unlock = unlock_impl;
}
