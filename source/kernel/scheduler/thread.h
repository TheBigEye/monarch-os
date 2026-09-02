/**
 * @file thread.h
 * @brief Cooperative thread object and scheduler API.
 */

#ifndef MONARCH_KERNEL_SCHEDULER_THREAD_H
#define MONARCH_KERNEL_SCHEDULER_THREAD_H 1

#include "arch/x86/context.h"
#include "base/api/monarch.h"
#include "kernel/scheduler/process.h"

#define THREAD_MAX 64
#define THREAD_NAME 32
#define THREAD_STACK 32768u

typedef void (*threadentry)(void *arg);

enum threadstate {
    THREAD_UNUSED = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_ZOMBIE
};

struct thread {
    uint32_t tid;
    char name[THREAD_NAME];
    enum threadstate state;
    struct context context;
    uint8_t *stack;
    size_t stack_size;
    threadentry entry;
    void *arg;
    struct process *process;
    uint32_t wake;
    uintptr_t syscall_stack;
    int process_main;
};

void thread(struct thread *t, uint32_t tid, const char *name, threadentry entry, void *arg, struct process *process);
void threadinit(const char *name);
struct thread *threadspawn(const char *name, threadentry entry, void *arg);
struct thread *threadspawnfor(struct process *process, const char *name, threadentry entry, void *arg);
struct thread *threadcurrent(void);
void yield(void);
void threadsleep(uint32_t milliseconds);
void threadsetsysstack(uintptr_t stack);
uintptr_t threadsysstack(void);
void threadexit(void) __attribute__((noreturn));
void threadreap(struct process *process);
void threadeach(void (*iter)(void *ctx, struct thread *thread), void *ctx);
const char *threadstate(enum threadstate state);

void schedtick(void);
void schedpoll(void);
void schedpreempt(int enabled);
int schedpreempting(void);
void schedquantum(uint32_t ticks);
uint32_t schedquantumticks(void);
int schedpending(void);

#endif /* MONARCH_KERNEL_SCHEDULER_THREAD_H */
