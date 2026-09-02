/**
 * @file process.c
 * @brief Process table, file descriptors, address spaces, argv/envp and wait/reap logic.
 *
 */

#include "kernel/scheduler/process.h"
#include "arch/x86/paging.h"
#include "kernel/core/debug.h"
#include "kernel/memory/heap.h"
#include "kernel/memory/physical.h"
#include "kernel/scheduler/thread.h"

static struct process table[PROCESS_MAX];
static struct process *current;
static uint32_t nextpid = 1;

static int inrange(int fd) {
    return fd >= 0 && fd < PROCESS_FDS;
}

static void closeslot(struct fdslot *slot) {
    if (!slot || !slot->used) {
        return;
    }

    if (slot->type == FD_FILE && slot->file) {
        slot->file->close(slot->file);
    }
    if (slot->type == FD_DIR && slot->dir) {
        slot->dir->close(slot->dir);
    }

    memset(slot, 0, sizeof(*slot));
}

static int bind_impl(struct process *self, int fd, struct vfile *file, uint32_t flags) {
    if (!self || !file || !inrange(fd)) {
        return -1;
    }

    closeslot(&self->fds[fd]);
    self->fds[fd].used = 1;
    self->fds[fd].flags = flags;
    self->fds[fd].type = FD_FILE;
    self->fds[fd].file = file;
    if (file->_path) {
        strncpy(self->fds[fd].path, file->_path, sizeof(self->fds[fd].path));
    }
    return fd;
}


static int binddir_impl(struct process *self, int fd, struct vdir *dir, const char *path, uint32_t flags) {
    if (!self || !dir || !inrange(fd)) {
        return -1;
    }

    closeslot(&self->fds[fd]);
    self->fds[fd].used = 1;
    self->fds[fd].flags = flags;
    self->fds[fd].type = FD_DIR;
    self->fds[fd].dir = dir;
    strncpy(self->fds[fd].path, path ? path : "", sizeof(self->fds[fd].path));
    return fd;
}

static int alloc_impl(struct process *self, struct vfile *file, uint32_t flags) {
    if (!self || !file) {
        return -1;
    }

    for (int fd = 3; fd < PROCESS_FDS; fd++) {
        if (!self->fds[fd].used) {
            self->fds[fd].used = 1;
            self->fds[fd].flags = flags;
            self->fds[fd].type = FD_FILE;
            self->fds[fd].file = file;
            if (file->_path) {
                strncpy(self->fds[fd].path, file->_path, sizeof(self->fds[fd].path));
            }
            return fd;
        }
    }

    return -1;
}

static int allocdir_impl(struct process *self, struct vdir *dir, const char *path, uint32_t flags) {
    if (!self || !dir) {
        return -1;
    }

    for (int fd = 3; fd < PROCESS_FDS; fd++) {
        if (!self->fds[fd].used) {
            self->fds[fd].used = 1;
            self->fds[fd].flags = flags;
            self->fds[fd].type = FD_DIR;
            self->fds[fd].dir = dir;
            strncpy(self->fds[fd].path, path ? path : "", sizeof(self->fds[fd].path));
            return fd;
        }
    }

    return -1;
}

static struct vfile *file_impl(struct process *self, int fd) {
    if (!self || !inrange(fd) || !self->fds[fd].used || self->fds[fd].type != FD_FILE) {
        return nil;
    }
    return self->fds[fd].file;
}

static struct vdir *dir_impl(struct process *self, int fd) {
    if (!self || !inrange(fd) || !self->fds[fd].used || self->fds[fd].type != FD_DIR) {
        return nil;
    }
    return self->fds[fd].dir;
}

static int close_impl(struct process *self, int fd) {
    if (!self || !inrange(fd) || !self->fds[fd].used) {
        return -1;
    }

    closeslot(&self->fds[fd]);
    return 0;
}

static int used_impl(struct process *self, int fd) {
    return self && inrange(fd) && self->fds[fd].used &&
        ((self->fds[fd].type == FD_FILE && self->fds[fd].file) ||
         (self->fds[fd].type == FD_DIR && self->fds[fd].dir));
}

