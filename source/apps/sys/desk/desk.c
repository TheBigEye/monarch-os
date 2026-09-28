/**
 * @file desk.c
 * @brief Minimal desktop shell client: taskbar and future Start menu.
 */
#include "base/usr/sys.h"
#include "base/win/protocol.h"
#include "base/win/window.h"

#include "desk.h"

static int connect_desk(Desk *self) {
    struct sockaddr_un address;
    if (!self) return 0;
    memset(&address, 0, sizeof(address));
    address.family = AF_UNIX;
    strcpy(address.path, "/tmp/wing.sock");
    for (unsigned attempt = 0; attempt < 1000u; attempt++) {
        self->fd = (int)socket(AF_UNIX, SOCK_STREAM, 0);
        if (self->fd >= 0) {
            fcntl(self->fd, F_SETFD, FD_CLOEXEC);
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

static void queue_event(Desk *self, const WinMessage *message) {
    if (!self || !message || self->pending_event_count >= 8u) return;
    self->pending_events[self->pending_event_count++] = *message;
}

static void reposition_buttons(Desk *self) {
    if (!self) return;
    for (uint32_t i = 0; i < self->window_count; i++)
        if (self->windows[i].move)
            self->windows[i].move(&self->windows[i],
                                  DESK_WINDOW_BUTTON_X +
                                  (int32_t)i * DESK_WINDOW_BUTTON_STEP);
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
            DeskButton *entry;
            if (message->parent == self->taskbar) return;
            entry = &self->windows[self->window_count++];
            DeskButton_init(entry, &self->channel, &self->request);
            entry->handle = message->handle;
            strncpy(entry->title, message->text, sizeof(entry->title));
            entry->state = message->value;
            entry->button = 0;
            if (!self->taskbar_model.create_button(&self->taskbar_model, entry->title,
                               DESK_WINDOW_BUTTON_X +
                               (int32_t)(self->window_count - 1u) * DESK_WINDOW_BUTTON_STEP,
                               DESK_WINDOW_BUTTON_WIDTH, &entry->button)) {
                entry->button = 0;
            }
            self->taskbar_model.refresh(&self->taskbar_model);
        }
    } else if (message->event == WIN_EVENT_WINDOW_DESTROYED) {
        for (uint32_t i = 0; i < self->window_count; i++) {
            if (self->windows[i].handle == message->handle) {
                if (self->windows[i].button) {
                    WinMessage command;
                    self->channel.clear(&self->channel, &command, WIN_DESTROY);
                    command.request = self->request++;
                    command.handle = self->windows[i].button;
                    self->channel.send(&self->channel, &command);
                }
                for (uint32_t j = i + 1u; j < self->window_count; j++) self->windows[j - 1u] = self->windows[j];
                self->window_count--;
                if (self->active_handle == message->handle)
                    self->active_handle = 0;
                reposition_buttons(self);
                self->taskbar_model.refresh(&self->taskbar_model);
                break;
            }
        }
    } else if (message->event == WIN_EVENT_WINDOW_STATE) {
        for (uint32_t i = 0; i < self->window_count; i++)
            if (self->windows[i].handle == message->handle) {
                self->windows[i].state = message->value;
                strncpy(self->windows[i].title, message->text, sizeof(self->windows[i].title));
                self->taskbar_model.refresh(&self->taskbar_model);
            }
    }
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

static void taskbar_queue_event(void *context, const WinMessage *message) {
    queue_event((Desk *)context, message);
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
    self->taskbar_model.handle = self->taskbar;
    if (!self->taskbar_model.create_button(&self->taskbar_model, "Start", 4, DESK_START_WIDTH, &self->start_button))
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
    if (!self || !self->connect(self)) return 0;
    /* Desk is not a PTY endpoint. Release all inherited PTY descriptors
       after connecting, so USH can produce EOF when its session ends. */
    close(0);
    close(1);
    close(2);
    DeskTaskbar_init(&self->taskbar_model, &self->channel, &self->request, self->fd);
    self->taskbar_model.event_context = self;
    self->taskbar_model.queue_event = taskbar_queue_event;
    if (!create_taskbar(self)) return 0;
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

void Desk_init(Desk *self) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->fd = -1;
    self->request = WIN_REQUEST_FIRST;
    self->event = event;
    self->connect = connect_desk;
    self->run = run_desk;
}
