#ifndef MONARCH_SYS_DESK_BUTTON_H
#define MONARCH_SYS_DESK_BUTTON_H 1

#include "base/win/protocol.h"

typedef struct DeskButton DeskButton;

/* Local model for one remote taskbar button. The protocol references are
   borrowed from Desk and remain valid for the lifetime of the session. */
struct DeskButton {
    uint32_t handle;
    uint32_t button;
    uint32_t state;
    char title[WIN_PROTOCOL_TEXT];
    WinChannel *channel;
    uint32_t *request;
    void (*move)(DeskButton *self, int32_t x);
};

void DeskButton_init(DeskButton *self, WinChannel *channel, uint32_t *request);

#endif /* MONARCH_SYS_DESK_BUTTON_H */
