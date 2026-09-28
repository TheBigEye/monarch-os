#ifndef MONARCH_SYS_DESK_TASKBAR_H
#define MONARCH_SYS_DESK_TASKBAR_H 1

#include "config.h"
#include "base/win/protocol.h"

typedef struct DeskTaskbar DeskTaskbar;
typedef void (*DeskTaskbarQueue)(void *context, const WinMessage *message);

struct DeskTaskbar {
    uint32_t handle;
    uint32_t start_button;
    uint32_t *request;
    WinChannel *channel;
    int fd;
    void *event_context;
    DeskTaskbarQueue queue_event;
    void (*refresh)(DeskTaskbar *self);
    int (*create_button)(DeskTaskbar *self, const char *title,
                         int32_t x, uint32_t width, uint32_t *handle);
};

void DeskTaskbar_init(DeskTaskbar *self, WinChannel *channel,
                      uint32_t *request, int fd);

#endif /* MONARCH_SYS_DESK_TASKBAR_H */
