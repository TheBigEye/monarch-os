/**
 * @file server.c
 * @brief WingServer object: sockets, clients, windows and protocol dispatch.
 */
#include "server.h"
#include "base/usr/sys.h"
#include "base/gfx/allocator.h"
#include "base/win/terminal.h"

private Window *server_create(WingServer *self, const WinMessage *message, WingClient *client) {
    WindowManager *manager = self->manager;
    Window *window;
    Window *parent = nil;
    uint32_t slot;

    if (!manager || !message || manager->count >= WING_MANAGER_MAX) {
        return nil;
    }

    // If the message is a child creation request ...
    if (message->opcode == WIN_CREATE_CHILD) {
        // for each window in the manager ...
        for (uint32_t i = 0; i < manager->count; i++) {
            // ... Find the one with the matching id to the parent id in the message
            if (manager->windows[i].id == (int)message->parent) {
                parent = &manager->windows[i];
                break;
            }
        }
        // If no parent was found, return nil
        if (!parent) { return nil; }
    }

    // Find an available slot for the new window.
    slot = manager->count;
    for (uint32_t i = 0; i < manager->count; i++) {
        // If the window has no id, is not visible, has no parent, and has no first child, it's an available slot.
        if (!manager->windows[i].id && !manager->windows[i].visible &&
            !manager->windows[i].parent && !manager->windows[i].first_child) {
            slot = i;
            break;
        }
    }

    // Check if we have reached the maximum number of windows allowed
    if (slot == manager->count && manager->count >= WING_MANAGER_MAX) {
        return nil;
    }

    // Get the window at the available slot
    window = &manager->windows[slot];
    if ( // If the window creation fails, return nil
        !Window_create(
            window, // The window to create
            message->text[0] ? message->text : "Window", // The title of the window
            message->x, message->y, // The x and y position of the window
            message->width ? message->width : 240u, // The width of the window, default to 240 if not specified
            message->height ? message->height : 140u, // The height of the window, default to 140 if not specified
            message->color ? message->color : 0x164A82u,  // The frame color of the window
            (message->flags & WINDOW_TERMINAL) ? 0x101820u : 0xF4F5F7u, // The body color of the window, dark for terminal, light for others
            0 // The active state of the window, default to 0 (inactive)
        )
    ) {
        return nil;
    }

    // Set the window's id to the slot index + 1 (to avoid id 0)
    window->id = (int)(slot + 1u);
    window->owner = client;
    if (message->flags) {
        window->set_flags(window, (int)message->flags);
    }

    // If the message has no title and has text, set the window's content to the text
    if ((message->flags & WINDOW_NO_TITLE) && message->text[0]) {
        window->content_set(window, message->text, 8, 3);
    }

    // If the window is a terminal ...
    if (message->flags & WINDOW_TERMINAL) {
        // Allocate memory for a new Terminal structure
        Terminal *terminal = gfx_alloc(sizeof(*terminal));

        if (!terminal) {
            return nil;
        }

        // Initialize the terminal with the appropriate number of columns and rows based on the window size
        Terminal_init(terminal);
        {
            // Colums are calculated by subtracting 8 pixels from the window width and dividing by 8 (the width of a character)
            uint32_t cols = message->width > 8u ? (message->width - 8u) / 8u : 1u;
            // And rows are calculated by subtracting 28 pixels from the window height and dividing by 16 (the height of a character)
            uint32_t rows = message->height > 28u ? (message->height - 28u) / 16u : 1u;

            // If the terminal initialization fails, free the allocated memory and return nil
            if (!terminal->init(terminal, cols, rows)) {
                return nil;
            }
        }
        window->set_widget(window, terminal);
    }

    if (slot == manager->count) {
        manager->count++;
    }

    if (parent) {
        parent->add_child(parent, window);
    } else {
        manager->attach_top(manager, window);
    }

    return window;
}

