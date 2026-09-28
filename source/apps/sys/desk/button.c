/**
 * @file button.c
 * @brief Remote taskbar button object used by Desk.
 */
#include "button.h"
#include "config.h"

static void move_button(DeskButton *self, int32_t x) {
    WinMessage message;
    if (!self || !self->channel || !self->request || !self->button) return;
    self->channel->clear(self->channel, &message, WIN_MOVE);
    message.request = (*self->request)++;
    message.handle = self->button;
    message.x = x;
    message.y = DESK_TASKBAR_Y + 4;
    self->channel->send(self->channel, &message);
}

void DeskButton_init(DeskButton *self, WinChannel *channel, uint32_t *request) {
    if (!self) return;
    self->channel = channel;
    self->request = request;
    self->move = move_button;
}
