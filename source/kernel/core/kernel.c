#include "kernel/core/kernel.h"
#include "kernel/core/init.h"
#include "arch/x86/cpu.h"
#include "kernel/shell/bush.h"
#include "kernel/scheduler/thread.h"

struct kernel monarch;

static struct bush shell;

static void stop(struct kernel *self) __attribute__((noreturn));

static void shellmain(void *arg) {
    struct kernel *self = arg;

    /* The recovery shell is now the normal boot interface. The graphical
       desktop is an explicit userspace session started with `wing`. */
    self->tty.write(&self->tty, "Monarch: entering Bush shell\n");
    bush(&shell, &self->tty, &self->fs, &self->speaker, &self->ac97, &self->floppy);
    shell.run(&shell);
}

static void run(struct kernel *self) {
    if (!threadspawn("bush", shellmain, self)) {
        self->tty.write(&self->tty, "cannot start shell thread\n");
        self->stop(self);
    }

    /* Keep the boot thread as a tiny idle thread. The interactive shell runs
       on a normal scheduler-allocated stack, not on the bootstrap stack. */
    for (;;) {
        yield();
        __asm__ volatile ("hlt");
    }
}

static void stop(struct kernel *self) {
    unused(self);
    halt();
}

void kernel(uint32_t magic, uintptr_t info) {
    memset(&monarch, 0, sizeof(monarch));
    monarch.run = run;
    monarch.stop = stop;
    init(&monarch, magic, info);
    monarch.run(&monarch);
    monarch.stop(&monarch);
}
