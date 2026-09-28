/**
 * @file desk.c
 * @brief Minimal desktop shell client: taskbar and future Start menu.
 */
#include "base/usr/sys.h"
#include "base/win/protocol.h"
#include "base/win/window.h"

typedef struct Desk Desk;

typedef struct DeskWindow {
    uint32_t handle;
    uint32_t button;
    uint32_t state;
    char title[WIN_PROTOCOL_TEXT];
} DeskWindow;

struct Desk {
    int fd;
    uint32_t taskbar;
    uint32_t start_button;
    uint32_t active_handle;
    uint32_t request;
    WinChannel channel;
    DeskWindow windows[32];
    WinMessage pending_events[8];
    uint32_t pending_event_count;
    uint32_t window_count;
    void (*event)(Desk *self, const WinMessage *message);
    void (*refresh)(Desk *self);
    int (*connect)(Desk *self);
    int (*run)(Desk *self);
};

static int connect_desk(Desk *self) {
    struct sockaddr_un address;
    if (!self) return 0;
    memset(&address, 0, sizeof(address));
    address.family = AF_UNIX;
    strcpy(address.path, "/tmp/wing.sock");
    for (unsigned attempt = 0; attempt < 1000u; attempt++) {
        self->fd = (int)socket(AF_UNIX, SOCK_STREAM, 0);
        if (self->fd >= 0) {
            fcntl(self->fd, F_SETFL, O_NONBLOCK);
            if (connect(self->fd, &address) == 0) {
                WinMessage hello;
                WinChannel_init(&self->channel, self->fd);
                self->channel.clear(&self->channel, &hello, WIN_HELLO);
                hello.request = WIN_REQUEST_HELLO;
                hello.flags = WIN_CLIENT_DESKTOP;
                self->channel.send(&self->channel, &hello);
                {
                    WinMessage reply;
                    struct pollfd ready;
                    ready.fd = self->fd; ready.events = POLLIN; ready.revents = 0;
                    if (poll(&ready, 1, 1000) != 1 ||
                        !self->channel.recv(&self->channel, &reply) ||
                        reply.opcode != WIN_ACK) {
                        close(self->fd);
                        self->fd = -1;
                        continue;
                    }
                }
                fcntl(self->fd, F_SETFL, 0);
                return 1;
            }
            close(self->fd);
        }
        self->fd = -1;
        sleepms(10);
    }
    return 0;
}

static void event(Desk *self, const WinMessage *message);

static void queue_event(Desk *self, const WinMessage *message) {
    if (!self || !message || self->pending_event_count >= 8u) return;
    self->pending_events[self->pending_event_count++] = *message;
}

static void drain_events(Desk *self) {
    while (self && self->pending_event_count) {
        WinMessage message = self->pending_events[0];
        for (uint32_t i = 1; i < self->pending_event_count; i++)
            self->pending_events[i - 1u] = self->pending_events[i];
        self->pending_event_count--;
        self->event(self, &message);
    }
}

static int wait_ack(Desk *self, uint32_t request, WinMessage *reply) {
    for (;;) {
        struct pollfd ready;
        ready.fd = self->fd; ready.events = POLLIN; ready.revents = 0;
        if (poll(&ready, 1, 1000) != 1) return 0;
        if (!self->channel.recv(&self->channel, reply)) return 0;
        if (reply->opcode == WIN_ACK && reply->request == request) return 1;
        /* Creating a taskbar button is synchronous, but the server can queue
           application notifications before the button ACK. Do not discard
           those events: doing so loses WUSH/WASP entries and desynchronizes
           the taskbar model. Button-child creation events are identified by
           parent == taskbar and intentionally ignored. */
        if (reply->opcode == WIN_EVENT && reply->parent != self->taskbar)
            queue_event(self, reply);
    }
}

