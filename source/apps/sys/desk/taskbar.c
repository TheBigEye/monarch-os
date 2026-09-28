/**
 * @file taskbar.c
 * @brief Taskbar object and its remote button operations.
 */
#include "taskbar.h"
#include "base/win/window.h"

static int wait_ack(DeskTaskbar *self, uint32_t request, WinMessage *reply) {
    for (;;) {
        struct pollfd ready;
        ready.fd = self->fd;
        ready.events = POLLIN;
        ready.revents = 0;
        if (poll(&ready, 1, 1000) != 1) return 0;
        if (!self->channel->recv(self->channel, reply)) return 0;
        if (reply->opcode == WIN_ACK && reply->request == request) return 1;
        /* Do not recurse into Desk while waiting. Queue application events
           and let Desk drain them from its outer event loop. */
        if (reply->opcode == WIN_EVENT && reply->parent != self->handle &&
            self->queue_event)
            self->queue_event(self->event_context, reply);
    }
}

static int create_button(DeskTaskbar *self, const char *title,
                         int32_t x, uint32_t width, uint32_t *handle) {
    WinMessage message;
    WinMessage reply;
    uint32_t request;
    if (!self || !self->channel || !self->request || !title || !handle) return 0;
    self->channel->clear(self->channel, &message, WIN_CREATE_CHILD);
    request = (*self->request)++;
    message.request = request;
    message.parent = self->handle;
    message.x = x;
    message.y = DESK_TASKBAR_Y + 4;
    message.width = width;
    message.height = DESK_BUTTON_HEIGHT;
    message.color = 0x808080u;
    message.flags = WINDOW_NO_TITLE | WINDOW_NO_BUTTONS |
                    WINDOW_NO_MOVE | WINDOW_NO_RESIZE |
                    WINDOW_ALWAYS_ON_TOP;
    strncpy(message.text, title, sizeof(message.text));
    if (!self->channel->send(self->channel, &message) ||
        !wait_ack(self, request, &reply)) return 0;
    *handle = reply.handle;
    self->channel->clear(self->channel, &message, WIN_SET_CONTENT);
    request = (*self->request)++;
    message.request = request;
    message.handle = *handle;
    message.x = 8;
    message.y = 6;
    strncpy(message.text, title, sizeof(message.text));
    return self->channel->send(self->channel, &message) &&
           wait_ack(self, request, &reply);
}

static void refresh(DeskTaskbar *self) {
    WinMessage message;
    if (!self || !self->channel || !self->request || !self->handle) return;
    self->channel->clear(self->channel, &message, WIN_SET_CONTENT);
    message.request = (*self->request)++;
    message.handle = self->handle;
    message.text[0] = '\0';
    self->channel->send(self->channel, &message);
}

void DeskTaskbar_init(DeskTaskbar *self, WinChannel *channel,
                      uint32_t *request, int fd) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->channel = channel;
    self->request = request;
    self->fd = fd;
    self->refresh = refresh;
    self->create_button = create_button;
}