private int server_open(WingServer *self) {
    struct sockaddr_un address;
    WingClient *clients = self->clients;
    int fd;

    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        clients[i].fd = -1;
        clients[i].used = 0;
        WingClient_init(&clients[i], -1);
    }

    fd = (int) socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        self->fd = -1;
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.family = AF_UNIX;
    strcpy(address.path, WING_SOCKET_PATH);
    unlink(address.path);

    if (bind(fd, &address) < 0 || listen(fd, WING_CLIENT_MAX) < 0) {
        close(fd);
        self->fd = -1;
        return -1;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    self->fd = fd;
    return fd;
}

private void server_evict(WingServer *self, WingClient *client) {
    WindowManager *manager = self->manager;
    struct dirty *dirty = self->dirty;
    if (!manager || !client) {
        return;
    }

    if (manager->active_window && manager->active_window->owner == client) {
        manager->active_window = nil;
        manager->active = -1;
    }

    if (manager->hovered_window && manager->hovered_window->owner == client) {
        manager->hovered_window = nil;
    }

    if (manager->drag_window && manager->drag_window->owner == client) {
        manager->drag_window = nil;
        manager->drag = -1;
        manager->resize_edges = 0;
    }

    for (uint32_t i = 0; i < manager->count; i++) {
        Window *window = &manager->windows[i];
        if (window->owner != client) {
            continue;
        }

        if (dirty) {
            dirty_add(dirty, window->frame);
        }

        /* A client can disappear without sending WIN_DESTROY (for example
           WASP exits after receiving its close event). Keep Desk's taskbar
           model synchronized with the compositor's ownership cleanup. */
        if (!(client->flags & WIN_CLIENT_DESKTOP)) {
            self->notify(self, window, WIN_EVENT_WINDOW_DESTROYED);
        }

        window->destroy(window);
    }

    for (uint32_t i = 0; i < manager->count; i++) {
        Window *window = &manager->windows[i];
        if (window->owner == client) {
            window->id = 0;
            window->owner = nil;
            window->parent = nil;
            window->next_sibling = nil;
            window->prev_sibling = nil;
            window->first_child = nil;
        }
    }

    if (manager->recover) {
        manager->recover(manager, dirty);
    }
}

private void server_prune(WingServer *self, Window *parent) {
    struct dirty *dirty = self->dirty;
    Window *child = parent ? parent->first_child : nil;

    while (child) {
        Window *next = child->next_sibling;
        if (dirty) dirty_add(dirty, child->frame);
        self->notify(self, child, WIN_EVENT_WINDOW_DESTROYED);
        self->prune(self, child);
        child->destroy(child);
        child->id = 0;
        child->owner = nil;
        child = next;
    }

    if (parent) {
        parent->first_child = nil;
    }
}

private int server_apply(WingServer *self, const WinMessage *message, WingClient *client) {
    WindowManager *manager = self->manager;
    struct dirty *dirty = self->dirty;
    Window *window;
    struct rect old;
    uint32_t event = 0;
    if (!manager || !message) return 0;
    window = manager->find(manager, message->handle);
    if (!window) return 0;
    old = window->frame;
    switch (message->opcode) {
        case WIN_SET_TITLE:
            window->set_title(window, message->text);
            break;
        case WIN_SET_CONTENT:
            window->content_set(window, message->text, message->x, message->y);
            break;
        case WIN_TERMINAL_DATA:
            if (!(window->flags & WINDOW_TERMINAL) || !window->widget) return 0;
            ((Terminal *)window->widget)->feed((Terminal *)window->widget,
                                               message->text,
                                               message->value <= WIN_PROTOCOL_TEXT ? message->value : WIN_PROTOCOL_TEXT);
            window->cache_valid = 0;
            break;
        case WIN_MOVE:
            window->move(window, message->x, message->y);
            event = WIN_EVENT_MOVE;
            break;
        case WIN_RESIZE:
            window->resize(window, message->x, message->y,
                           message->width, message->height);
            if ((window->flags & WINDOW_TERMINAL) && window->widget) {
                Terminal *terminal = (Terminal *)window->widget;
                uint32_t cols = message->width > 8u ? (message->width - 8u) / 8u : 1u;
                uint32_t rows = message->height > 28u ? (message->height - 28u) / 16u : 1u;
                terminal->resize(terminal, cols, rows);
            }
            event = WIN_EVENT_RESIZE;
            break;
        case WIN_SHOW:
            window->show(window);
            break;
        case WIN_HIDE:
            window->hide(window);
            break;
        case WIN_SET_FLAGS:
            window->set_flags(window, (int)message->flags);
            break;
        case WIN_FOCUS:
            manager->focus_window(manager, window, dirty);
            window->bring_to_front(window);
            break;
        case WIN_MINIMIZE:
            window->minimize(window);
            break;
        case WIN_MAXIMIZE:
            window->maximize(window);
            break;
        case WIN_RESTORE:
            if (window->maximized) window->maximize(window);
            else window->show(window);
            break;
        case WIN_DESTROY:
            event = WIN_EVENT_CLOSE;
            window->hide(window);
            if (window->parent && window->parent->remove_child)
                window->parent->remove_child(window->parent, window);
            break;
        default:
            return 0;
    }
    if (dirty) {
        dirty_add(dirty, old);
        dirty_add(dirty, window->frame);
    }
    /* Notifications are emitted by server_dispatch after the synchronous ACK.
       A client must never receive an asynchronous event while waiting for a
       command reply. */
    unused(event);
    unused(client);
    return 1;
}

