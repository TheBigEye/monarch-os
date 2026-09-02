/**
 * @file thread.c
 * @brief Cooperative kernel thread scheduler and soft-preemption hooks.
 *
 */

#include "kernel/scheduler/thread.h"
#include "arch/x86/cpu.h"
#include "arch/x86/gdt.h"
#include "arch/x86/pit.h"
#include "kernel/core/debug.h"
#include "kernel/memory/heap.h"

static struct thread table[THREAD_MAX];
static struct thread *current;
static uint32_t nexttid = 1;

static volatile int preempt = 1;
static volatile int need = 0;
static uint32_t quantum = 5;
static uint32_t slice = 0;

static uintptr_t kstacktop(struct thread *t) {
    return t && t->stack ? (uintptr_t)t->stack + t->stack_size : 0;
}

static void boot(void) {
    if (current && current->entry) {
        current->entry(current->arg);
    }
    threadexit();
}

void thread(struct thread *t, uint32_t tid, const char *name, threadentry entry, void *arg, struct process *process) {
    memset(t, 0, sizeof(*t));
    t->tid = tid;
    t->state = THREAD_READY;
    t->entry = entry;
    t->arg = arg;
    t->process = process;
    strncpy(t->name, name ? name : "thread", sizeof(t->name));
}

void threadinit(const char *name) {
    thread(&table[0], nexttid++, name ? name : "main", nil, nil, processcurrent());
    table[0].state = THREAD_RUNNING;
    current = &table[0];
}

static struct thread *spawn(struct process *owner, const char *name, threadentry entry, void *arg, int process_main) {
    threadreap(nil);

    for (unsigned i = 0; i < THREAD_MAX; i++) {
        if (table[i].state == THREAD_UNUSED) {
            uintptr_t top;
            thread(&table[i], nexttid++, name ? name : "thread", entry, arg, owner);
            table[i].process_main = process_main;
            table[i].stack_size = THREAD_STACK;
            table[i].stack = kmalloc(table[i].stack_size);
            if (!table[i].stack) {
                memset(&table[i], 0, sizeof(table[i]));
                return nil;
            }
            top = ((uintptr_t)table[i].stack + table[i].stack_size) & ~(uintptr_t)0xFu;
            context(&table[i].context, top, (uintptr_t)boot);
            if (preempt) {
                need = 1;
            }
            return &table[i];
        }
    }

    return nil;
}

struct thread *threadspawn(const char *name, threadentry entry, void *arg) {
    return spawn(processcurrent(), name, entry, arg, 0);
}

struct thread *threadspawnfor(struct process *owner, const char *name, threadentry entry, void *arg) {
    return spawn(owner, name, entry, arg, 1);
}

struct thread *threadcurrent(void) {
    return current;
}

static int ready(void) {
    for (unsigned i = 0; i < THREAD_MAX; i++) {
        if (table[i].state == THREAD_READY) {
            return 1;
        }
    }
    return 0;
}

static int wake(void) {
    uint32_t now = ticks();
    int woke = 0;

    for (unsigned i = 0; i < THREAD_MAX; i++) {
        if (table[i].state == THREAD_SLEEPING && (int32_t)(now - table[i].wake) >= 0) {
            table[i].state = THREAD_READY;
            table[i].wake = 0;
            if (table[i].process && table[i].process->state == PROCESS_SLEEPING) {
                processsetstate(table[i].process, PROCESS_READY);
            }
            woke = 1;
        }
    }

    return woke;
}

static struct thread *next(void) {
    unsigned start;

    if (!current) {
        return nil;
    }

    if (wake()) {
        need = 1;
    }

    start = (unsigned)(current - table);
    for (unsigned n = 1; n <= THREAD_MAX; n++) {
        unsigned i = (start + n) % THREAD_MAX;
        if (table[i].state == THREAD_READY) {
            return &table[i];
        }
    }

    return current;
}

