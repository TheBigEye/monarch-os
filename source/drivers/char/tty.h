#ifndef MONARCH_DRIVERS_CHAR_TTY_H
#define MONARCH_DRIVERS_CHAR_TTY_H 1

#include "base/api/monarch.h"
#include "drivers/char/console.h"
#include "drivers/char/keyboard.h"

struct tty_editops {
    int (*history)(void *ctx, int direction, char *buffer, size_t size);
    int (*complete)(void *ctx, char *buffer, size_t size, size_t *used, size_t *cursor);
    void (*prompt)(void *ctx);
    void *ctx;
};

struct tty {
    void (*clear)(struct tty *self);
    int (*line)(struct tty *self, char *buffer, size_t size);
    int (*edit)(struct tty *self, char *buffer, size_t size, struct tty_editops *ops);
    void (*write)(struct tty *self, const char *text);
    int (*printf)(struct tty *self, const char *fmt, ...);

    struct console *_console;
    struct keyboard *_keyboard;
};

void tty(struct tty *self, struct console *screen, struct keyboard *keys);

#endif /* MONARCH_DRIVERS_CHAR_TTY_H */
