/**
 * @file pipe.c
 * @brief Small cooperative pipe implementation exposed as vfile pairs.
 *
 */

#include "kernel/fs/pipe.h"
#include "kernel/memory/heap.h"
#include "kernel/scheduler/thread.h"

#define PIPE_SIZE 4096u
#define PIPE_NAMED_MAX 64u

struct pipebuf {
    uint8_t data[PIPE_SIZE];
    size_t read;
    size_t write;
    size_t used;
    unsigned readers;
    unsigned writers;
    unsigned refs;
};

struct namedpipe {
    int used;
    char path[VFS_PATH];
    struct pipebuf pipe;
};

struct pipefile {
    struct vfile file;
    struct pipebuf *pipe;
    struct namedpipe *named;
    int writer;
};

static struct namedpipe namedpipes[PIPE_NAMED_MAX];

static struct pipefile *owner(struct vfile *file) {
    return (struct pipefile *)file;
}

/** Read from a pipe, blocking cooperatively until data or writer EOF. */
static int piperead(struct vfile *file, char *buffer, size_t size) {
    struct pipefile *pf = owner(file);
    struct pipebuf *pipe;
    size_t done = 0;

    if (!pf || pf->writer || !buffer) {
        return -1;
    }

    pipe = pf->pipe;
    while (done < size) {
        while (pipe->used == 0) {
            if (pipe->writers == 0) {
                return (int)done;
            }
            if (file->_nonblock) return done ? (int)done : -EAGAIN;
            threadsleep(1);
        }

        buffer[done++] = (char)pipe->data[pipe->read];
        pipe->read = (pipe->read + 1u) % PIPE_SIZE;
        pipe->used--;
    }

    return (int)done;
}

/** Write to a pipe, blocking cooperatively while the ring buffer is full. */
static int pipewrite(struct vfile *file, const char *buffer, size_t size) {
    struct pipefile *pf = owner(file);
    struct pipebuf *pipe;
    size_t done = 0;

    if (!pf || !pf->writer || (!buffer && size)) {
        return -1;
    }

    pipe = pf->pipe;
    while (done < size) {
        if (pipe->readers == 0) {
            return done ? (int)done : -1;
        }

        while (pipe->used == PIPE_SIZE) {
            if (pipe->readers == 0) {
                return done ? (int)done : -1;
            }
            if (file->_nonblock) return done ? (int)done : -EAGAIN;
            threadsleep(1);
        }

        pipe->data[pipe->write] = (uint8_t)buffer[done++];
        pipe->write = (pipe->write + 1u) % PIPE_SIZE;
        pipe->used++;
    }

    return (int)done;
}

static uint32_t pipeready(struct vfile *file, uint32_t events) {
    struct pipefile *pf = owner(file);
    struct pipebuf *pipe = pf ? pf->pipe : nil;
    uint32_t result = 0;
    if (!pipe) return POLLERR | POLLHUP;
    if ((events & POLLIN) && (pipe->used || pipe->writers == 0)) result |= POLLIN;
    if ((events & POLLOUT) && (pipe->used < PIPE_SIZE || pipe->readers == 0)) result |= POLLOUT;
    if (pipe->writers == 0) result |= POLLHUP;
    if (pipe->readers == 0) result |= POLLERR;
    return result;
}

static void piperetain(struct vfile *file) {
    if (file) {
        file->_refs++;
    }
}

static void pipeclose(struct vfile *file) {
    struct pipefile *pf = owner(file);
    struct pipebuf *pipe;

    if (!pf) {
        return;
    }

    if (pf->file._refs > 1) {
        pf->file._refs--;
        return;
    }

    pipe = pf->pipe;
    if (pipe) {
        if (pf->writer) {
            if (pipe->writers) {
                pipe->writers--;
            }
        } else if (pipe->readers) {
            pipe->readers--;
        }

        if (pipe->refs) {
            pipe->refs--;
        }
            if (pipe->refs == 0 && !pf->named) {
            kfree(pipe);
        } else if (pipe->refs == 0 && pf->named && !pf->named->used) {
            memset(pf->named, 0, sizeof(*pf->named));
        }
    }

    if (pf->file._path) {
        kfree(pf->file._path);
    }
    kfree(pf);
}

static struct vfile *pipefile(struct pipebuf *pipe, struct namedpipe *named,
                              int writer, const char *name, const char *mode) {
    struct pipefile *pf = kcalloc(1, sizeof(*pf));
    if (!pf) return nil;
    pf->pipe = pipe;
    pf->named = named;
    pf->writer = writer;
    pf->file._path = kstrdup(name ? name : "pipe");
    pf->file.read = piperead;
    pf->file.write = pipewrite;
    pf->file.ready = pipeready;
    pf->file.retain = piperetain;
    pf->file.close = pipeclose;
    pf->file._refs = 1;
    pf->file._nonblock = mode && strchr(mode, 'n') != nil;
    if (writer) pipe->writers++; else pipe->readers++;
    pipe->refs++;
    return &pf->file;
}

int pipeopen(struct vfile **read_end, struct vfile **write_end) {
    struct pipebuf *pipe;

    if (!read_end || !write_end) {
        return 0;
    }

    *read_end = nil;
    *write_end = nil;

    pipe = kcalloc(1, sizeof(*pipe));
    if (!pipe) {
        return 0;
    }

    *read_end = pipefile(pipe, nil, 0, "pipe:[read]", nil);
    *write_end = pipefile(pipe, nil, 1, "pipe:[write]", nil);
    if (!*read_end || !*write_end) {
        if (*read_end) {
            (*read_end)->close(*read_end);
        }
        if (*write_end) {
            (*write_end)->close(*write_end);
        }
        if (!*read_end && !*write_end) {
            kfree(pipe);
        }
        return 0;
    }

    return 1;
}

int pipe_mkfifo(const char *path) {
    if (!path || !*path) return 0;
    for (unsigned i = 0; i < PIPE_NAMED_MAX; i++)
        if (namedpipes[i].used && strcmp(namedpipes[i].path, path) == 0) return 1;
    for (unsigned i = 0; i < PIPE_NAMED_MAX; i++) {
        if (!namedpipes[i].used && namedpipes[i].pipe.refs == 0) {
            memset(&namedpipes[i], 0, sizeof(namedpipes[i]));
            namedpipes[i].used = 1;
            strncpy(namedpipes[i].path, path, sizeof(namedpipes[i].path));
            return 1;
        }
    }
    return 0;
}

struct vfile *pipe_named_open(const char *path, const char *mode) {
    if (!path || !mode) return nil;
    for (unsigned i = 0; i < PIPE_NAMED_MAX; i++) {
        if (namedpipes[i].used && strcmp(namedpipes[i].path, path) == 0) {
            struct vfile *file = pipefile(&namedpipes[i].pipe, &namedpipes[i],
                                          strchr(mode, 'w') != nil, path, mode);
            struct pipefile *pf = owner(file);
            if (!file || !pf || pf->file._nonblock) return file;
            while (pf->writer ? pf->pipe->readers == 0 : pf->pipe->writers == 0) {
                if (!namedpipes[i].used) break;
                threadsleep(1);
            }
            return file;
        }
    }
    return nil;
}

int pipe_unlink(const char *path) {
    if (!path) return 0;
    for (unsigned i = 0; i < PIPE_NAMED_MAX; i++) {
        if (namedpipes[i].used && strcmp(namedpipes[i].path, path) == 0) {
            namedpipes[i].used = 0;
            if (namedpipes[i].pipe.refs == 0)
                memset(&namedpipes[i], 0, sizeof(namedpipes[i]));
            return 1;
        }
    }
    return 0;
}