private void server_drop(WingServer *self, WingClient *client) {
    int terminal_client;
    if (!client) {
        return;
    }

    terminal_client = (client->flags & WIN_CLIENT_TERMINAL) != 0;

    if (client->fd >= 0) {
        self->evict(self, client);
        close(client->fd);
    }

    gfx_free(client->output);
    client->fd = -1;
    client->used = 0;
    client->output = nil;
    client->output_used = 0;
    client->output_capacity = 0;
    client->flags = 0;

    if (terminal_client) {
        exit(0);
    }
}

private void server_emit(WingServer *self, Window *target, uint32_t event, int32_t x, int32_t y, int32_t dx, int32_t dy, uint32_t value) {
    WingClient *clients = self->clients;
    WinMessage message;
    memset(&message, 0, sizeof(message));
    message.version = WIN_PROTOCOL_VERSION;
    message.opcode = WIN_EVENT;
    message.size = sizeof(message);
    message.event = event;
    message.handle = target ? (uint32_t)target->id : 0;
    message.x = x; message.y = y; message.value = value;
    message.width = (uint32_t)dx; message.height = (uint32_t)dy;
    WingClient *owner = target ? (WingClient *)target->owner : nil;
    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        if (clients[i].fd >= 0 && (!owner || &clients[i] == owner) &&
            !(event == WIN_EVENT_MOUSE && (clients[i].flags & WIN_CLIENT_TERMINAL)) &&
            !clients[i].send(&clients[i], &message)) {
            self->drop(self, &clients[i]);
        }
    }
}

private void server_notify(WingServer *self, Window *window, uint32_t event) {
    WingClient *clients = self->clients;
    WinMessage message;

    if (!window) {
        return;
    }

    memset(&message, 0, sizeof(message));
    message.version = WIN_PROTOCOL_VERSION;
    message.opcode = WIN_EVENT;
    message.size = sizeof(message);
    message.event = event;
    message.handle = (uint32_t)window->id;
    message.parent = window->parent ? (uint32_t)window->parent->id : 0u;
    message.x = window->frame.x;
    message.y = window->frame.y;
    message.width = window->frame.w;
    message.height = window->frame.h;
    message.value = (uint32_t)window->state;

    if (window->title) {
        strncpy(message.text, window->title, sizeof(message.text));
    }

    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        if (clients[i].fd >= 0 && (clients[i].flags & WIN_CLIENT_DESKTOP)) {
            /* A desktop listener (Desk's taskbar, etc.) that stops reading
               must be dropped like any other stuck client. Previously this
               ignored the result, so a stalled desktop client kept
               accumulating queued notifications - growing its buffer via
               sbrk() on every window event, forever - instead of being
               disconnected on the first failed send. That is the bug behind
               the endless "SYS_BRK ... error=12" retries seen in log.txt. */
            if (!clients[i].send(&clients[i], &message)) {
                self->drop(self, &clients[i]);
            }
        }
    }
}