static int isdir_impl(struct process *self, int fd) {
    return self && inrange(fd) && self->fds[fd].used && self->fds[fd].type == FD_DIR;
}

static const char *path_impl(struct process *self, int fd) {
    if (!self || !inrange(fd) || !self->fds[fd].used) {
        return nil;
    }
    if (self->fds[fd].path[0]) {
        return self->fds[fd].path;
    }
    if (self->fds[fd].type == FD_FILE && self->fds[fd].file) {
        return self->fds[fd].file->_path;
    }
    return nil;
}

static uint32_t flags_impl(struct process *self, int fd) {
    if (!self || !inrange(fd) || !self->fds[fd].used) {
        return 0;
    }
    return self->fds[fd].flags;
}

void process(struct process *proc, uint32_t pid, uint32_t ppid, const char *name) {
    memset(proc, 0, sizeof(*proc));
    proc->bind = bind_impl;
    proc->binddir = binddir_impl;
    proc->alloc = alloc_impl;
    proc->allocdir = allocdir_impl;
    proc->file = file_impl;
    proc->dir = dir_impl;
    proc->close = close_impl;
    proc->used = used_impl;
    proc->isdir = isdir_impl;
    proc->path = path_impl;
    proc->flags = flags_impl;
    proc->pid = pid;
    proc->ppid = ppid;
    proc->state = PROCESS_READY;
    proc->space = pagekernel() ? pagekernel() : pagedirectory();
    strncpy(proc->name, name ? name : "process", sizeof(proc->name));
    strncpy(proc->cwd, "/", sizeof(proc->cwd));
}

struct process *processinit(const char *name) {
    process(&table[0], nextpid++, 0, name ? name : "init");
    table[0].state = PROCESS_RUNNING;
    current = &table[0];
    return current;
}

struct process *processspawn(const char *name) {
    uint32_t parent = current ? current->pid : 0;

    for (unsigned i = 0; i < PROCESS_MAX; i++) {
        if (table[i].state == PROCESS_UNUSED) {
            process(&table[i], nextpid++, parent, name ? name : "process");
            KLOG("process", "spawn pid=%u ppid=%u name=%s", table[i].pid, table[i].ppid, table[i].name);
            return &table[i];
        }
    }

    return nil;
}

int processinherit(struct process *child, struct process *parent, struct vfs *fs) {
    if (!child || !parent) {
        return 0;
    }

    strncpy(child->cwd, parent->cwd, sizeof(child->cwd));

    for (int fd = 0; fd < PROCESS_FDS; fd++) {
        if (!parent->used(parent, fd) || (parent->fds[fd].fdflags & FD_CLOEXEC)) {
            continue;
        }

        if (parent->fds[fd].type == FD_FILE) {
            uint32_t flags = parent->flags(parent, fd);
            struct vfile *file = parent->file(parent, fd);

            if (!file) {
                continue;
            }

            file->retain(file);
            if (child->bind(child, fd, file, flags) < 0) {
                file->close(file);
            }
            continue;
        }

        if (parent->fds[fd].type == FD_DIR) {
            const char *path = parent->path(parent, fd);
            struct vdir *dir = parent->dir(parent, fd);
            if (!dir) {
                continue;
            }
            dir->retain(dir);
            if (child->binddir(child, fd, dir, path, parent->flags(parent, fd)) < 0) {
                dir->close(dir);
            }
        }
    }

    return 1;
}


void processcloseall(struct process *proc) {
    if (!proc) {
        return;
    }

    for (int fd = 0; fd < PROCESS_FDS; fd++) {
        closeslot(&proc->fds[fd]);
    }
}

void processcloexec(struct process *proc) {
    if (!proc) {
        return;
    }

    for (int fd = 0; fd < PROCESS_FDS; fd++) {
        if (proc->fds[fd].used && (proc->fds[fd].fdflags & FD_CLOEXEC)) {
            closeslot(&proc->fds[fd]);
        }
    }
}

