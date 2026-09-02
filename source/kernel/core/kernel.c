#include "kernel/core/kernel.h"
#include "kernel/core/init.h"
#include "kernel/core/exec.h"
#include "kernel/scheduler/process.h"
#include "arch/x86/pit.h"
#include "kernel/shell/bush.h"
#include "arch/x86/cpu.h"
#include "kernel/scheduler/thread.h"

struct kernel monarch;

static struct bush shell;

static void stop(struct kernel *self) __attribute__((noreturn));

static int startup_shift(struct kernel *self) {
    uint32_t start = ticks();
    if (!self) return 0;
    /* Give the operator a short pre-init window to hold either Shift key. */
    while (ticks() - start < 50u) {
        if (self->keyboard._shift) return 1;
        threadsleep(1);
    }
    return self->keyboard._shift != 0;
}

static void shellmain(void *arg) {
    struct kernel *self = arg;
    struct execresult result;
    int status;
    
    if (startup_shift(self)) {
        self->tty.write(&self->tty, "SPARK: SHIFT held, entering Bush debug shell\n");
        bush(&shell, &self->tty, &self->fs, &self->speaker, &self->ac97, &self->floppy);
        shell.run(&shell);
        return;
    }

    self->tty.write(&self->tty, "SPARK: starting userspace init\n");
    if (!execspawn(&self->fs, "/initrd/sys/bin/spark.elf",
                   processcurrent(), &result)) {
        self->tty.write(&self->tty, "SPARK: launch failed, falling back to Bush\n");
        bush(&shell, &self->tty, &self->fs, &self->speaker, &self->ac97, &self->floppy);
        shell.run(&shell);
        return;
    }
    processwait((int)result.pid, &status);
    self->tty.write(&self->tty, "SPARK: userspace init stopped, entering Bush\n");
    bush(&shell, &self->tty, &self->fs, &self->speaker, &self->ac97, &self->floppy);
    shell.run(&shell);
}

static void run(struct kernel *self) {
    if (!threadspawn("bush", shellmain, self)) {
        self->tty.write(&self->tty, "cannot start shell thread\n");
        self->stop(self);
    }

    /*
     * Keep the boot thread as a tiny idle thread.  The interactive shell runs
     * on a normal scheduler-allocated stack, not on the limited bootstrap stack.
     */
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
