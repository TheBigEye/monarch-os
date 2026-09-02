/**
 * @file tty.c
 * @brief TTY adapter with editable input lines over the keyboard and console.
 *
 */

#include "drivers/char/tty.h"

static void clear(struct tty *self) {
    self->_console->clear(self->_console);
}

static void redraw(struct tty *self, char *buffer, size_t used, size_t cursor, size_t old_used, unsigned row, unsigned column) {
    size_t clear = max(used, old_used);

    self->_console->move(self->_console, row, column);
    for (size_t i = 0; i < used; i++) {
        self->_console->put(self->_console, buffer[i]);
    }
    for (size_t i = used; i <= clear; i++) {
        self->_console->put(self->_console, ' ');
    }
    self->_console->move(self->_console, row, column + (unsigned)cursor);
}

/*
 * Read one editable line from the keyboard.
 *
 * The basic line() entry point below calls this with no callbacks.  Shells can
 * pass a tiny callback table to hook Unix-like extras such as Up/Down history
 * and Tab completion without teaching the keyboard driver about commands,
 * paths, or filesystems.
 */
static int edit(struct tty *self, char *buffer, size_t size, struct tty_editops *ops) {
    size_t used = 0;
    size_t cursor = 0;
    unsigned row;
    unsigned column;

    if (!size) {
        return 0;
    }

    buffer[0] = '\0';
    row = self->_console->_row;
    column = self->_console->_column;

    for (;;) {
        struct keyevent event;
        console_tick();
        event = self->_keyboard->event(self->_keyboard);

        if (event.code == KEY_LEFT) {
            if (cursor > 0) {
                cursor--;
                self->_console->move(self->_console, row, column + (unsigned)cursor);
            }
            continue;
        }
        if (event.code == KEY_RIGHT) {
            if (cursor < used) {
                cursor++;
                self->_console->move(self->_console, row, column + (unsigned)cursor);
            }
            continue;
        }
        if (event.code == KEY_HOME) {
            cursor = 0;
            self->_console->move(self->_console, row, column);
            continue;
        }
        if (event.code == KEY_END) {
            cursor = used;
            self->_console->move(self->_console, row, column + (unsigned)cursor);
            continue;
        }
        if (event.code == KEY_UP || event.code == KEY_DOWN) {
            if (ops && ops->history) {
                size_t old_used = used;
                if (ops->history(ops->ctx, event.code == KEY_UP ? -1 : 1, buffer, size)) {
                    used = strlen(buffer);
                    cursor = used;
                    redraw(self, buffer, used, cursor, old_used, row, column);
                }
            }
            continue;
        }
        if (event.code == KEY_DELETE) {
            if (cursor < used) {
                size_t old_used = used;
                memmove(buffer + cursor, buffer + cursor + 1u, used - cursor - 1u);
                used--;
                buffer[used] = '\0';
                redraw(self, buffer, used, cursor, old_used, row, column);
            }
            continue;
        }

        if (!event.pressed || !event.ascii) {
            continue;
        }

        if (event.ascii == '\t') {
            if (ops && ops->complete) {
                size_t old_used = used;
                int action = ops->complete(ops->ctx, buffer, size, &used, &cursor);
                if (action == 1) {
                    redraw(self, buffer, used, cursor, old_used, row, column);
                } else if (action == 2 && ops->prompt) {
                    ops->prompt(ops->ctx);
                    row = self->_console->_row;
                    column = self->_console->_column;
                    for (size_t i = 0; i < used; i++) {
                        self->_console->put(self->_console, buffer[i]);
                    }
                    self->_console->move(self->_console, row, column + (unsigned)cursor);
                }
            }
            continue;
        }

        if (event.ascii == '\n' || event.ascii == '\r') {
            buffer[used] = '\0';
            self->_console->move(self->_console, row, column + (unsigned)used);
            self->_console->put(self->_console, '\n');
            return (int)used;
        }

        if (event.ascii == '\b') {
            if (cursor > 0) {
                size_t old_used = used;
                memmove(buffer + cursor - 1u, buffer + cursor, used - cursor);
                cursor--;
                used--;
                buffer[used] = '\0';
                redraw(self, buffer, used, cursor, old_used, row, column);
            }
            continue;
        }

        if (event.ascii >= ' ' && used + 1u < size && column + used + 1u < self->_console->_columns) {
            size_t old_used = used;
            memmove(buffer + cursor + 1u, buffer + cursor, used - cursor);
            buffer[cursor++] = event.ascii;
            used++;
            buffer[used] = '\0';
            redraw(self, buffer, used, cursor, old_used, row, column);
        }
    }
}

static int line(struct tty *self, char *buffer, size_t size) {
    return edit(self, buffer, size, nil);
}

static void write(struct tty *self, const char *text) {
    self->_console->write(self->_console, text);
}

static void putctx(void *ctx, char ch) {
    struct tty *self = ctx;
    self->_console->put(self->_console, ch);
}

static int print(struct tty *self, const char *fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = kvformat(putctx, self, fmt, ap);
    va_end(ap);
    return result;
}

void tty(struct tty *self, struct console *screen, struct keyboard *keys) {
    memset(self, 0, sizeof(*self));
    self->clear = clear;
    self->line = line;
    self->edit = edit;
    self->write = write;
    self->printf = print;
    self->_console = screen;
    self->_keyboard = keys;
}