void processclearargs(struct process *proc) {
    if (!proc) {
        return;
    }

    if (proc->args) {
        kfree(proc->args);
    }
    proc->args = nil;
    proc->argc = 0;
    for (unsigned i = 0; i < countof(proc->argv); i++) {
        proc->argv[i] = nil;
    }
}

/**
 * Parse a tiny shell-style command line into argv storage owned by `proc`.
 *
 * This is not POSIX shell parsing.  It only understands the quote and escape
 * rules Monarch currently needs before a real libc/shell grows.  Keeping this
 * parser here centralises argv construction so every exec path behaves the same.
 */
int processsetargs(struct process *proc, const char *command) {
    char *src;
    char *dst;

    if (!proc || !command) {
        return 0;
    }

    processclearargs(proc);
    proc->args = kstrdup(command);
    if (!proc->args) {
        return 0;
    }

    src = trim(proc->args);
    dst = proc->args;

    while (*src && proc->argc < PROCESS_ARGS) {
        int quote = 0;
        int escape = 0;

        while (space(*src)) {
            src++;
        }
        if (!*src) {
            break;
        }

        proc->argv[proc->argc++] = dst;
        while (*src) {
            char ch = *src++;

            if (escape) {
                *dst++ = ch;
                escape = 0;
                continue;
            }

            if (ch == '\\') {
                escape = 1;
                continue;
            }

            if (quote) {
                if (ch == quote) {
                    quote = 0;
                } else {
                    *dst++ = ch;
                }
                continue;
            }

            if (ch == '\'' || ch == '"') {
                quote = ch;
                continue;
            }

            if (space(ch)) {
                break;
            }

            *dst++ = ch;
        }

        *dst++ = '\0';
    }

    proc->argv[proc->argc] = nil;
    return proc->argc > 0;
}



void processclearenv(struct process *proc) {
    if (!proc) {
        return;
    }

    if (proc->envs) {
        kfree(proc->envs);
    }
    proc->envs = nil;
    proc->envc = 0;
    for (unsigned i = 0; i < countof(proc->env); i++) {
        proc->env[i] = nil;
    }
}

/**
 * Store an inherited environment block in process-owned memory.
 *
 * The block is the traditional userspace format: `NAME=value\0...\0\0`.
 * Pointers in proc->env[] point into the copied backing buffer, so freeing the
 * environment is a single kfree plus clearing the pointer array.
 */
int processsetenvblock(struct process *proc, const char *block, size_t size) {
    size_t used = 0;
    char *p;

    if (!proc) {
        return 0;
    }

    processclearenv(proc);
    if (!block || !size || (size == 1u && block[0] == '\0')) {
        return 1;
    }
    if (size > PROCESS_ENV_TEXT) {
        return 0;
    }

    proc->envs = kcalloc(size + 1u, 1u);
    if (!proc->envs) {
        return 0;
    }

    memcpy(proc->envs, block, size);
    proc->envs[size] = '\0';
    p = proc->envs;

    while (used < size && *p && proc->envc < PROCESS_ENVS) {
        size_t len = strlen(p);
        if (len == 0 || used + len >= size + 1u) {
            break;
        }
        proc->env[proc->envc++] = p;
        used += len + 1u;
        p += len + 1u;
    }

    proc->env[proc->envc] = nil;
    return 1;
}

int processcopyenv(struct process *dst, struct process *src) {
    size_t size = 0;

    if (!dst) {
        return 0;
    }
    if (!src || !src->envs || src->envc <= 0) {
        return processsetenvblock(dst, nil, 0);
    }

    for (int i = 0; i < src->envc; i++) {
        size += strlen(src->env[i]) + 1u;
    }
    size++;
    return processsetenvblock(dst, src->envs, size);
}

int processargc(struct process *proc) {
    return proc ? proc->argc : 0;
}

char **processargv(struct process *proc) {
    return proc ? proc->argv : nil;
}


int processenvc(struct process *proc) {
    return proc ? proc->envc : 0;
}

char **processenv(struct process *proc) {
    return proc ? proc->env : nil;
}