#if MONARCH_LOGWM
static void wm_log_state(WingServer *server, const char *reason) {
    int fd;
    char line[160];
    WindowManager *manager = server ? server->manager : nil;
    WingClient *clients = server ? server->clients : nil;
    if (!reason || !manager || !clients) return;
    fd = (int)open("/dev/serial", OWRITE);
    if (fd < 0) return;
    snprintf(line, sizeof(line),
             "[logwm] %s count=%u active=%d active_id=%u hover=%u drag=%u\n",
             reason, manager->count, manager->active,
             manager->active_window ? (uint32_t)manager->active_window->id : 0u,
             manager->hovered_window ? (uint32_t)manager->hovered_window->id : 0u,
             manager->drag_window ? (uint32_t)manager->drag_window->id : 0u);
    write(fd, line, strlen(line));
    for (uint32_t i = 0; i < manager->count; i++) {
        Window *window = &manager->windows[i];
        snprintf(line, sizeof(line),
                 "[logwm] win[%u] id=%d vis=%d state=%d flags=%x parent=%u prev=%u next=%u child=%u owner=%p frame=%d,%d %ux%u\n",
                 i, window->id, window->visible, window->state,
                 (unsigned)window->flags,
                 window->parent ? (uint32_t)window->parent->id : 0u,
                 window->prev_sibling ? (uint32_t)window->prev_sibling->id : 0u,
                 window->next_sibling ? (uint32_t)window->next_sibling->id : 0u,
                 window->first_child ? (uint32_t)window->first_child->id : 0u,
                 window->owner, window->frame.x, window->frame.y,
                 window->frame.w, window->frame.h);
        write(fd, line, strlen(line));
    }
    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        if (clients[i].fd >= 0) {
            snprintf(line, sizeof(line),
                     "[logwm] client[%u] fd=%d flags=%x in=%u out=%u\n",
                     i, clients[i].fd, (unsigned)clients[i].flags,
                     (unsigned)clients[i].used,
                     (unsigned)clients[i].output_used);
            write(fd, line, strlen(line));
        }
    }
    close(fd);
}
#else
static void wm_log_state(WingServer *server, const char *reason) {
    unused(server);
    unused(reason);
}
#endif

private void server_dispatch(WingServer *self, WingClient *client, const WinMessage *message) {
    WindowManager *manager = self->manager;
    struct dirty *dirty = self->dirty;
    WinMessage reply;
    if (message->opcode == WIN_HELLO) {
        client->flags = message->flags;
        client->channel.clear(&client->channel, &reply, WIN_ACK);
        reply.request = message->request;
        reply.value = WIN_PROTOCOL_VERSION;
        client->send(client, &reply);
    } else if (message->opcode == WIN_CREATE || message->opcode == WIN_CREATE_CHILD) {
        Window *window = self->create(self, message, client);
        client->channel.clear(&client->channel, &reply, window ? WIN_ACK : WIN_ERROR);
        reply.request = message->request;
        reply.handle = window ? (uint32_t)window->id : 0;
        /* The requester must receive its synchronous ACK before the desktop
           notification. Desk creates the taskbar itself and is also a
           desktop client; sending WINDOW_CREATED first makes it interpret the
           event as the create reply and reconnect forever. */
        client->send(client, &reply);
        if (window) {
            dirty_add(dirty, window->frame);
            self->notify(self, window, WIN_EVENT_WINDOW_CREATED);
        }
    } else {
        int ok = self->apply(self, message, client);
        Window *window = manager->find(manager, message->handle);
        uint32_t event = 0;
        if (message->opcode == WIN_MOVE || message->opcode == WIN_RESIZE)
            event = message->opcode == WIN_MOVE ? WIN_EVENT_MOVE : WIN_EVENT_RESIZE;
        else if (message->opcode == WIN_DESTROY)
            event = WIN_EVENT_CLOSE;

        if (message->opcode == WIN_TERMINAL_DATA) {
            wm_log_state(self, "terminal-data");
            return;
        }
        client->channel.clear(&client->channel, &reply, ok ? WIN_ACK : WIN_ERROR);
        reply.request = message->request;
        reply.handle = message->handle;
        /* Replies are deliberately first. Both Desk and WASP synchronously
           wait for ACK after each request; notifications are asynchronous. */
        client->send(client, &reply);
        if (!ok || !window) {
            wm_log_state(self, "command-failed");
            return;
        }

        if (message->opcode == WIN_FOCUS) {
            self->notify(self, window, WIN_EVENT_FOCUS);
        } else if (
            message->opcode == WIN_SET_TITLE ||
            message->opcode == WIN_MINIMIZE ||
            message->opcode == WIN_MAXIMIZE ||
            message->opcode == WIN_RESTORE ||
            message->opcode == WIN_DESTROY
        ) {
            self->notify(self, window, message->opcode == WIN_DESTROY ? WIN_EVENT_WINDOW_DESTROYED : WIN_EVENT_WINDOW_STATE);
        }

        if (event) {
            WinMessage notification;
            client->channel.clear(&client->channel, &notification, WIN_EVENT);
            notification.event = event;
            notification.handle = (uint32_t)window->id;
            notification.x = window->frame.x;
            notification.y = window->frame.y;
            notification.width = window->frame.w;
            notification.height = window->frame.h;
            client->send(client, &notification);
        }
    }
    wm_log_state(self, "protocol");
}

