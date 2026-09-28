#ifndef MONARCH_SYS_WING_SERVER_H
#define MONARCH_SYS_WING_SERVER_H 1

#include "apps/sys/wing/config.h"
#include "apps/sys/wing/client.h"
#include "base/win/manager.h"

typedef struct WingServer WingServer;

struct WingServer {
    int fd;
    WingClient *clients;
    WindowManager *manager;
    struct dirty *dirty;

    int (*open)(struct WingServer *self);
    int (*poll)(struct WingServer *self);
    void (*dispatch)(struct WingServer *self, WingClient *client, const WinMessage *message);
    Window *(*create)(struct WingServer *self, const WinMessage *message, WingClient *client);
    int (*apply)(struct WingServer *self, const WinMessage *message, WingClient *client);
    void (*evict)(struct WingServer *self, WingClient *client);
    void (*drop)(struct WingServer *self, WingClient *client);
    void (*prune)(struct WingServer *self, Window *parent);
    void (*emit)(struct WingServer *self, Window *target, uint32_t event,
                 int32_t x, int32_t y, int32_t dx, int32_t dy, uint32_t value);
    void (*notify)(struct WingServer *self, Window *window, uint32_t event);
    void (*log)(struct WingServer *self, const char *reason);
};

void WingServer_init(struct WingServer *self, WingClient *clients,
                     WindowManager *manager, struct dirty *dirty);

#endif /* MONARCH_SYS_WING_SERVER_H */