uintptr_t processspace(struct process *proc) {
    return proc && proc->space ? proc->space : pagekernel();
}

int processnewspace(struct process *proc) {
    uintptr_t space;

    if (!proc) {
        return 0;
    }

    if (proc->space && proc->space != pagekernel()) {
        pagedestroy(proc->space);
    }

    proc->image_start = 0;
    proc->image_end = 0;
    proc->image_entry = 0;
    proc->stack_base = 0;
    proc->stack_top = 0;
    proc->stack_pointer = 0;
    proc->user_argv = 0;
    proc->user_env = 0;
    proc->heap_base = 0;
    proc->heap_break = 0;
    proc->heap_limit = 0;

    space = pagecreate();
    if (!space) {
        proc->space = pagekernel();
        return 0;
    }

    proc->space = space;
    return 1;
}

void processsetspace(struct process *proc, uintptr_t space) {
    if (proc) {
        proc->space = space ? space : pagekernel();
    }
}


static int writeuser(struct process *proc, uintptr_t dst, const void *src, size_t size) {
    const uint8_t *bytes = src;

    while (size) {
        uintptr_t p = pagegetin(processspace(proc), dst);
        size_t chunk;

        if (!p) {
            return 0;
        }

        chunk = PAGE_SIZE - (dst & (PAGE_SIZE - 1u));
        if (chunk > size) {
            chunk = size;
        }

        memcpy((void *)p, bytes, chunk);
        dst += chunk;
        bytes += chunk;
        size -= chunk;
    }

    return 1;
}

static int writeword(struct process *proc, uintptr_t dst, uintptr_t value) {
    return writeuser(proc, dst, &value, sizeof(value));
}

void processclearstack(struct process *proc) {
    if (!proc || !proc->stack_base || !proc->stack_top || proc->stack_top <= proc->stack_base) {
        return;
    }

    for (uintptr_t v = proc->stack_base; v < proc->stack_top; v += PAGE_SIZE) {
        uintptr_t p = pagegetin(processspace(proc), v);
        if (p) {
            pageunmapin(processspace(proc), v);
            pmmfree(p & ~(uintptr_t)(PAGE_SIZE - 1u));
        }
    }

    proc->stack_base = 0;
    proc->stack_top = 0;
    proc->stack_pointer = 0;
    proc->user_argv = 0;
    proc->user_env = 0;
}

/**
 * Build the initial userspace stack for ring-3 entry.
 *
 * The resulting stack looks like a normal C call frame for our crt0:
 * fake return address, argc, argv, envp.  Argument and environment strings are
 * copied onto the process stack so user code never points into kernel memory.
 */
