/**
 * @file client.c
 * @brief Private transport implementation for one Wing client object.
 */
#include "client.h"
#include "base/usr/sys.h"
#include "base/gfx/allocator.h"

private int flush(WingClient *self) {
    while (self && self->output_used) {
        long count = write(self->fd, self->output, self->output_used);
        if (count < 0 && errno == EAGAIN) return 1;
        if (count <= 0) return 0;
        self->output_used -= (size_t)count;
        if (self->output_used)
            memmove(self->output, self->output + count, self->output_used);
    }
    return 1;
}

private int send(WingClient *self, const WinMessage *message) {
    size_t needed;
    size_t capacity;
    uint8_t *buffer;
    if (!self || self->fd < 0 || !message) return 0;
    if (message->opcode == WIN_EVENT && message->event == WIN_EVENT_MOUSE &&
        self->output_used >= sizeof(*message)) {
        WinMessage *last = (WinMessage *)(self->output +
                              self->output_used - sizeof(*message));
        if (last->opcode == WIN_EVENT && last->event == WIN_EVENT_MOUSE) {
            *last = *message;
            return self->flush(self);
        }
    }
    needed = self->output_used + sizeof(*message);
    if (needed > self->output_capacity) {
        capacity = self->output_capacity ? self->output_capacity : sizeof(*message) * 4u;
        while (capacity < needed) capacity *= 2u;
        buffer = gfx_alloc(capacity);
        if (!buffer) return 0;
        if (self->output_used) memcpy(buffer, self->output, self->output_used);
        gfx_free(self->output);
        self->output = buffer;
        self->output_capacity = capacity;
    }
    memcpy(self->output + self->output_used, message, sizeof(*message));
    self->output_used += sizeof(*message);
    return self->flush(self);
}

void WingClient_init(WingClient *self, int fd) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->fd = fd;
    WinChannel_init(&self->channel, fd);
    self->send = send;
    self->flush = flush;
}