static int create_button(Desk *self, uint32_t parent, const char *title,
                         int32_t x, uint32_t width, uint32_t *handle) {
    WinMessage message;
    WinMessage reply;
    uint32_t request;
    if (!self || !parent || !title || !handle) return 0;
    self->channel.clear(&self->channel, &message, WIN_CREATE_CHILD);
    request = self->request++;
    message.request = request;
    message.parent = parent;
    message.x = x;
    message.y = 576;
    message.width = width;
    message.height = 20;
    message.color = 0x808080u;
    message.flags = WINDOW_NO_TITLE | WINDOW_NO_BUTTONS |
                    WINDOW_NO_MOVE | WINDOW_NO_RESIZE |
                    WINDOW_ALWAYS_ON_TOP;
    strncpy(message.text, title, sizeof(message.text));
    if (!self->channel.send(&self->channel, &message)) return 0;
    if (!wait_ack(self, request, &reply)) return 0;
    *handle = reply.handle;
    self->channel.clear(&self->channel, &message, WIN_SET_CONTENT);
    request = self->request++;
    message.request = request;
    message.handle = *handle;
    message.x = 8;
    message.y = 6;
    strncpy(message.text, title, sizeof(message.text));
    if (!self->channel.send(&self->channel, &message) ||
        !wait_ack(self, request, &reply)) return 0;
    return 1;
}

static void move_button(Desk *self, uint32_t handle, int32_t x) {
    WinMessage message;
    if (!self || !handle) return;
    self->channel.clear(&self->channel, &message, WIN_MOVE);
    message.request = self->request++;
    message.handle = handle;
    message.x = x;
    message.y = 576;
    self->channel.send(&self->channel, &message);
}

static void reposition_buttons(Desk *self) {
    if (!self) return;
    for (uint32_t i = 0; i < self->window_count; i++)
        move_button(self, self->windows[i].button, 70 + (int32_t)i * 120);
}

static void refresh(Desk *self) {
    WinMessage message;
    char text[WIN_PROTOCOL_TEXT];
    if (!self || !self->taskbar) return;
    /* The taskbar itself is an empty background. Visible labels belong to
       child button windows so each entry has its own hit target and bevel. */
    text[0] = '\0';
    self->channel.clear(&self->channel, &message, WIN_SET_CONTENT);
    message.request = self->request++;
    message.handle = self->taskbar;
    message.x = 10; message.y = 5;
    strncpy(message.text, text, sizeof(message.text));
    self->channel.send(&self->channel, &message);
}

static void event(Desk *self, const WinMessage *message) {
    if (!self || !message) return;
    if (message->event == WIN_EVENT_MOUSE && (message->value & 1u)) {
        uint32_t index;
        if (message->handle == self->start_button) {
            spawn("/initrd/win/bin/wasp.elf");
            return;
        }
        for (index = 0; index < self->window_count; index++)
            if (self->windows[index].button == message->handle) break;
        /* Accept taskbar-background coordinates as a compatibility fallback
           while clients from an older Wing may still target the parent. */
        if (index == self->window_count && message->handle == self->taskbar) {
            if (message->x < 70) {
                spawn("/initrd/win/bin/wasp.elf");
                return;
            }
            index = (uint32_t)(message->x - 70) / 120u;
        }
        if (index < self->window_count) {
            WinMessage command;
            uint32_t handle = self->windows[index].handle;
            if (self->windows[index].state == WINDOW_MINIMIZED_STATE ||
                self->windows[index].state == WINDOW_HIDDEN_STATE) {
                self->channel.clear(&self->channel, &command, WIN_RESTORE);
                command.request = self->request++;
                command.handle = handle;
                self->channel.send(&self->channel, &command);
                self->active_handle = handle;
            } else if (handle == self->active_handle) {
                self->channel.clear(&self->channel, &command, WIN_MINIMIZE);
                command.request = self->request++;
                command.handle = handle;
                self->channel.send(&self->channel, &command);
            } else {
                self->channel.clear(&self->channel, &command, WIN_FOCUS);
                command.request = self->request++;
                command.handle = handle;
                self->channel.send(&self->channel, &command);
                self->active_handle = handle;
            }
        }
        return;
    }
    if (!message->handle || message->handle == self->taskbar) return;
    if (message->event == WIN_EVENT_FOCUS) {
        self->active_handle = message->handle;
        return;
    }
    if (message->event == WIN_EVENT_WINDOW_CREATED) {
        if (self->window_count < 32u) {
            DeskWindow *entry;
            if (message->parent == self->taskbar) return;
            entry = &self->windows[self->window_count++];
            entry->handle = message->handle;
            strncpy(entry->title, message->text, sizeof(entry->title));
            entry->state = message->value;
            entry->button = 0;
            if (!create_button(self, self->taskbar, entry->title,
                               70 + (int32_t)(self->window_count - 1u) * 120,
                               112, &entry->button)) {
                entry->button = 0;
            }
            self->refresh(self);
        }
    } else if (message->event == WIN_EVENT_WINDOW_DESTROYED) {
        for (uint32_t i = 0; i < self->window_count; i++) {
            if (self->windows[i].handle == message->handle) {
                if (self->windows[i].button) {
                    WinMessage command;
                    WinMessage reply;
                    uint32_t request = self->request++;
                    self->channel.clear(&self->channel, &command, WIN_DESTROY);
                    command.request = request;
                    command.handle = self->windows[i].button;
                    if (self->channel.send(&self->channel, &command))
                        wait_ack(self, request, &reply);
                }
                for (uint32_t j = i + 1u; j < self->window_count; j++) self->windows[j - 1u] = self->windows[j];
                self->window_count--;
                if (self->active_handle == message->handle)
                    self->active_handle = 0;
                reposition_buttons(self);
                self->refresh(self);
                break;
            }
        }
    } else if (message->event == WIN_EVENT_WINDOW_STATE) {
        for (uint32_t i = 0; i < self->window_count; i++)
            if (self->windows[i].handle == message->handle) {
                self->windows[i].state = message->value;
                strncpy(self->windows[i].title, message->text, sizeof(self->windows[i].title));
                self->refresh(self);
            }
    }
}