int processbuildstack(struct process *proc) {
    uintptr_t base = PROCESS_STACK_TOP - PROCESS_STACK_SIZE;
    uintptr_t top = PROCESS_STACK_TOP;
    uintptr_t sp = top;
    uintptr_t argptrs[PROCESS_ARGS];
    uintptr_t envptrs[PROCESS_ENVS];

    if (!proc || !proc->space || proc->space == pagekernel() || proc->argc <= 0) {
        return 0;
    }

    processclearstack(proc);

    for (uintptr_t v = base; v < top; v += PAGE_SIZE) {
        uintptr_t p = pmmalloc();
        if (!p) {
            processclearstack(proc);
            return 0;
        }
        memset((void *)p, 0, PAGE_SIZE);
        if (!pagemapin(proc->space, v, p, PAGE_WRITE | PAGE_USER)) {
            pmmfree(p);
            processclearstack(proc);
            return 0;
        }
    }

    proc->stack_base = base;
    proc->stack_top = top;

    for (int i = proc->envc - 1; i >= 0; i--) {
        size_t len = strlen(proc->env[i]) + 1u;
        sp -= len;
        if (sp < base || !writeuser(proc, sp, proc->env[i], len)) {
            processclearstack(proc);
            return 0;
        }
        envptrs[i] = sp;
    }

    for (int i = proc->argc - 1; i >= 0; i--) {
        size_t len = strlen(proc->argv[i]) + 1u;
        sp -= len;
        if (sp < base || !writeuser(proc, sp, proc->argv[i], len)) {
            processclearstack(proc);
            return 0;
        }
        argptrs[i] = sp;
    }

    sp &= ~(uintptr_t)0xFu;

    sp -= sizeof(uintptr_t);
    if (sp < base || !writeword(proc, sp, 0)) {
        processclearstack(proc);
        return 0;
    }
    for (int i = proc->envc - 1; i >= 0; i--) {
        sp -= sizeof(uintptr_t);
        if (sp < base || !writeword(proc, sp, envptrs[i])) {
            processclearstack(proc);
            return 0;
        }
    }
    proc->user_env = sp;

    sp -= sizeof(uintptr_t);
    if (sp < base || !writeword(proc, sp, 0)) {
        processclearstack(proc);
        return 0;
    }
    for (int i = proc->argc - 1; i >= 0; i--) {
        sp -= sizeof(uintptr_t);
        if (sp < base || !writeword(proc, sp, argptrs[i])) {
            processclearstack(proc);
            return 0;
        }
    }
    proc->user_argv = sp;

    sp -= sizeof(uintptr_t);
    if (sp < base || !writeword(proc, sp, proc->user_env)) {
        processclearstack(proc);
        return 0;
    }

    sp -= sizeof(uintptr_t);
    if (sp < base || !writeword(proc, sp, proc->user_argv)) {
        processclearstack(proc);
        return 0;
    }

    sp -= sizeof(uintptr_t);
    if (sp < base || !writeword(proc, sp, (uintptr_t)proc->argc)) {
        processclearstack(proc);
        return 0;
    }

    sp -= sizeof(uintptr_t);
    if (sp < base || !writeword(proc, sp, 0)) {
        processclearstack(proc);
        return 0;
    }

    proc->stack_pointer = sp;
    return 1;
}


uintptr_t processstack(struct process *proc) {
    return proc ? proc->stack_pointer : 0;
}

char **processuserargv(struct process *proc) {
    return proc ? (char **)proc->user_argv : nil;
}

char **processuserenv(struct process *proc) {
    return proc ? (char **)proc->user_env : nil;
}

void processfinish(struct process *proc, int status) {
    if (!proc || proc->state == PROCESS_UNUSED || proc->state == PROCESS_ZOMBIE) {
        return;
    }

    proc->exit_code = status;
    proc->state = PROCESS_ZOMBIE;
    KLOG("process", "finish pid=%u status=%d name=%s", proc->pid, status, proc->name);
}

void processclearimage(struct process *proc) {
    uintptr_t start;
    uintptr_t end;

    if (!proc || !proc->image_start || !proc->image_end || proc->image_end <= proc->image_start) {
        return;
    }

    start = proc->image_start & ~(uintptr_t)(PAGE_SIZE - 1u);
    end = alignup(proc->image_end, PAGE_SIZE);

    for (uintptr_t v = start; v < end; v += PAGE_SIZE) {
        uintptr_t p = pagegetin(processspace(proc), v);
        if (p) {
            pageunmapin(processspace(proc), v);
            pmmfree(p & ~(uintptr_t)(PAGE_SIZE - 1u));
        }
    }

    proc->image_start = 0;
    proc->image_end = 0;
    proc->image_entry = 0;
}

void processreap(struct process *proc) {
    if (!proc || proc == current || proc->state == PROCESS_UNUSED || proc->state == PROCESS_RUNNING) {
        return;
    }

    KLOG("process", "reap pid=%u status=%d name=%s", proc->pid, proc->exit_code, proc->name);
    processcloseall(proc);
    processclearargs(proc);
    processclearenv(proc);
    processclearimage(proc);
    processclearstack(proc);
    if (proc->space && proc->space != pagekernel()) {
        pagedestroy(proc->space);
    }
    memset(proc, 0, sizeof(*proc));
}

struct process *processfind(uint32_t pid) {
    for (unsigned i = 0; i < PROCESS_MAX; i++) {
        if (table[i].state != PROCESS_UNUSED && table[i].pid == pid) {
            return &table[i];
        }
    }
    return nil;
}

