/**
 * @file protocol.c
 * @brief Object-style channel methods for the fixed window wire message.
 */
#include "protocol.h"

static int transfer(int fd, void *buffer, size_t size, int writing) {
    size_t done = 0;
    while (done < size) {
        long count = writing ? write(fd, (const uint8_t *)buffer + done, size - done)
                              : read(fd, (uint8_t *)buffer + done, size - done);
        if (count > 0) {
            done += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EAGAIN) {
            struct pollfd ready;
            ready.fd = fd;
            ready.events = writing ? POLLOUT : POLLIN;
            ready.revents = 0;
            if (poll(&ready, 1, 1000) == 1 &&
                (ready.revents & (writing ? POLLOUT : POLLIN)))
                continue;
        }
        return 0;
    }
    return 1;
}

static void clear(WinChannel *self, WinMessage *message, uint32_t opcode) {
    unused(self);
    if (!message) return;
    memset(message, 0, sizeof(*message));
    message->version = WIN_PROTOCOL_VERSION;
    message->opcode = opcode;
    message->size = sizeof(*message);
}

static int send(WinChannel *self, const WinMessage *message) {
    if (!self || self->fd < 0 || !message || message->size != sizeof(*message)) return 0;
    return transfer(self->fd, (void *)message, sizeof(*message), 1);
}

static int recv(WinChannel *self, WinMessage *message) {
    if (!self || self->fd < 0 || !message) return 0;
    if (!transfer(self->fd, message, sizeof(*message), 0)) return 0;
    return message->version == WIN_PROTOCOL_VERSION &&
           message->size == sizeof(*message);
}

void WinChannel_init(WinChannel *channel, int fd) {
    if (!channel) return;
    memset(channel, 0, sizeof(*channel));
    channel->fd = fd;
    channel->clear = clear;
    channel->send = send;
    channel->recv = recv;
}
