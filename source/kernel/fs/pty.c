/**
 * @file pty.c
 * @brief Small pseudoterminal with canonical input and echo.
 */
#include "kernel/fs/pty.h"
#include "kernel/memory/heap.h"
#include "kernel/scheduler/thread.h"
#include "kernel/core/debug.h"

#define PTY_BUFFER 4096u
#define PTY_LINE 256u

struct pty_state;
struct pty_end {
    struct vfile file;
    struct pty_state *state;
    int master;
};

struct pty_state {
    int used;
    struct pty_end *master;
    struct pty_end *slave;
    uint8_t input[PTY_BUFFER];
    size_t input_read;
    size_t input_write;
    size_t input_used;
    uint8_t output[PTY_BUFFER];
    size_t output_read;
    size_t output_write;
    size_t output_used;
    char line[PTY_LINE];
    size_t line_used;
    int eof;
    struct winsize size;
    struct termios termios;
};


static struct pty_end *endof(struct vfile *file) {
    return (struct pty_end *)file;
}

static int push(uint8_t *buffer, size_t *write, size_t *used, uint8_t value) {
    if (*used >= PTY_BUFFER) return 0;
    buffer[(*write)++] = value;
    if (*write == PTY_BUFFER) *write = 0;
    (*used)++;
    return 1;
}

static int pop(uint8_t *buffer, size_t *read, size_t *used, uint8_t *value) {
    if (!*used) return 0;
    *value = buffer[(*read)++];
    if (*read == PTY_BUFFER) *read = 0;
    (*used)--;
    return 1;
}

static int flush_line(struct pty_state *state) {
    for (size_t i = 0; i < state->line_used; i++)
        if (!push(state->input, &state->input_write, &state->input_used,
                  (uint8_t)state->line[i])) return 0;
    state->line_used = 0;
    return 1;
}

static int read_pty(struct vfile *file, char *buffer, size_t size) {
    struct pty_end *end = endof(file);
    struct pty_state *state = end ? end->state : nil;
    uint8_t value;
    size_t done = 0;
    if (!state || !buffer) return -1;
    while (done < size) {
        if (end->master && !state->slave)
            return done ? (int)done : 0;
        int ok = end->master ? pop(state->output, &state->output_read,
                                   &state->output_used, &value)
                             : pop(state->input, &state->input_read,
                                   &state->input_used, &value);
        if (!ok) {
            if (!end->master && state->eof) {
                state->eof = 0;
                return (int)done;
            }
            if (file->_nonblock) return done ? (int)done : -EAGAIN;
            threadsleep(1);
            continue;
        }
        buffer[done++] = (char)value;
    }
    return (int)done;
}

static int write_pty(struct vfile *file, const char *buffer, size_t size) {
    struct pty_end *end = endof(file);
    struct pty_state *state = end ? end->state : nil;
    size_t done = 0;
    if (!state || (!buffer && size)) return -1;
    while (done < size) {
        uint8_t value = (uint8_t)buffer[done];
        if (end->master && !state->slave)
            return done ? (int)done : -EIO;
        if (end->master) {
            if ((state->termios.lflag & TERMIOS_ICANON) && value == 0x04u) {
                if (state->line_used) flush_line(state);
                else state->eof = 1;
                done++;
                continue;
            }
            if (state->termios.lflag & TERMIOS_ICANON) {
                if (state->line_used >= PTY_LINE) {
                    if (file->_nonblock) return done ? (int)done : -EAGAIN;
                    threadsleep(1);
                    continue;
                }
                state->line[state->line_used++] = (char)value;
                /* USH redraws Enter and Backspace itself. Echo printable
                   characters here, but do not echo control keys twice. */
                if ((state->termios.lflag & TERMIOS_ECHO) && value >= ' ' && value != 0x7Fu)
                    if (!push(state->output, &state->output_write,
                              &state->output_used, value)) return done ? (int)done : -EAGAIN;
                done++;
                if (value == '\n') flush_line(state);
                continue;
            }
            if (!push(state->input, &state->input_write, &state->input_used, value)) {
                if (file->_nonblock) return done ? (int)done : -EAGAIN;
                threadsleep(1);
                continue;
            }
        } else if (!push(state->output, &state->output_write, &state->output_used, value)) {
            if (file->_nonblock) return done ? (int)done : -EAGAIN;
            threadsleep(1);
            continue;
        }
        done++;
    }
    return (int)done;
}