static int childof(struct process *parent, struct process *child, int pid) {
    if (!parent || !child || child->state == PROCESS_UNUSED || child->ppid != parent->pid) {
        return 0;
    }
    return pid <= 0 || child->pid == (uint32_t)pid;
}

long processwait(int pid, int *status) {
    struct process *parent = current;
    unsigned loops = 0;

    if (!parent) {
        KLOG("wait", "no current process target=%d", pid);
        return -1;
    }

    KLOG("wait", "parent=%u target=%d begin", parent->pid, pid);

    for (;;) {
        struct process *zombie = nil;
        int found = 0;

        for (unsigned i = 0; i < PROCESS_MAX; i++) {
            if (!childof(parent, &table[i], pid)) {
                continue;
            }

            found = 1;
            if (loops == 0 || (loops % 128u) == 0) {
                KTRACE("wait", "parent=%u sees child pid=%u state=%s exit=%d", parent->pid, table[i].pid, processstate(table[i].state), table[i].exit_code);
            }
            if (table[i].state == PROCESS_ZOMBIE) {
                zombie = &table[i];
                break;
            }
        }

        if (zombie) {
            uint32_t waited = zombie->pid;
            int code = zombie->exit_code;
            KLOG("wait", "parent=%u reaping child=%u exit=%d", parent->pid, waited, code);
            if (status) {
                *status = code;
            }
            threadreap(zombie);
            processreap(zombie);
            KLOG("wait", "parent=%u done child=%u", parent->pid, waited);
            return (long)waited;
        }

        if (!found) {
            KLOG("wait", "parent=%u no child target=%d", parent->pid, pid);
            return -1;
        }

        loops++;
        if ((loops % 128u) == 0) {
            KTRACE("wait", "parent=%u target=%d still waiting loops=%u", parent->pid, pid, loops);
        }
        yield();
    }
}


struct process *processcurrent(void) {
    return current;
}

void processswitch(struct process *proc) {
    if (!proc || proc->state == PROCESS_UNUSED || proc->state == PROCESS_ZOMBIE) {
        return;
    }

    if (current && current->state == PROCESS_RUNNING) {
        current->state = PROCESS_READY;
    }

    current = proc;
    current->state = PROCESS_RUNNING;
    pageswitch(current->space ? current->space : pagekernel());
}

void processsetstate(struct process *proc, enum processstate state) {
    if (proc) {
        proc->state = state;
    }
}

void processsetname(struct process *proc, const char *name) {
    if (proc && name) {
        strncpy(proc->name, name, sizeof(proc->name));
    }
}

void processsetcwd(struct process *proc, const char *cwd) {
    if (proc && cwd) {
        strncpy(proc->cwd, cwd, sizeof(proc->cwd));
    }
}

const char *processcwd(struct process *proc) {
    return proc ? proc->cwd : "/";
}

void processsetimage(struct process *proc, uintptr_t start, uintptr_t end, uintptr_t entry) {
    if (!proc) {
        return;
    }
    proc->image_start = start;
    proc->image_end = end;
    proc->image_entry = entry;
    proc->heap_base = (end + PAGE_SIZE - 1u) & ~(uintptr_t)(PAGE_SIZE - 1u);
    proc->heap_break = proc->heap_base;
    proc->heap_limit = 0x80000000u;
}

void processeach(void (*iter)(void *ctx, struct process *proc), void *ctx) {
    if (!iter) {
        return;
    }

    for (unsigned i = 0; i < PROCESS_MAX; i++) {
        if (table[i].state != PROCESS_UNUSED) {
            iter(ctx, &table[i]);
        }
    }
}

const char *processstate(enum processstate state) {
    switch (state) {
        case PROCESS_READY: return "ready";
        case PROCESS_RUNNING: return "running";
        case PROCESS_SLEEPING: return "sleeping";
        case PROCESS_ZOMBIE: return "zombie";
        default: return "unused";
    }
}
