#ifndef MONARCH_SYS_DESK_DESK_H
#define MONARCH_SYS_DESK_DESK_H 1

#include "config.h"
#include "button.h"
#include "taskbar.h"
#include "base/usr/sys.h"
#include "base/win/protocol.h"

typedef struct Desk Desk;

struct Desk {
    int fd;
    uint32_t taskbar;
    uint32_t start_button;
    uint32_t active_handle;
    uint32_t request;
    WinChannel channel;
    DeskTaskbar taskbar_model;
    DeskButton windows[DESK_WINDOW_MAX];
    WinMessage pending_events[8];
    uint32_t pending_event_count;
    uint32_t window_count;
    void (*event)(Desk *self, const WinMessage *message);
    int (*connect)(Desk *self);
    int (*run)(Desk *self);
};

void Desk_init(Desk *self);


#endif /* MONARCH_SYS_DESK_DESK_H */
