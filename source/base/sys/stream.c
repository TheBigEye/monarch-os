/**
 * @file stream.c
 * @brief Generic kernel-side character stream adapter.
 */

#include "base/sys/stream.h"

static void put(struct stream *self, char ch) {
    self->_put(self->_target, ch);
}

static void write(struct stream *self, const char *text) {
    while (*text) {
        self->put(self, *text++);
    }
}

static void putctx(void *ctx, char ch) {
    struct stream *self = ctx;
    self->put(self, ch);
}

static int print(struct stream *self, const char *fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = kvformat(putctx, self, fmt, ap);
    va_end(ap);
    return result;
}

void stream(struct stream *self, void *target, void (*putfn)(void *target, char ch)) {
    memset(self, 0, sizeof(*self));
    self->put = put;
    self->write = write;
    self->printf = print;
    self->_target = target;
    self->_put = putfn;
}
