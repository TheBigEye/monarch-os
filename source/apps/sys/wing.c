/**
 * @file wing.c
 * @brief First userspace Monarch window-manager shell.
 */

#include "base/usr/sys.h"
#include "base/fbd/fbd.h"
#include "base/gfx/render.h"
#include "base/gfx/surface.h"
#include "base/gfx/dirty.h"
#include "base/win/protocol.h"
#include "base/win/terminal.h"
#include "base/win/window.h"
#include "base/win/cursor.h"
#include "base/win/input.h"
#include "base/win/manager.h"

#define WING_W 800u
#define WING_H 600u
#define CURSOR_W 16u
#define CURSOR_H 24u
#define MOUSE_LEFT  1u
#define MOUSE_RIGHT 2u

#define WING_CLIENT_MAX 32u

struct wing_client {
    int fd;
    size_t used;
    uint8_t buffer[sizeof(WinMessage)];
    uint8_t *output;
    size_t output_used;
    size_t output_capacity;
    uint32_t flags;
    WinChannel channel;
};

#if MONARCH_LOGWM
static void wm_log_state(const char *reason, WindowManager *manager,
                         struct wing_client clients[WING_CLIENT_MAX]) {
    int fd;
    char line[160];
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
#define wm_log_state(reason, manager, clients) ((void)0)
#endif

static void notify_desktop(struct wing_client clients[WING_CLIENT_MAX],
                           Window *window, uint32_t event);

static int wing_server_open(struct wing_client clients[WING_CLIENT_MAX]) {
    struct sockaddr_un address;
    int fd;
    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        clients[i].fd = -1;
        clients[i].used = 0;
    }
    fd = (int)socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.family = AF_UNIX;
    strcpy(address.path, "/tmp/wing.sock");
    unlink(address.path);
    if (bind(fd, &address) < 0 || listen(fd, WING_CLIENT_MAX) < 0) {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static Window *server_create_window(WindowManager *manager, const WinMessage *message,
                                     struct wing_client *client) {
    Window *window;
    Window *parent = nil;
    uint32_t slot;
    if (!manager || !message || manager->count >= WING_MANAGER_MAX) { return nil; }
    if (message->opcode == WIN_CREATE_CHILD) {
        for (uint32_t i = 0; i < manager->count; i++) {
            if (manager->windows[i].id == (int)message->parent) {
                parent = &manager->windows[i];
                break;
            }
        }
        if (!parent) { return nil; }
    }
    slot = manager->count;
    for (uint32_t i = 0; i < manager->count; i++) {
        if (!manager->windows[i].id && !manager->windows[i].visible &&
            !manager->windows[i].parent && !manager->windows[i].first_child) {
            slot = i;
            break;
        }
    }
    if (slot == manager->count && manager->count >= WING_MANAGER_MAX) return nil;
    window = &manager->windows[slot];
    if (!Window_create(window, message->text[0] ? message->text : "Window",
                       message->x, message->y,
                       message->width ? message->width : 240u,
                       message->height ? message->height : 140u,
                       message->color ? message->color : 0x164A82u,
                       (message->flags & WINDOW_TERMINAL) ? 0x101820u : 0xF4F5F7u,
                       0)) { return nil; }
    window->id = (int)(slot + 1u);
    window->owner = client;
    if (message->flags) window->set_flags(window, (int)message->flags);
    if ((message->flags & WINDOW_NO_TITLE) && message->text[0])
        window->content_set(window, message->text, 8, 3);
    if (message->flags & WINDOW_TERMINAL) {
        Terminal *terminal = gfx_alloc(sizeof(*terminal));
        if (!terminal) return nil;
        Terminal_init(terminal);
        {
            uint32_t cols = message->width > 8u ? (message->width - 8u) / 8u : 1u;
            uint32_t rows = message->height > 28u ? (message->height - 28u) / 16u : 1u;
            if (!terminal->init(terminal, cols, rows)) return nil;
        }
        window->set_widget(window, terminal);
    }
    if (slot == manager->count) manager->count++;
    if (parent) parent->add_child(parent, window);
    else manager->attach_top(manager, window);
    return window;
}

static void server_cleanup_client(WindowManager *manager,
                                   struct wing_client clients[WING_CLIENT_MAX],
                                   struct wing_client *client,
                                   struct dirty *dirty) {
    if (!manager || !client) return;
    if (manager->active_window && manager->active_window->owner == client) {
        manager->active_window = nil;
        manager->active = -1;
    }
    if (manager->hovered_window && manager->hovered_window->owner == client)
        manager->hovered_window = nil;
    if (manager->drag_window && manager->drag_window->owner == client) {
        manager->drag_window = nil;
        manager->drag = -1;
        manager->resize_edges = 0;
    }
    for (uint32_t i = 0; i < manager->count; i++) {
        Window *window = &manager->windows[i];
        if (window->owner != client) continue;
        if (dirty) dirty_add(dirty, window->frame);
        /* A client can disappear without sending WIN_DESTROY (for example
           WASP exits after receiving its close event). Keep Desk's taskbar
           model synchronized with the compositor's ownership cleanup. */
        if (!(client->flags & WIN_CLIENT_DESKTOP))
            notify_desktop(clients, window, WIN_EVENT_WINDOW_DESTROYED);
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
    if (manager->recover) manager->recover(manager, dirty);
}

static int client_send(struct wing_client *client, const WinMessage *message);

static void close_child_tree(struct wing_client clients[WING_CLIENT_MAX],
                             Window *parent, struct dirty *dirty) {
    Window *child = parent ? parent->first_child : nil;
    while (child) {
        Window *next = child->next_sibling;
        if (dirty) dirty_add(dirty, child->frame);
        notify_desktop(clients, child, WIN_EVENT_WINDOW_DESTROYED);
        close_child_tree(clients, child, dirty);
        child->destroy(child);
        child->id = 0;
        child->owner = nil;
        child = next;
    }
    if (parent) parent->first_child = nil;
}

static int server_apply_window(WindowManager *manager,
                               const WinMessage *message,
                               struct wing_client *client, struct dirty *dirty) {
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
    /* Notifications are emitted by server_message after the synchronous ACK.
       A client must never receive an asynchronous event while waiting for a
       command reply. */
    unused(event);
    unused(client);
    return 1;
}

static int client_flush(struct wing_client *client) {
    while (client && client->output_used) {
        long count = write(client->fd, client->output, client->output_used);
        if (count < 0 && errno == EAGAIN) return 1;
        if (count <= 0) return 0;
        client->output_used -= (size_t)count;
        if (client->output_used)
            memmove(client->output, client->output + count, client->output_used);
    }
    return 1;
}

static int client_send(struct wing_client *client, const WinMessage *message) {
    size_t needed;
    size_t capacity;
    uint8_t *buffer;
    if (!client || client->fd < 0 || !message) return 0;
    if (message->opcode == WIN_EVENT && message->event == WIN_EVENT_MOUSE &&
        client->output_used >= sizeof(*message)) {
        WinMessage *last = (WinMessage *)(client->output +
                              client->output_used - sizeof(*message));
        if (last->opcode == WIN_EVENT && last->event == WIN_EVENT_MOUSE) {
            *last = *message;
            return client_flush(client);
        }
    }
    needed = client->output_used + sizeof(*message);
    if (needed > client->output_capacity) {
        capacity = client->output_capacity ? client->output_capacity : sizeof(*message) * 4u;
        while (capacity < needed) capacity *= 2u;
        buffer = gfx_alloc(capacity);
        if (!buffer) return 0;
        if (client->output_used) memcpy(buffer, client->output, client->output_used);
        gfx_free(client->output);
        client->output = buffer;
        client->output_capacity = capacity;
    }
    memcpy(client->output + client->output_used, message, sizeof(*message));
    client->output_used += sizeof(*message);
    return client_flush(client);
}

static void client_close(struct wing_client *client,
                         struct wing_client clients[WING_CLIENT_MAX],
                         WindowManager *manager, struct dirty *dirty) {
    if (!client) return;
    if (client->fd >= 0) {
        server_cleanup_client(manager, clients, client, dirty);
        close(client->fd);
    }
    gfx_free(client->output);
    client->fd = -1;
    client->used = 0;
    client->output = nil;
    client->output_used = 0;
    client->output_capacity = 0;
}

static void wing_send_event(struct wing_client clients[WING_CLIENT_MAX], Window *target,
                            uint32_t event, int32_t x, int32_t y,
                            int32_t dx, int32_t dy, uint32_t value) {
    WinMessage message;
    memset(&message, 0, sizeof(message));
    message.version = WIN_PROTOCOL_VERSION;
    message.opcode = WIN_EVENT;
    message.size = sizeof(message);
    message.event = event;
    message.handle = target ? (uint32_t)target->id : 0;
    message.x = x; message.y = y; message.value = value;
    message.width = (uint32_t)dx; message.height = (uint32_t)dy;
    struct wing_client *owner = target ? (struct wing_client *)target->owner : nil;
    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        if (clients[i].fd >= 0 && (!owner || &clients[i] == owner) &&
            !(event == WIN_EVENT_MOUSE && (clients[i].flags & WIN_CLIENT_TERMINAL)) &&
            !client_send(&clients[i], &message)) {
            close(clients[i].fd);
            gfx_free(clients[i].output);
            clients[i].fd = -1;
            clients[i].used = 0;
            clients[i].output = nil;
            clients[i].output_used = 0;
            clients[i].output_capacity = 0;
        }
    }
}

static void notify_desktop(struct wing_client clients[WING_CLIENT_MAX],
                           Window *window, uint32_t event) {
    WinMessage message;
    if (!window) return;
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
    if (window->title) strncpy(message.text, window->title, sizeof(message.text));
    for (unsigned i = 0; i < WING_CLIENT_MAX; i++)
        if (clients[i].fd >= 0 && (clients[i].flags & WIN_CLIENT_DESKTOP))
            client_send(&clients[i], &message);
}

static void server_message(struct wing_client *client,
                           struct wing_client clients[WING_CLIENT_MAX],
                           WindowManager *manager, struct dirty *dirty,
                           const WinMessage *message) {
    WinMessage reply;
    if (message->opcode == WIN_HELLO) {
        client->flags = message->flags;
        client->channel.clear(&client->channel, &reply, WIN_ACK);
        reply.request = message->request;
        reply.value = WIN_PROTOCOL_VERSION;
        client_send(client, &reply);
    } else if (message->opcode == WIN_CREATE || message->opcode == WIN_CREATE_CHILD) {
        Window *window = server_create_window(manager, message, client);
        client->channel.clear(&client->channel, &reply, window ? WIN_ACK : WIN_ERROR);
        reply.request = message->request;
        reply.handle = window ? (uint32_t)window->id : 0;
        /* The requester must receive its synchronous ACK before the desktop
           notification. Desk creates the taskbar itself and is also a
           desktop client; sending WINDOW_CREATED first makes it interpret the
           event as the create reply and reconnect forever. */
        client_send(client, &reply);
        if (window) {
            dirty_add(dirty, window->frame);
            notify_desktop(clients, window, WIN_EVENT_WINDOW_CREATED);
        }
    } else {
        int ok = server_apply_window(manager, message, client, dirty);
        Window *window = manager->find(manager, message->handle);
        uint32_t event = 0;
        if (message->opcode == WIN_MOVE || message->opcode == WIN_RESIZE)
            event = message->opcode == WIN_MOVE ? WIN_EVENT_MOVE : WIN_EVENT_RESIZE;
        else if (message->opcode == WIN_DESTROY)
            event = WIN_EVENT_CLOSE;
        if (message->opcode == WIN_TERMINAL_DATA) {
            wm_log_state("terminal-data", manager, clients);
            return;
        }
        client->channel.clear(&client->channel, &reply, ok ? WIN_ACK : WIN_ERROR);
        reply.request = message->request;
        reply.handle = message->handle;
        /* Replies are deliberately first. Both Desk and WASP synchronously
           wait for ACK after each request; notifications are asynchronous. */
        client_send(client, &reply);
        if (!ok || !window) {
            wm_log_state("command-failed", manager, clients);
            return;
        }
        if (message->opcode == WIN_FOCUS) {
            notify_desktop(clients, window, WIN_EVENT_FOCUS);
        } else if (message->opcode == WIN_SET_TITLE ||
                   message->opcode == WIN_MINIMIZE ||
                   message->opcode == WIN_MAXIMIZE ||
                   message->opcode == WIN_RESTORE ||
                   message->opcode == WIN_DESTROY) {
            notify_desktop(clients, window,
                           message->opcode == WIN_DESTROY ? WIN_EVENT_WINDOW_DESTROYED :
                           WIN_EVENT_WINDOW_STATE);
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
            client_send(client, &notification);
        }
    }
    wm_log_state("protocol", manager, clients);
}

static int wing_server_poll(int server, struct wing_client clients[WING_CLIENT_MAX],
                            WindowManager *manager, struct dirty *dirty) {
    int changed = 0;
    if (server >= 0) {
        for (;;) {
            int client = (int)accept(server);
            if (client < 0) break;
            fcntl(client, F_SETFL, O_NONBLOCK);
            for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
                if (clients[i].fd < 0) { clients[i].fd = client; clients[i].used = 0; WinChannel_init(&clients[i].channel, client); clients[i].output = nil; clients[i].output_used = 0; clients[i].output_capacity = 0; client = -1; break; }
            }
            if (client >= 0) close(client);
            else changed = 1;
        }
    }
    for (unsigned i = 0; i < WING_CLIENT_MAX; i++) {
        struct wing_client *client = &clients[i];
        if (client->fd < 0) continue;
        if (!client_flush(client)) { client_close(client, clients, manager, dirty); continue; }
        long count = read(client->fd, client->buffer + client->used,
                          sizeof(client->buffer) - client->used);
        if (count == 0 || (count < 0 && errno != EAGAIN)) {
            client_close(client, clients, manager, dirty);
            changed = 1;
            continue;
        }
        if (count > 0) { client->used += (size_t)count; changed = 1; }
        while (client->used >= sizeof(WinMessage)) {
            WinMessage message;
            memcpy(&message, client->buffer, sizeof(message));
            memmove(client->buffer, client->buffer + sizeof(message),
                    client->used - sizeof(message));
            client->used -= sizeof(message);
            if (message.version != WIN_PROTOCOL_VERSION ||
                message.size != sizeof(message)) continue;
            server_message(client, clients, manager, dirty, &message);
        }
    }
    return changed;
}

static int present_rects(struct fbd *fb, struct surface *screen, struct dirty *dirty) {
    if (!fb || !screen || !dirty) return 0;
    for (uint32_t n = 0; n < dirty->count; n++) {
        struct rect r = rect_clip(dirty->rects[n], fb->width, fb->height);
        struct fb_blit blit;
        if (!r.w || !r.h) continue;
        blit.x = (uint32_t)r.x; blit.y = (uint32_t)r.y;
        blit.w = r.w; blit.h = r.h; blit.pitch = screen->pitch;
        blit.pixels = (uintptr_t)(screen->pixels + (size_t)r.y * screen->pitch + (size_t)r.x * 4u);
        if (ioctl(fb->fd, FB_BLIT, &blit) < 0) return 0;
    }
    return 1;
}

static struct rect cursor_rect(struct Cursor *cursor) {
    return rect_make(cursor->x, cursor->y, CURSOR_W, CURSOR_H);
}

static void sync_window_terminal(Window *window) {
    Terminal *terminal;
    uint32_t cols;
    uint32_t rows;
    if (!window || !(window->flags & WINDOW_TERMINAL) || !window->widget) return;
    terminal = (Terminal *)window->widget;
    cols = window->frame.w > 8u ? (window->frame.w - 8u) / 8u : 1u;
    rows = window->frame.h > 28u ? (window->frame.h - 28u) / 16u : 1u;
    if (cols < 1u) cols = 1u;
    if (rows < 1u) rows = 1u;
    if (terminal->cols != cols || terminal->rows != rows)
        terminal->resize(terminal, cols, rows);
    window->cache_valid = 0;
}

static void paint_desktop(struct renderer *renderer, uint32_t width, uint32_t height) {
    renderer->clear(renderer, 0x202A36);
    renderer->rect(renderer, 0, 0, width, 24, 0x101820);
    renderer->rect(renderer, 0, height - 28, width, 28, 0x18202C);
    renderer->text(renderer, "Monarch Wing", 10, 7, 0xFFFFFF, 0x101820);
    renderer->text(renderer, "Userspace desktop", 12, height - 20, 0xB8C4D0, 0x18202C);
}

struct context_menu {
    int visible;
    int mode;
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    Window *target;
};

static struct rect context_rect(struct context_menu *menu) {
    return rect_make(menu->x, menu->y, menu->w, menu->h);
}

static void context_show(struct context_menu *menu, Window *target, int mode,
                         int32_t x, int32_t y, struct dirty *dirty) {
    uint32_t h = mode == 1 ? 70u : 114u;
    if (!menu) return;
    if (menu->visible && dirty) dirty_add(dirty, context_rect(menu));
    if (x + 190 > 800) x = 800 - 190;
    if (y + (int32_t)h > 600) y = 600 - (int32_t)h;
    menu->visible = 1;
    menu->mode = mode;
    menu->x = x;
    menu->y = y;
    menu->w = 190;
    menu->h = h;
    menu->target = target;
    if (dirty) dirty_add(dirty, context_rect(menu));
}

static void context_hide(struct context_menu *menu, struct dirty *dirty) {
    if (!menu || !menu->visible) return;
    if (dirty) dirty_add(dirty, context_rect(menu));
    menu->visible = 0;
    menu->target = nil;
}

static void context_paint(struct context_menu *menu, struct renderer *renderer) {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    if (!menu || !menu->visible || !renderer) return;
    x = menu->x; y = menu->y; w = menu->w; h = menu->h;
    renderer->rect(renderer, (uint32_t)x, (uint32_t)y, w, h, 0x000000);
    renderer->rect(renderer, (uint32_t)x + 1u, (uint32_t)y + 1u, w - 2u, h - 2u, 0xFFFFFF);
    renderer->rect(renderer, (uint32_t)x + 3u, (uint32_t)y + 3u, w - 6u, h - 6u, 0x808080);
    renderer->rect(renderer, (uint32_t)x + 4u, (uint32_t)y + 4u, w - 8u, h - 8u, 0xC0C0C0);
    renderer->rect(renderer, (uint32_t)x + 4u, (uint32_t)y + 4u, w - 8u, 18u, 0x000080);
    renderer->text(renderer, menu->mode == 1 ? "Desktop" : "Window",
                   (uint32_t)x + 10u, (uint32_t)y + 7u, 0xFFFFFF, 0x000080);
    if (menu->mode == 1) {
        renderer->text(renderer, "New window", (uint32_t)x + 14u,
                       (uint32_t)y + 35u, 0x000000, 0xC0C0C0);
    } else {
        renderer->text(renderer, "Minimize", (uint32_t)x + 14u,
                       (uint32_t)y + 32u, 0x000000, 0xC0C0C0);
        renderer->text(renderer, "Maximize", (uint32_t)x + 14u,
                       (uint32_t)y + 60u, 0x000000, 0xC0C0C0);
        renderer->text(renderer, "Close", (uint32_t)x + 14u,
                       (uint32_t)y + 88u, 0x000000, 0xC0C0C0);
    }
}

static int spawn_window_app(void) {
    return spawn("/initrd/win/bin/wasp.elf") >= 0;
}

static int context_click(struct context_menu *menu, WindowManager *manager,
                         int32_t x, int32_t y, struct dirty *dirty) {
    Window *target;
    if (!menu || !manager || !menu->visible || x < menu->x || y < menu->y ||
        x >= menu->x + (int32_t)menu->w || y >= menu->y + (int32_t)menu->h) return 0;
    if (y < menu->y + 24) return 1;
    if (menu->mode == 1) {
        if (y < menu->y + 58) {
            context_hide(menu, dirty);
            spawn_window_app();
        }
        return 1;
    }
    target = menu->target;
    if (!target || !target->visible) { context_hide(menu, dirty); return 1; }
    if (y < menu->y + 54) manager->pending_action = EVENT_MINIMIZE;
    else if (y < menu->y + 82) manager->pending_action = EVENT_MAXIMIZE;
    else manager->pending_action = EVENT_CLOSE;
    target->cache_valid = 0;
    target->minimize_button.pressed = manager->pending_action == EVENT_MINIMIZE;
    target->maximize_button.pressed = manager->pending_action == EVENT_MAXIMIZE;
    target->close_button.pressed = manager->pending_action == EVENT_CLOSE;
    manager->pending_window = target;
    manager->pending_ticks = 1;
    context_hide(menu, dirty);
    dirty_add(dirty, target->frame);
    return 1;
}

int main(int argc, char **argv) {
    struct fbd fb;
    struct surface *screen;
    struct surface *background;
    struct renderer renderer;
    struct Window *windows;
    struct context_menu context_menu;
    int context_mode = 0;
    struct Cursor cursor;
    struct WindowManager manager;
    struct Input input;
    struct Event event;
    struct wing_client *clients = nil;
    int server_fd = -1;
    struct dirty dirty;
    Window *drag_window = nil;
    uint32_t old_buttons = 0;
    int once = argc > 1 && strcmp(argv[1], "--once") == 0;

    /* context_menu is a plain state object, not a constructor-backed widget.
       It must start hidden; otherwise stale stack bytes can make the first
       compositor pass paint an arbitrary rectangle or dereference garbage. */
    memset(&context_menu, 0, sizeof(context_menu));

    if (!fbd_open(&fb) || fb.width != WING_W || fb.height != WING_H) {
        eputs("wing: need an 800x600 linear framebuffer\n");
        return 1;
    }
    Input_init(&input);
    if (!once) input.open(&input);
    {
        struct pixel_format format;
        memset(&format, 0, sizeof(format));
        format.bpp = (uint8_t)fb.bpp;
        format.red_position = (uint8_t)fb.red_pos;
        format.red_mask_size = (uint8_t)fb.red_size;
        format.green_position = (uint8_t)fb.green_pos;
        format.green_mask_size = (uint8_t)fb.green_size;
        format.blue_position = (uint8_t)fb.blue_pos;
        format.blue_mask_size = (uint8_t)fb.blue_size;
        screen = surface_native(fb.width, fb.height, &format);
        background = surface_native(fb.width, fb.height, &format);
    }
    if (!screen || !background) {
        eputs("wing: cannot allocate desktop surfaces\n");
        if (screen) surface_destroy(screen);
        if (background) surface_destroy(background);
        if (!once) input.close(&input);
        fbd_close(&fb);
        return 1;
    }

    windows = gfx_zero(WING_MANAGER_MAX, sizeof(*windows));
    if (!windows) {
        eputs("wing: cannot allocate window storage\n");
        surface_destroy(screen);
        surface_destroy(background);
        gfx_free(clients);
        gfx_free(windows);
        if (!once) input.close(&input);
        fbd_close(&fb);
        return 1;
    }

    clients = gfx_zero(WING_CLIENT_MAX, sizeof(*clients));
    if (!clients) {
        eputs("wing: cannot allocate client storage\n");
        gfx_free(windows);
        surface_destroy(screen);
        surface_destroy(background);
        if (!once) input.close(&input);
        fbd_close(&fb);
        return 1;
    }

    if (!Cursor_init(&cursor, 12, 36)) {
        eputs("wing: cannot create cursor\n");
        surface_destroy(screen);
        surface_destroy(background);
        if (!once) input.close(&input);
        fbd_close(&fb);
        return 1;
    }

    WindowManager_init(&manager, windows, 0, &cursor);

    /* The background is stable between events. Moving actors only restore the
       old and new rectangles, just like the Boing animation path. */
    render_target(&renderer, background);
    paint_desktop(&renderer, fb.width, fb.height);
    render_target(&renderer, screen);
    paint_desktop(&renderer, fb.width, fb.height);
    manager.paint(&manager, &renderer);
    context_paint(&context_menu, &renderer);
    cursor.paint(&cursor, &renderer);
    dirty_init(&dirty);
    dirty_add(&dirty, rect_make(0, 0, fb.width, fb.height));
    if (!present_rects(&fb, screen, &dirty)) {
        eputs("wing: framebuffer presentation failed\n");
        cursor.destroy(&cursor);
        surface_destroy(screen);
        surface_destroy(background);
        gfx_free(clients);
        gfx_free(windows);
        if (!once) input.close(&input);
        fbd_close(&fb);
        return 1;
    }

    /* Publish the window server only after the first complete desktop frame
       is ready. Clients cannot race the initial compositor setup. */
    server_fd = wing_server_open(clients);
    if (server_fd < 0) eputs("wing: socket server unavailable\n");

    if (!once) {
        for (;;) {
            int changed = 0;
            dirty_clear(&dirty);
            changed = manager.tick(&manager, &dirty);
            changed |= wing_server_poll(server_fd, clients, &manager, &dirty);
            while (input.poll(&input, &event)) {
                if (event.type == WING_EVENT_MOUSE) {
                    struct rect old_cursor = cursor_rect(&cursor);
                    int pressed = (event.buttons & MOUSE_LEFT) && !(old_buttons & MOUSE_LEFT);
                    int released = !(event.buttons & MOUSE_LEFT) && (old_buttons & MOUSE_LEFT);
                    int right_pressed = (event.buttons & MOUSE_RIGHT) && !(old_buttons & MOUSE_RIGHT);
                    event.action = pressed ? EVENT_MOUSE_DOWN : released ? EVENT_MOUSE_UP : EVENT_MOUSE_MOVE;
                    event.mouse.x = event.x;
                    event.mouse.y = event.y;
                    event.mouse.dx = event.dx;
                    event.mouse.dy = event.dy;
                    event.mouse.buttons = event.buttons;
                    wing_send_event(clients, manager.hit(&manager, event.x, event.y),
                                    WIN_EVENT_MOUSE, event.x, event.y,
                                    event.dx, event.dy, event.buttons);
                    cursor.x = event.x;
                    cursor.y = event.y;
                    dirty_add(&dirty, old_cursor);
                    dirty_add(&dirty, cursor_rect(&cursor));
                    manager.update_hover(&manager, event.x, event.y, &dirty);
                    manager.event(&manager, event.action ? event.action : event.type, &event);
                    if (right_pressed) {
                        Window *hit = manager.hit(&manager, event.x, event.y);
                        if (!hit) {
                            context_mode = 1;
                            context_show(&context_menu, nil, context_mode, event.x, event.y, &dirty);
                        } else if (hit->title_hit(hit, event.x, event.y)) {
                            context_mode = 2;
                            context_show(&context_menu, hit, context_mode, event.x, event.y, &dirty);
                        }
                    }
                    if (pressed && context_click(&context_menu, &manager, event.x, event.y, &dirty)) {
                        /* The context menu consumed this click. */
                    } else if (pressed) {
                        /* A missed release packet must not keep dragging the
                           previous window into the next click. */
                        manager.end_drag(&manager);
                        drag_window = nil;
                        if (context_menu.visible)
                            context_hide(&context_menu, &dirty);
                        {
                            Window *focus_before = manager.active_window;
                            Window *clicked = manager.hit(&manager, event.x, event.y);
                            int clicked_close = clicked && clicked->close_button.hit(&clicked->close_button, event.x, event.y);
                            drag_window = manager.press(&manager, event.x, event.y, &dirty);
                            if (clicked_close) {
                                wing_send_event(clients, clicked, WIN_EVENT_CLOSE,
                                                0, 0, 0, 0, 0);
                                /* The close is committed locally before the
                                   client exits. Publish it immediately so
                                   Desk removes the taskbar button instead
                                   of keeping a stale WASP handle. */
                                notify_desktop(clients, clicked,
                                               WIN_EVENT_WINDOW_DESTROYED);
                                /* manager.press() has already hidden and
                                   detached the parent. Remove its descendants
                                   before recovering focus; otherwise
                                   activate_top() can select a visible child
                                   whose parent is no longer in the root. */
                                close_child_tree(clients, clicked, &dirty);
                                manager.recover(&manager, &dirty);
                            }
                            if (focus_before != manager.active_window) {
                                wing_send_event(clients, focus_before, WIN_EVENT_BLUR,
                                                 0, 0, 0, 0, 0);
                                wing_send_event(clients, manager.active_window, WIN_EVENT_FOCUS,
                                                 0, 0, 0, 0, 0);
                                notify_desktop(clients, manager.active_window, WIN_EVENT_FOCUS);
                            }
                        }
                        if (drag_window) {
                            if (manager.begin_drag(&manager, drag_window, event.x, event.y)) {
                                drag_window = manager.drag_window;
                            } else {
                                /* Body clicks focus the window but do not
                                   start a drag operation. */
                                drag_window = nil;
                            }
                        }
                    }
                    if (drag_window && (event.buttons & 1u)) {
                        manager.update_drag(&manager, event.x, event.y, &dirty);
                        sync_window_terminal(drag_window);
                        if (manager.resize_edges)
                            wing_send_event(clients, drag_window, WIN_EVENT_RESIZE,
                                            drag_window->frame.x, drag_window->frame.y,
                                            (int32_t)drag_window->frame.w,
                                            (int32_t)drag_window->frame.h, 0);
                    }
                    if (released) {
                        manager.release_buttons(&manager, &dirty);
                        manager.end_drag(&manager);
                        drag_window = nil;
                    }
                    old_buttons = event.buttons;
                    wm_log_state("mouse", &manager, clients);
                    changed = 1;
                } else if (event.type == WING_EVENT_KEY) {
                    wing_send_event(clients, manager.active_window,
                                    WIN_EVENT_KEY, 0, 0, 0, 0, event.key);
                    if (manager.key(&manager, event.key) < 0) exit(0);
                    wm_log_state("key", &manager, clients);
                }
            }
            if (changed) {
                /* Always-on-top overlays (Desk's taskbar and its button
                   children) must be included in every visual transaction.
                   Restoring a dirty region from the background can otherwise
                   erase an overlay and leave it absent until a later full
                   redraw. The area is tiny, so correctness wins over a few
                   pixels of extra work. */
                for (uint32_t i = 0; i < manager.count; i++)
                    if (manager.windows[i].visible &&
                        (manager.windows[i].flags & WINDOW_ALWAYS_ON_TOP))
                        dirty_add(&dirty, manager.windows[i].frame);
            }
            if (changed && dirty.count) {
                for (uint32_t i = 0; i < dirty.count; i++) {
                    struct rect r = rect_clip(dirty.rects[i], screen->width, screen->height);
                    if (r.w && r.h) surface_copy(screen, background, &r, (uint32_t)r.x, (uint32_t)r.y);
                }
                render_target(&renderer, screen);
                /* Draw the complete scene graph, but clip each pass to the
                   rectangles touched by this input batch. This keeps widget
                   code simple while avoiding work outside the dirty regions. */
                for (uint32_t i = 0; i < dirty.count; i++) {
                    struct rect r = rect_clip(dirty.rects[i], screen->width, screen->height);
                    if (!r.w || !r.h) continue;
                    renderer.clip(&renderer, r.x, r.y, r.w, r.h);
                    manager.paint(&manager, &renderer);
                    context_paint(&context_menu, &renderer);
                    cursor.paint(&cursor, &renderer);
                    renderer.no_clip(&renderer);
                }
                if (!present_rects(&fb, screen, &dirty)) break;
            }
            /* Four milliseconds keeps cursor latency low without busy-spinning. */
            sleepms(4);
        }
    }
    cursor.destroy(&cursor);
    surface_destroy(screen);
    surface_destroy(background);
    gfx_free(clients);
    gfx_free(windows);
    if (!once) input.close(&input);
    fbd_close(&fb);
    puts("wing: desktop frame presented\n");
    return 0;
}
