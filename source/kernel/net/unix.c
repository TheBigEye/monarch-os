/**
 * @file unix.c
 * @brief Minimal local SOCK_STREAM socketpair transport.
 */
#include "kernel/net/unix.h"
#include "kernel/memory/heap.h"
#include "kernel/scheduler/thread.h"

#define UNIX_BUFFER 4096u

struct unix_buffer {
    uint8_t data[UNIX_BUFFER];
    size_t read;
    size_t write;
    size_t used;
    unsigned refs;
};

struct unix_end {
    struct vfile file;
    struct unix_buffer *in;
    struct unix_buffer *out;
    struct unix_end *peer;
};

static struct unix_end *owner(struct vfile *file) {
    return (struct unix_end *)file;
}

static int read_stream(struct vfile *file, char *buffer, size_t size) {
    struct unix_end *end = owner(file);
    size_t done = 0;
    if (!end || !buffer) return -1;
    while (done < size) {
        while (!end->in->used) {
            if (!end->peer) return (int)done;
            if (file->_nonblock) return done ? (int)done : -EAGAIN;
            threadsleep(1);
        }
        buffer[done++] = (char)end->in->data[end->in->read];
        end->in->read = (end->in->read + 1u) % UNIX_BUFFER;
        end->in->used--;
    }
    return (int)done;
}

static int write_stream(struct vfile *file, const char *buffer, size_t size) {
    struct unix_end *end = owner(file);
    size_t done = 0;
    if (!end || (!buffer && size)) return -1;
    while (done < size) {
        if (!end->peer) return done ? (int)done : -EAGAIN;
        while (end->out->used == UNIX_BUFFER) {
            if (!end->peer) return done ? (int)done : -EAGAIN;
            if (file->_nonblock) return done ? (int)done : -EAGAIN;
            threadsleep(1);
        }
        end->out->data[end->out->write] = (uint8_t)buffer[done++];
        end->out->write = (end->out->write + 1u) % UNIX_BUFFER;
        end->out->used++;
    }
    return (int)done;
}

static uint32_t ready_stream(struct vfile *file, uint32_t events) {
    struct unix_end *end = owner(file);
    uint32_t result = 0;
    if (!end) return POLLERR | POLLHUP;
    if ((events & POLLIN) && (end->in->used || !end->peer)) result |= POLLIN;
    if ((events & POLLOUT) && end->peer && end->out->used < UNIX_BUFFER) result |= POLLOUT;
    if (!end->peer) result |= POLLHUP | POLLERR;
    return result;
}

static void retain_stream(struct vfile *file) {
    if (file) file->_refs++;
}

static void close_stream(struct vfile *file) {
    struct unix_end *end = owner(file);
    if (!end) return;
    if (file->_refs > 1) { file->_refs--; return; }
    if (end->peer) {
        end->peer->peer = nil;
        end->peer = nil;
    }
    if (end->in->refs) end->in->refs--;
    if (end->out->refs) end->out->refs--;
    if (!end->in->refs) kfree(end->in);
    if (!end->out->refs) kfree(end->out);
    if (file->_path) kfree(file->_path);
    kfree(end);
}

static struct vfile *endpoint(struct unix_buffer *in, struct unix_buffer *out) {
    struct unix_end *end = kcalloc(1, sizeof(*end));
    if (!end) return nil;
    end->in = in;
    end->out = out;
    end->file._path = kstrdup("socket:[unix]");
    end->file.read = read_stream;
    end->file.write = write_stream;
    end->file.ready = ready_stream;
    end->file.retain = retain_stream;
    end->file.close = close_stream;
    end->file._refs = 1;
    in->refs++;
    out->refs++;
    return &end->file;
}

int unix_socketpair(struct vfile **left, struct vfile **right) {
    struct unix_buffer *a;
    struct unix_buffer *b;
    struct unix_end *la;
    struct unix_end *rb;
    if (!left || !right) return 0;
    *left = nil; *right = nil;
    a = kcalloc(1, sizeof(*a));
    b = kcalloc(1, sizeof(*b));
    if (!a || !b) { if (a) kfree(a); if (b) kfree(b); return 0; }
    *left = endpoint(a, b);
    *right = endpoint(b, a);
    if (!*left || !*right) {
        if (*left) (*left)->close(*left);
        if (*right) (*right)->close(*right);
        if (!*left && !*right) { kfree(a); kfree(b); }
        return 0;
    }
    la = owner(*left);
    rb = owner(*right);
    la->peer = rb;
    rb->peer = la;
    return 1;
}

#define UNIX_LISTENERS 64u
#define UNIX_BACKLOG 16u

