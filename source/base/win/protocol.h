#ifndef MONARCH_BASE_WIN_PROTOCOL_H
#define MONARCH_BASE_WIN_PROTOCOL_H 1

#include "base/usr/sys.h"

#define WIN_PROTOCOL_VERSION 1u
#define WIN_PROTOCOL_TEXT 256u
#define WIN_CLIENT_TERMINAL 0x0001u
#define WIN_CLIENT_DESKTOP  0x0002u
#define WIN_REQUEST_HELLO 1u
#define WIN_REQUEST_FIRST 2u

/* Request IDs are opaque client-owned values. They are not child indexes. */

/* Numeric wire data is kept separate from the object methods. */
typedef struct WinMessage WinMessage;

struct WinMessage {
    uint32_t version;
    uint32_t opcode;
    uint32_t size;
    uint32_t request;
    uint32_t handle;
    uint32_t parent;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    uint32_t color;
    uint32_t event;
    uint32_t value;
    char text[WIN_PROTOCOL_TEXT];
};

typedef struct WinChannel WinChannel;

struct WinChannel {
    int fd;
    void (*clear)(WinChannel *self, WinMessage *message, uint32_t opcode);
    int (*send)(WinChannel *self, const WinMessage *message);
    int (*recv)(WinChannel *self, WinMessage *message);
};

void WinChannel_init(WinChannel *channel, int fd);

/* Wire opcodes. WinMessage remains value-only so it is safe to serialize. */
enum win_opcode {
    WIN_HELLO = 1u, WIN_CREATE = 2u, WIN_CREATE_CHILD = 3u,
    WIN_DESTROY = 4u, WIN_MOVE = 5u, WIN_RESIZE = 6u,
    WIN_SHOW = 7u, WIN_HIDE = 8u, WIN_SET_TITLE = 9u,
    WIN_SET_CONTENT = 10u, WIN_SET_FLAGS = 11u, WIN_FOCUS = 12u,
    WIN_EVENT = 13u, WIN_ACK = 14u, WIN_ERROR = 15u,
    WIN_TERMINAL_OPEN = 16u, WIN_TERMINAL_DATA = 17u,
    WIN_TERMINAL_KEY = 18u, WIN_TERMINAL_RESIZE = 19u,
    WIN_TERMINAL_CLOSE = 20u,
    WIN_MINIMIZE = 21u,
    WIN_MAXIMIZE = 22u,
    WIN_RESTORE = 23u
};

enum win_event_code {
    WIN_EVENT_MOUSE = 1u, WIN_EVENT_KEY = 2u, WIN_EVENT_CLOSE = 3u,
    WIN_EVENT_FOCUS = 4u, WIN_EVENT_BLUR = 5u, WIN_EVENT_RESIZE = 6u,
    WIN_EVENT_MOVE = 7u,
    WIN_EVENT_WINDOW_CREATED = 8u,
    WIN_EVENT_WINDOW_DESTROYED = 9u,
    WIN_EVENT_WINDOW_STATE = 10u
};

#endif /* MONARCH_BASE_WIN_PROTOCOL_H */
