/**
 * @file wasp.c
 * @brief WASP: Window API Showcase Program.
 *
 * WASP is a client of Wing. It intentionally does not contain a second local
 * window tree: all visible objects are created through the public protocol.
 */
#include "base/usr/sys.h"
#include "base/win/protocol.h"
#include "base/win/window.h"

static void trace(const char *text) { int fd=(int)open("/dev/serial", OWRITE); if(fd>=0){write(fd,text,strlen(text));close(fd);} }

static int command(WinChannel *channel, uint32_t request, uint32_t opcode,
                   uint32_t handle, const char *text, int32_t x, int32_t y,
                   uint32_t width, uint32_t height, uint32_t flags,
                   uint32_t *reply_handle) {
    WinMessage message;
    WinMessage reply;
    struct pollfd ready;
    channel->clear(channel, &message, opcode);
    message.request = request;
    message.handle = handle;
    /* WIN_CREATE_CHILD addresses its container through the wire-level
       parent field. Keep handle populated too for uniform request helpers,
       but do not rely on it for child creation. */
    message.parent = (opcode == WIN_CREATE_CHILD) ? handle : 0u;
    message.x = x;
    message.y = y;
    message.width = width;
    message.height = height;
    message.flags = flags;
    if (text) strncpy(message.text, text, sizeof(message.text));
    if (!channel->send(channel, &message)) { trace("WASP command send fail\n"); return 0; }
    ready.fd = channel->fd;
    ready.events = POLLIN;
    ready.revents = 0;
    if (poll(&ready, 1, 1000) != 1 || !(ready.revents & POLLIN)) { trace("WASP command poll fail\n"); return 0; }
    if (!channel->recv(channel, &reply) || reply.opcode != WIN_ACK ||
        reply.request != request) {
        int serial = (int)open("/dev/serial", OWRITE);
        if (serial >= 0) {
            char info[32];
            snprintf(info, sizeof(info), "WASP ack op=%u req=%u want=%u\n", reply.opcode, reply.request, request);
            write(serial, info, strlen(info)); close(serial);
        }
        return 0;
    }
    if (reply_handle) *reply_handle = reply.handle;
    return 1;
}

static int events(WinChannel *channel, uint32_t parent) {
    WinMessage message;
    struct pollfd ready;
    for (;;) {
        ready.fd = channel->fd;
        ready.events = POLLIN;
        ready.revents = 0;
        if (poll(&ready, 1, 1000) < 0) return 0;
        if (!(ready.revents & POLLIN)) continue;
        if (!channel->recv(channel, &message)) return 0;
        if (message.opcode == WIN_EVENT &&
            message.event == WIN_EVENT_CLOSE && message.handle == parent)
            return 1;
    }
}

int main(int argc, char **argv) {
    WinChannel channel;
    WinMessage hello;
    WinMessage hello_ack;
    struct sockaddr_un address;
    uint32_t request = WIN_REQUEST_FIRST;
    uint32_t parent;
    uint32_t child_two;
    uint32_t child_three;
    int fd = -1;
    unused(argc);
    unused(argv);

    memset(&address, 0, sizeof(address));
    address.family = AF_UNIX;
    strcpy(address.path, "/tmp/wing.sock");
    for (unsigned attempt = 0; attempt < 1000u && fd < 0; attempt++) {
        fd = (int)socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd >= 0) {
            fcntl(fd, F_SETFL, O_NONBLOCK);
            if (connect(fd, &address) != 0) { close(fd); fd = -1; }
        }
        if (fd < 0) sleepms(10);
    }
    if (fd < 0) return 1;

    WinChannel_init(&channel, fd);
    channel.clear(&channel, &hello, WIN_HELLO);
    hello.request = WIN_REQUEST_HELLO;
    if (!channel.send(&channel, &hello)) goto fail;
    fcntl(fd, F_SETFL, 0);
    if (!channel.recv(&channel, &hello_ack) || hello_ack.opcode != WIN_ACK) goto fail;
    if (!command(&channel, request++, WIN_CREATE, 0, "WASP Demo",
                 120, 90, 520, 340, 0, &parent)) goto fail;
    if (!command(&channel, request++, WIN_CREATE_CHILD, parent, "Child One",
                 145, 135, 220, 120, 0, nil)) goto fail;
    if (!command(&channel, request++, WIN_CREATE_CHILD, parent, "Child Two",
                 380, 135, 220, 120, 0, &child_two)) goto fail;
    if (!command(&channel, request++, WIN_CREATE_CHILD, parent, "Child Three",
                 260, 275, 220, 120, 0, &child_three)) goto fail;
    command(&channel, request++, WIN_SET_FLAGS, child_two, nil, 0, 0, 0, 0,
            WINDOW_CONSTRAINED, nil);
    command(&channel, request++, WIN_SET_FLAGS, child_three, nil, 0, 0, 0, 0,
            WINDOW_NO_TITLE | WINDOW_NO_BUTTONS, nil);
    command(&channel, request++, WIN_SET_CONTENT, parent,
            "Remote Window API showcase", 35, 75, 0, 0, 0, nil);
    command(&channel, request++, WIN_SET_CONTENT, child_three,
            "WASP log\nparent created\nchildren ready", 12, 12, 0, 0, 0, nil);
    command(&channel, request++, WIN_SET_TITLE, parent,
            "WASP Demo", 0, 0, 0, 0, 0, nil);
    command(&channel, request++, WIN_RESIZE, child_three, nil,
            260, 275, 240, 130, 0, nil);
    events(&channel, parent);

fail:
    close(fd);
    return 1;
}