struct unix_control {
    struct vfile file;
    int bound;
    int listening;
    unsigned listener;
    char path[UNIX_PATH_MAX];
};

struct unix_listener {
    int used;
    int listening;
    char path[UNIX_PATH_MAX];
    struct vfile *queue[UNIX_BACKLOG];
    uint32_t head;
    uint32_t count;
};

static struct unix_listener listeners[UNIX_LISTENERS];

static struct unix_control *control(struct vfile *file) {
    return (struct unix_control *)file;
}

static void close_control(struct vfile *file) {
    struct unix_control *socket = control(file);
    if (!socket) return;
    if (socket->bound && socket->listener < UNIX_LISTENERS) {
        struct unix_listener *listener = &listeners[socket->listener];
        if (!listener->used || strcmp(listener->path, socket->path) != 0) {
            listener = nil;
        }
        for (uint32_t i = 0; listener && i < listener->count; i++) {
            uint32_t index = (listener->head + i) % UNIX_BACKLOG;
            if (listener->queue[index]) listener->queue[index]->close(listener->queue[index]);
        }
        if (listener) listener->listening = 0;
    }
    if (file->_path) kfree(file->_path);
    kfree(socket);
}

static struct unix_listener *find_path(const char *path) {
    for (unsigned i = 0; i < UNIX_LISTENERS; i++)
        if (listeners[i].used && strcmp(listeners[i].path, path) == 0)
            return &listeners[i];
    return nil;
}

static struct unix_listener *find_listener(const char *path) {
    struct unix_listener *listener = find_path(path);
    return listener && listener->listening ? listener : nil;
}

int unix_socket(struct vfile **out) {
    struct unix_control *socket;
    if (!out) return 0;
    *out = nil;
    socket = kcalloc(1, sizeof(*socket));
    if (!socket) return 0;
    socket->file._path = kstrdup("socket:[unbound]");
    socket->file.close = close_control;
    socket->file._refs = 1;
    if (!socket->file._path) { kfree(socket); return 0; }
    *out = &socket->file;
    return 1;
}

int unix_bind(struct vfile *file, const struct sockaddr_un *address) {
    struct unix_control *socket = control(file);
    if (!socket || !address || address->family != AF_UNIX || !address->path[0] || socket->bound) return 0;
    if (find_path(address->path)) return 0;
    for (unsigned i = 0; i < UNIX_LISTENERS; i++) {
        if (!listeners[i].used) {
            listeners[i].used = 1;
            strncpy(listeners[i].path, address->path, sizeof(listeners[i].path));
            socket->bound = 1;
            socket->listener = i;
            strncpy(socket->path, address->path, sizeof(socket->path));
            return 1;
        }
    }
    return 0;
}

int unix_listen(struct vfile *file, uint32_t backlog) {
    struct unix_control *socket = control(file);
    if (!socket || !socket->bound || socket->listener >= UNIX_LISTENERS) return 0;
    socket->listening = 1;
    listeners[socket->listener].listening = 1;
    if (backlog > UNIX_BACKLOG) backlog = UNIX_BACKLOG;
    unused(backlog);
    return 1;
}

int unix_connect(struct vfile *file, const struct sockaddr_un *address,
                 struct vfile **connected) {
    struct unix_control *socket = control(file);
    struct unix_listener *listener;
    struct vfile *left;
    struct vfile *right;
    if (!connected) return 0;
    *connected = nil;
    if (!socket || !address || address->family != AF_UNIX) return 0;
    listener = find_listener(address->path);
    if (!listener) return 0;
    if (listener->count >= UNIX_BACKLOG || !unix_socketpair(&left, &right)) return 0;
    listener->queue[(listener->head + listener->count) % UNIX_BACKLOG] = right;
    listener->count++;
    *connected = left;
    return 1;
}

struct vfile *unix_accept(struct vfile *file) {
    struct unix_control *socket = control(file);
    struct unix_listener *listener;
    struct vfile *result;
    if (!socket || !socket->listening || socket->listener >= UNIX_LISTENERS) return nil;
    listener = &listeners[socket->listener];
    if (!listener->count) return nil;
    result = listener->queue[listener->head];
    listener->queue[listener->head] = nil;
    listener->head = (listener->head + 1u) % UNIX_BACKLOG;
    listener->count--;
    return result;
}

int unix_unlink(const char *path) {
    struct unix_listener *listener;
    if (!path || !(listener = find_path(path))) return 0;
    for (uint32_t i = 0; i < listener->count; i++) {
        uint32_t index = (listener->head + i) % UNIX_BACKLOG;
        if (listener->queue[index]) listener->queue[index]->close(listener->queue[index]);
    }
    memset(listener, 0, sizeof(*listener));
    return 1;
}