static uint32_t ready_pty(struct vfile *file, uint32_t events) {
    struct pty_end *end = endof(file);
    struct pty_state *state = end ? end->state : nil;
    uint32_t ready = 0;
    if (!state) return POLLERR | POLLHUP;
    if (end->master) {
        if (!state->slave) ready |= POLLHUP | POLLERR;
        if ((events & POLLIN) && (state->output_used || !state->slave)) ready |= POLLIN;
        if ((events & POLLOUT) && state->input_used < PTY_BUFFER && state->line_used < PTY_LINE) ready |= POLLOUT;
    } else {
        if ((events & POLLIN) && (state->input_used || state->eof)) ready |= POLLIN;
        if ((events & POLLOUT) && state->output_used < PTY_BUFFER) ready |= POLLOUT;
    }
    return ready;
}

static void retain_pty(struct vfile *file) { if (file) file->_refs++; }

static void close_pty(struct vfile *file) {
    struct pty_end *end = endof(file);
    if (!end) return;
    if (file->_refs > 1) {
        file->_refs--;
        KLOG("pty", "drop %s refs=%u", end->master ? "master" : "slave", (unsigned)file->_refs);
        return;
    }
    if (end->master) end->state->master = nil; else end->state->slave = nil;
    KLOG("pty", "close %s master=%p slave=%p", end->master ? "master" : "slave",
         (void *)end->state->master, (void *)end->state->slave);
    if (file->_path) kfree(file->_path);
    if (!end->state->master && !end->state->slave) {
        end->state->used = 0;
        kfree(end->state);
    }
    kfree(end);
}

static struct pty_end *make_end(struct pty_state *state, int master) {
    struct pty_end *end = kcalloc(1, sizeof(*end));
    if (!end) return nil;
    end->state = state;
    end->master = master;
    end->file._path = kstrdup(master ? "pty:[master]" : "pty:[slave]");
    end->file.read = read_pty;
    end->file.write = write_pty;
    end->file.ready = ready_pty;
    end->file.retain = retain_pty;
    end->file.close = close_pty;
    end->file._refs = 1;
    return end;
}

static struct pty_state *find(struct vfile *file) {
    struct pty_end *end = file ? endof(file) : nil;
    return end ? end->state : nil;
}

int pty_pair(struct vfile **master, struct vfile **slave) {
    struct pty_state *state;
    if (!master || !slave) return 0;
    state = kcalloc(1, sizeof(*state));
    if (!state) return 0;
    state->used = 1;
    state->size.rows = 25; state->size.cols = 80;
    state->termios.lflag = TERMIOS_ICANON | TERMIOS_ECHO | TERMIOS_ISIG;
    state->master = make_end(state, 1);
    state->slave = make_end(state, 0);
    if (!state->master || !state->slave) return 0;
    *master = &state->master->file;
    *slave = &state->slave->file;
    return 1;
}

int pty_getwinsize(struct vfile *file, struct winsize *out) { struct pty_state *s=find(file); if (!s||!out)return 0; *out=s->size; return 1; }
int pty_setwinsize(struct vfile *file, const struct winsize *size) { struct pty_state *s=find(file); if (!s||!size||!size->rows||!size->cols)return 0; s->size=*size; return 1; }
int pty_gettermios(struct vfile *file, struct termios *out) { struct pty_state *s=find(file); if (!s||!out)return 0; *out=s->termios; return 1; }
int pty_settermios(struct vfile *file, const struct termios *termios) { struct pty_state *s=find(file); if (!s||!termios)return 0; s->termios=*termios; return 1; }