void yield(void) {
    struct thread *old = current;
    struct thread *newthread;

    if (!old) {
        return;
    }

    need = 0;
    slice = 0;

    newthread = next();
    if (!newthread || newthread == old) {
        return;
    }

    KSCHED("switch tid=%u pid=%u state=%s -> tid=%u pid=%u state=%s",
        old->tid,
        old->process ? old->process->pid : 0,
        threadstate(old->state),
        newthread->tid,
        newthread->process ? newthread->process->pid : 0,
        threadstate(newthread->state));

    if (old->state == THREAD_RUNNING) {
        old->state = THREAD_READY;
    }

    current = newthread;
    current->state = THREAD_RUNNING;
    processswitch(current->process);
    tssesp0(kstacktop(current));

    contextswitch(&old->context.sp, current->context.sp);
}

void threadsleep(uint32_t milliseconds) {
    uint32_t hz;
    uint32_t amount;

    if (!current || milliseconds == 0) {
        yield();
        return;
    }

    hz = tickhz();
    amount = (milliseconds * hz + 999u) / 1000u;
    if (!amount) {
        amount = 1;
    }

    current->wake = ticks() + amount;
    current->state = THREAD_SLEEPING;
    if (current->process && current->process->state == PROCESS_RUNNING) {
        processsetstate(current->process, PROCESS_SLEEPING);
    }
    need = 1;

    while (current->state == THREAD_SLEEPING) {
        yield();
        __asm__ volatile ("hlt");
        if (wake()) {
            need = 1;
        }
    }
}

void threadsetsysstack(uintptr_t stack) {
    if (current) {
        current->syscall_stack = stack;
    }
}

uintptr_t threadsysstack(void) {
    return current ? current->syscall_stack : 0;
}

void threadexit(void) {
    disable();
    if (current) {
        KLOG("thread", "exit tid=%u pid=%u main=%d name=%s",
            current->tid,
            current->process ? current->process->pid : 0,
            current->process_main,
            current->name);
        current->state = THREAD_ZOMBIE;
        if (current->process_main && current->process) {
            processfinish(current->process, current->process->exit_code);
        }
    }
    need = 1;
    enable();

    for (;;) {
        if (current) {
            KTRACE("thread", "zombie tid=%u yielding", current->tid);
        }
        yield();
        __asm__ volatile ("hlt");
    }
}

void threadreap(struct process *process) {
    for (unsigned i = 0; i < THREAD_MAX; i++) {
        if (table[i].state == THREAD_ZOMBIE && (!process || table[i].process == process) && &table[i] != current) {
            KLOG("thread", "reap tid=%u pid=%u name=%s",
                table[i].tid,
                table[i].process ? table[i].process->pid : 0,
                table[i].name);
            if (table[i].stack) {
                kfree(table[i].stack);
            }
            memset(&table[i], 0, sizeof(table[i]));
        }
    }
}

void threadeach(void (*iter)(void *ctx, struct thread *thread), void *ctx) {
    if (!iter) {
        return;
    }

    for (unsigned i = 0; i < THREAD_MAX; i++) {
        if (table[i].state != THREAD_UNUSED) {
            iter(ctx, &table[i]);
        }
    }
}

const char *threadstate(enum threadstate state) {
    switch (state) {
        case THREAD_READY: return "ready";
        case THREAD_RUNNING: return "running";
        case THREAD_SLEEPING: return "sleeping";
        case THREAD_ZOMBIE: return "zombie";
        default: return "unused";
    }
}

void schedtick(void) {
    int woke = wake();

    if (!preempt || !current || !ready()) {
        return;
    }

    if (woke) {
        need = 1;
    }

    slice++;
    if (slice >= quantum) {
        slice = 0;
        need = 1;
    }
}

void schedpoll(void) {
    if (preempt && need && ready()) {
        yield();
    }
}

void schedpreempt(int enabled) {
    preempt = enabled ? 1 : 0;
    slice = 0;
    need = 0;
}

int schedpreempting(void) {
    return preempt;
}

void schedquantum(uint32_t ticks) {
    quantum = ticks ? ticks : 1;
    slice = 0;
}

uint32_t schedquantumticks(void) {
    return quantum;
}

int schedpending(void) {
    return need;
}