private int server_poll(WingServer *self) {
    WingClient *clients = self->clients;
    int listen_fd = self->fd;
    int changed = 0;
    if (listen_fd >= 0) {
        forever {
            int client = (int) accept(listen_fd);
            if (client < 0) {
                break;
            }

            /* Accepted client sockets belong exclusively to Wing. A WASP
               launched from Wing must not inherit Desk/WUSH connections. */
            fcntl(client, F_SETFD, FD_CLOEXEC);
            fcntl(client, F_SETFL, O_NONBLOCK);

            for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
                if (clients[i].fd < 0) {
                    clients[i].fd = client;
                    clients[i].used = 0;

                    WinChannel_init(&clients[i].channel, client);

                    clients[i].output = nil;
                    clients[i].output_used = 0;
                    clients[i].output_capacity = 0;
                    client = -1;
                    break;
                }
            }

            if (client >= 0) {
                close(client);
            } else {
                changed = 1;
            }
        }
    }

    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        WingClient *client = &clients[i];

        if (client->fd < 0) continue;
        if (!client->flush(client)) {
            self->drop(self, client); continue;
        }

        long count = read(client->fd, client->buffer + client->used, sizeof(client->buffer) - client->used);
        if (count == 0 || (count < 0 && errno != EAGAIN)) {
            self->drop(self, client);
            changed = 1;
            continue;
        }

        if (count > 0) { client->used += (size_t)count; changed = 1; }
        while (client->used >= sizeof(WinMessage)) {
            WinMessage message;

            memcpy(&message, client->buffer, sizeof(message));
            memmove(client->buffer, client->buffer + sizeof(message), client->used - sizeof(message));

            client->used -= sizeof(message);

            if (message.version != WIN_PROTOCOL_VERSION || message.size != sizeof(message)) {
                continue;
            }

            self->dispatch(self, client, &message);
        }
    }
    return changed;
}

/* Bind a WingServer to its vtable and to the three pieces of state (client
   table, window manager, dirty-rect tracker) that used to travel as
   separate arguments through nearly every function in this file. */
void WingServer_init(WingServer *self, WingClient *clients, WindowManager *manager, struct dirty *dirty) {
    self->fd = -1;
    self->clients = clients;
    self->manager = manager;
    self->dirty = dirty;
    self->open = server_open;
    self->poll = server_poll;
    self->dispatch = server_dispatch;
    self->create = server_create;
    self->apply = server_apply;
    self->evict = server_evict;
    self->drop = server_drop;
    self->prune = server_prune;
    self->emit = server_emit;
    self->notify = server_notify;
    self->log = wm_log_state;
}

