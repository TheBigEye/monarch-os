#include "kernel/core/panic.h"
#include "kernel/core/kernel.h"
#include "arch/x86/cpu.h"

static void putctx(void *ctx, char ch) {
    struct console *screen = ctx;
    screen->put(screen, ch);
}

void panic(const char *fmt, ...) {
    va_list ap;
    disable();
    monarch.console.color(&monarch.console, VGA_WHITE, VGA_RED);
    monarch.console.write(&monarch.console, "\n\nKERNEL PANIC: ");
    va_start(ap, fmt);
    kvformat(putctx, &monarch.console, fmt, ap);
    va_end(ap);
    monarch.console.write(&monarch.console, "\nSystem halted.\n");
    halt();
}