static int create_taskbar(Desk *self) {
    WinMessage message;
    WinMessage reply;
    struct pollfd ready;
    if (!self) return 0;
    self->channel.clear(&self->channel, &message, WIN_CREATE);
    message.request = self->request++;
    message.x = 0; message.y = 572; message.width = 800; message.height = 28;
    message.color = 0x18202Cu;
    message.flags = WINDOW_NO_TITLE | WINDOW_NO_CLOSE | WINDOW_NO_MINIMIZE |
                    WINDOW_NO_MAXIMIZE | WINDOW_NO_MOVE | WINDOW_NO_RESIZE |
                    WINDOW_ALWAYS_ON_TOP;
    strncpy(message.text, "Taskbar", sizeof(message.text));
    if (!self->channel.send(&self->channel, &message)) return 0;
    ready.fd = self->fd; ready.events = POLLIN; ready.revents = 0;
    if (poll(&ready, 1, 1000) != 1 || !self->channel.recv(&self->channel, &reply) ||
        reply.opcode != WIN_ACK) return 0;
    self->taskbar = reply.handle;
    if (!create_button(self, self->taskbar, "Start", 4, 60, &self->start_button))
        return 0;
    self->channel.clear(&self->channel, &message, WIN_SET_CONTENT);
    message.request = self->request++;
    message.handle = self->taskbar;
    message.x = 0; message.y = 0;
    message.text[0] = '\0';
    return self->channel.send(&self->channel, &message);
}

static int run_desk(Desk *self) {
    WinMessage message;
    if (!self || !create_taskbar(self)) return 0;
    for (;;) {
        struct pollfd ready;
        ready.fd = self->fd; ready.events = POLLIN; ready.revents = 0;
        if (poll(&ready, 1, 1000) < 0) break;
        if (ready.revents & (POLLIN | POLLHUP | POLLERR)) {
            if (!self->channel.recv(&self->channel, &message)) break;
            if (message.opcode == WIN_EVENT) self->event(self, &message);
            drain_events(self);
        }
    }
    close(self->fd);
    self->fd = -1;
    return 0;
}

int main(int argc, char **argv) {
    Desk desk;
    unused(argc); unused(argv);
    memset(&desk, 0, sizeof(desk));
    desk.fd = -1;
    /* Desk is not a PTY endpoint. Do not retain SPARK's inherited master. */
    close(3);
    desk.request = WIN_REQUEST_FIRST;
    desk.event = event;
    desk.refresh = refresh;
    desk.connect = connect_desk;
    desk.run = run_desk;
    if (!desk.connect(&desk)) return 1;
    /* Desk is a Wing client, not a PTY consumer. Release inherited slave
       descriptors so the session can observe USH termination. */
    close(0);
    close(1);
    close(2);
    return desk.run(&desk) ? 0 : 1;
}
