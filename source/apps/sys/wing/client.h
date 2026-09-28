#ifndef MONARCH_SYS_WING_CLIENT_H
#define MONARCH_SYS_WING_CLIENT_H 1

#include "base/usr/sys.h"
#include "base/win/protocol.h"

#define WING_CLIENT_INPUT_SIZE sizeof(WinMessage)

typedef struct WingClient WingClient;

typedef int (*WingClientSend)(WingClient *self, const WinMessage *message);
typedef int (*WingClientFlush)(WingClient *self);

/* A WingClient owns one remote socket and its framed input/output buffers. */
struct WingClient {
    int fd;
    size_t used;
    uint8_t buffer[WING_CLIENT_INPUT_SIZE];
    uint8_t *output;
    size_t output_used;
    size_t output_capacity;
    uint32_t flags;
    WinChannel channel;
    WingClientSend send;
    WingClientFlush flush;
};

void WingClient_init(WingClient *self, int fd);

#endif /* MONARCH_SYS_WING_CLIENT_H */
