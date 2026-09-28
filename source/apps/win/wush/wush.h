#ifndef MONARCH_APPS_WIN_WUSH_WUSH_H
#define MONARCH_APPS_WIN_WUSH_WUSH_H 1

#include "base/usr/sys.h"
#include "base/win/protocol.h"

struct Wush;
typedef struct Wush Wush;

struct Wush {
    struct sockaddr_un address;
    WinMessage message;
    WinChannel channel;
    int wing;
    uint32_t handle;

    int (*connect)(Wush *self);
    int (*create)(Wush *self);
    int (*run)(Wush *self);
};

void Wush_init(Wush *self);

#endif /* MONARCH_APPS_WIN_WUSH_WUSH_H */
