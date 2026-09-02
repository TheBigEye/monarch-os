/**
 * @file syscall.c
 * @brief Linux-like int 0x80 syscall dispatcher and safe userspace copy helpers.
 *
 */

#include "arch/x86/syscall.h"
#include "arch/x86/idt.h"
#include "arch/x86/paging.h"
#include "arch/x86/rtc.h"
#include "arch/x86/pit.h"
#include "kernel/core/debug.h"
#include "kernel/core/exec.h"
#include "kernel/fs/pipe.h"
#include "kernel/net/unix.h"
#include "kernel/fs/pty.h"
#include "kernel/core/version.h"
#include "kernel/scheduler/process.h"
#include "kernel/scheduler/thread.h"
#include "kernel/memory/physical.h"
#include "drivers/video/framebuffer.h"

extern void syscallentry(void);

static struct vfs *root;

static struct process *proc(void) {
    return processcurrent();
}

static struct vfile *fdfile(int fd) {
    struct process *p = proc();
    return p ? p->file(p, fd) : nil;
}

static void sync_cwd(void) {
    struct process *p = proc();
    if (root && p) {
        root->chdir(root, processcwd(p));
    }
}

static void save_cwd(void) {
    struct process *p = proc();
    char cwd[VFS_PATH];
    if (root && p && root->cwd(root, cwd, sizeof(cwd))) {
        processsetcwd(p, cwd);
    }
}

static const char *mode(uint32_t flags) {
    int nonblock = (flags & O_NONBLOCK) != 0;
    if ((flags & OWRITE) == 0) return nonblock ? "rn" : "r";
    if (flags & OAPPEND) return nonblock ? "an" : "a";
    if (flags & OTRUNC) return nonblock ? "wn" : "w";
    if (flags & OREAD) return nonblock ? "+n" : "+";
    return nonblock ? "wn" : "w";
}


static int userprocess(void) {
    struct process *p = proc();
    return p && processspace(p) != pagekernel();
}

static int userrange(uintptr_t address, size_t size) {
    if (!size) {
        return 1;
    }
    if (address < PAGE_USER_BASE || address >= PAGE_USER_TOP) {
        return 0;
    }
    if (size > PAGE_USER_TOP - address) {
        return 0;
    }
    return 1;
}

/** Copy bytes from userspace into kernel memory after validating the range. */
static int copyin(void *dst, const void *src, size_t size) {
    struct process *p = proc();
    uintptr_t address = (uintptr_t)src;
    uint8_t *out = dst;

    if (!dst || (!src && size)) {
        return 0;
    }

    if (!userprocess()) {
        memcpy(dst, src, size);
        return 1;
    }

    if (!userrange(address, size)) {
        return 0;
    }

    while (size) {
        uintptr_t physical = pagegetin(processspace(p), address);
        size_t chunk;

        if (!physical) {
            return 0;
        }

        chunk = PAGE_SIZE - (address & (PAGE_SIZE - 1u));
        if (chunk > size) {
            chunk = size;
        }

        memcpy(out, (const void *)physical, chunk);
        out += chunk;
        address += chunk;
        size -= chunk;
    }

    return 1;
}

/** Copy bytes from kernel memory into userspace after validating the range. */
static int copyout(void *dst, const void *src, size_t size) {
    struct process *p = proc();
    uintptr_t address = (uintptr_t)dst;
    const uint8_t *in = src;

    if ((!dst && size) || (!src && size)) {
        return 0;
    }

    if (!userprocess()) {
        memcpy(dst, src, size);
        return 1;
    }

    if (!userrange(address, size)) {
        return 0;
    }

    while (size) {
        uintptr_t physical = pagegetin(processspace(p), address);
        size_t chunk;

        if (!physical) {
            return 0;
        }

        chunk = PAGE_SIZE - (address & (PAGE_SIZE - 1u));
        if (chunk > size) {
            chunk = size;
        }

        memcpy((void *)physical, in, chunk);
        in += chunk;
        address += chunk;
        size -= chunk;
    }

    return 1;
}

/** Copy a NUL-terminated userspace string into a bounded kernel buffer. */
static int copyinstr(char *dst, const char *src, size_t size) {
    char ch;

    if (!dst || !src || !size) {
        return 0;
    }

    if (!userprocess()) {
        strncpy(dst, src, size);
        return 1;
    }

    for (size_t i = 0; i + 1u < size; i++) {
        if (!copyin(&ch, src + i, 1)) {
            return 0;
        }
        dst[i] = ch;
        if (!ch) {
            return 1;
        }
    }

    dst[size - 1u] = '\0';
    return 0;
}

void syscall(struct vfs *fs) {
    struct process *p;

    root = fs;

    if (!root) {
        return;
    }

    p = processinit("bush");
    if (!p) {
        return;
    }

    p->bind(p, 0, root->open(root, "/dev/keyboard", "r"), OREAD);
    p->bind(p, 1, root->open(root, "/dev/console", "w"), OWRITE);
    p->bind(p, 2, root->open(root, "/dev/console", "w"), OWRITE);

    /*
     * Use a trap gate (type 0xF), not an interrupt gate (type 0xE).
     *
     * An interrupt gate clears IF on entry. That is fine for hardware IRQs,
     * but terrible for syscalls that may block or yield (for example SYS_SLEEP):
     * the scheduler would switch away with interrupts disabled, the PIT would
     * stop ticking, and the sleeping thread would never wake up.
     *
     * 0xEF = present | DPL3 | 32-bit trap gate.
     */
    idtset(0x80, (uintptr_t)syscallentry, 0xEF);
}

long sysopen(const char *path, uint32_t flags) {
    struct process *p = proc();
    struct vfile *file;
    struct vdir *dir;
    struct vstat st;
    char kpath[VFS_PATH];
    int exists;
    int fd;

    if (!root || !p) {
        return -ENODEV;
    }
    if (!copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }

    sync_cwd();
    if (!root->realpath(root, kpath, kpath, sizeof(kpath))) {
        return -EFAULT;
    }
    exists = root->stat(root, kpath, &st);
    if (exists && st.type == VFS_DIR) {
        if (flags & OWRITE) {
            return -EISDIR;
        }
        dir = root->opendir(root, kpath);
        if (!dir) {
            return -ENOTDIR;
        }
        fd = p->allocdir(p, dir, kpath, flags | OREAD);
        if (fd < 0) {
            dir->close(dir);
            return -EMFILE;
        }
        return fd;
    }

    file = root->open(root, kpath, mode(flags));
    if (!file) {
        return exists && st.type == VFS_DIR ? -EISDIR : -ENOENT;
    }

    fd = p->alloc(p, file, flags);
    if (fd >= 0) file->_nonblock = (flags & O_NONBLOCK) != 0;
    if (fd < 0) {
        file->close(file);
        return -EMFILE;
    }

    return fd;
}

long sysread(int fd, void *buffer, size_t size) {
    struct process *p = proc();
    struct vfile *file = fdfile(fd);
    char temp[256];
    size_t amount;
    int count;

    if (p && p->isdir(p, fd)) {
        return -EISDIR;
    }
    if (!file) {
        return -EBADF;
    }
    if (!buffer && size) {
        return -EFAULT;
    }
    if (!size) {
        return 0;
    }

    if (!userprocess()) {
        return file->read(file, buffer, size);
    }

    amount = min(size, sizeof(temp));
    count = file->read(file, temp, amount);
    if (count <= 0) {
        return count;
    }

    return copyout(buffer, temp, (size_t)count) ? count : -EFAULT;
}

long syswrite(int fd, const void *buffer, size_t size) {
    struct process *p = proc();
    struct vfile *file = fdfile(fd);
    size_t done = 0;
    char temp[4096];

    if (p && p->isdir(p, fd)) {
        return -EISDIR;
    }
    if (!file) {
        return -EBADF;
    }
    if (!buffer && size) {
        return -EFAULT;
    }
    if (!size) {
        return 0;
    }

    if (!userprocess()) {
        return file->write(file, buffer, size);
    }

    while (done < size) {
        size_t amount = min(size - done, sizeof(temp));
        int count;

        if (!copyin(temp, (const uint8_t *)buffer + done, amount)) {
            return done ? (long)done : -EFAULT;
        }

        count = file->write(file, temp, amount);
        if (count < 0) {
            return done ? (long)done : -EIO;
        }
        if (count == 0) {
            break;
        }
        done += (size_t)count;
        if ((size_t)count < amount) {
            break;
        }
    }

    return (long)done;
}

long sysclose(int fd) {
    struct process *p = proc();
    if (!p) {
        return -ENODEV;
    }
    return p->close(p, fd) == 0 ? 0 : -EBADF;
}

static long sysselect(const struct select_args *user_args) {
    struct process *p = proc();
    struct select_args args;
    fd_set rin;
    fd_set win;
    fd_set ein;
    fd_set rout;
    fd_set wout;
    fd_set eout;
    struct timeval tv;
    uint32_t start;
    uint32_t wait_ticks = 0;
    long ready;

    if (!p || !user_args || !copyin(&args, user_args, sizeof(args)) ||
        args.nfds < 0 || args.nfds > (int32_t)MONARCH_FD_SETSIZE)
        return -EINVAL;
    FD_ZERO(&rin); FD_ZERO(&win); FD_ZERO(&ein);
    if (args.readfds && !copyin(&rin, (const void *)args.readfds, sizeof(rin))) return -EFAULT;
    if (args.writefds && !copyin(&win, (const void *)args.writefds, sizeof(win))) return -EFAULT;
    if (args.exceptfds && !copyin(&ein, (const void *)args.exceptfds, sizeof(ein))) return -EFAULT;
    if (args.timeout) {
        if (!copyin(&tv, (const void *)args.timeout, sizeof(tv)) ||
            tv.seconds < 0 || tv.microseconds < 0 || tv.microseconds >= 1000000)
            return -EINVAL;
        wait_ticks = (uint32_t)tv.seconds * 100u +
                     ((uint32_t)tv.microseconds + 9999u) / 10000u;
    }
    start = ticks();

    for (;;) {
        FD_ZERO(&rout); FD_ZERO(&wout); FD_ZERO(&eout);
        ready = 0;
        for (int fd = 0; fd < args.nfds; fd++) {
            struct vfile *file;
            uint32_t events = 0;
            uint32_t revents = 0;
            if (FD_ISSET(fd, &rin)) events |= POLLIN;
            if (FD_ISSET(fd, &win)) events |= POLLOUT;
            if (FD_ISSET(fd, &ein)) events |= POLLERR;
            if (!events) continue;
            if (!p->used(p, fd)) return -EBADF;
            file = p->file(p, fd);
            if (!file) return -EBADF;
            revents = file->ready ? file->ready(file, events) : events;
            if ((revents & POLLIN) && FD_ISSET(fd, &rin)) { FD_SET(fd, &rout); ready++; }
            if ((revents & POLLOUT) && FD_ISSET(fd, &win)) { FD_SET(fd, &wout); ready++; }
            if ((revents & (POLLERR | POLLHUP)) && FD_ISSET(fd, &ein)) { FD_SET(fd, &eout); ready++; }
        }
        if (ready || !args.timeout || wait_ticks == 0 || ticks() - start >= wait_ticks) {
            if (args.readfds && !copyout((void *)args.readfds, &rout, sizeof(rout))) return -EFAULT;
            if (args.writefds && !copyout((void *)args.writefds, &wout, sizeof(wout))) return -EFAULT;
            if (args.exceptfds && !copyout((void *)args.exceptfds, &eout, sizeof(eout))) return -EFAULT;
            return ready;
        }
        threadsleep(1);
    }
}

static int copy_sockaddr(struct sockaddr_un *out, const struct sockaddr_un *user) {
    return out && user && copyin(out, user, sizeof(*out)) &&
           out->family == AF_UNIX && out->path[0];
}

static long sysbrk(uintptr_t requested) {
    struct process *p = proc();
    uintptr_t oldbrk;
    uintptr_t oldpage;
    uintptr_t newpage;
    if (!p || !userprocess()) return -EINVAL;
    if (!p->heap_base) return -ENOMEM;
    oldbrk = p->heap_break;
    if (!requested) return (long)oldbrk;
    if (requested < p->heap_base || requested > p->heap_limit) return -ENOMEM;
    oldpage = (oldbrk + PAGE_SIZE - 1u) & ~(uintptr_t)(PAGE_SIZE - 1u);
    newpage = (requested + PAGE_SIZE - 1u) & ~(uintptr_t)(PAGE_SIZE - 1u);
    if (newpage > oldpage) {
        for (uintptr_t address = oldpage; address < newpage; address += PAGE_SIZE) {
            uintptr_t physical = pmmalloc();
            if (!physical || !pagemapin(processspace(p), address, physical, PAGE_WRITE | PAGE_USER)) {
                if (physical) pmmfree(physical);
                return -ENOMEM;
            }
            memset((void *)physical, 0, PAGE_SIZE);
        }
    } else if (newpage < oldpage) {
        for (uintptr_t address = newpage; address < oldpage; address += PAGE_SIZE) {
            uintptr_t physical = pagegetin(processspace(p), address);
            if (physical) { pageunmapin(processspace(p), address); pmmfree(physical); }
        }
    }
    p->heap_break = requested;
    return (long)requested;
}

static long sysptypair(int *user_fds) {
    struct process *p = proc();
    struct vfile *master = nil;
    struct vfile *slave = nil;
    int fds[2];
    if (!p || !user_fds || !pty_pair(&master, &slave)) return -ENOMEM;
    fds[0] = p->alloc(p, master, OREAD | OWRITE);
    if (fds[0] < 0) { master->close(master); slave->close(slave); return -EMFILE; }
    fds[1] = p->alloc(p, slave, OREAD | OWRITE);
    if (fds[1] < 0) { p->close(p, fds[0]); slave->close(slave); return -EMFILE; }
    if (!copyout(user_fds, fds, sizeof(fds))) {
        p->close(p, fds[0]); p->close(p, fds[1]); return -EFAULT;
    }
    return 0;
}

static long syssocket(uint32_t domain, uint32_t type, uint32_t protocol) {
    struct process *p = proc();
    struct vfile *file = nil;
    int fd;
    if (!p || domain != AF_UNIX || type != SOCK_STREAM || protocol != 0 ||
        !unix_socket(&file)) return -EINVAL;
    fd = p->alloc(p, file, OREAD | OWRITE);
    if (fd < 0) { file->close(file); return -EMFILE; }
    return fd;
}

static long sysbind(int fd, const struct sockaddr_un *user_address) {
    struct sockaddr_un address;
    struct vfile *file = fdfile(fd);
    if (!file || !copy_sockaddr(&address, user_address)) return -EINVAL;
    return unix_bind(file, &address) ? 0 : -EINVAL;
}

static long syslisten(int fd, uint32_t backlog) {
    struct vfile *file = fdfile(fd);
    if (!file) return -EBADF;
    return unix_listen(file, backlog) ? 0 : -EINVAL;
}

static long sysaccept(int fd) {
    struct process *p = proc();
    struct vfile *file = fdfile(fd);
    struct vfile *accepted;
    if (!p || !file) return -EBADF;
    for (;;) {
        accepted = unix_accept(file);
        if (accepted) {
            fd = p->alloc(p, accepted, OREAD | OWRITE);
            if (fd < 0) { accepted->close(accepted); return -EMFILE; }
            return fd;
        }
        if (file->_nonblock) return -EAGAIN;
        threadsleep(1);
    }
}

static long sysconnect(int fd, const struct sockaddr_un *user_address) {
    struct process *p = proc();
    struct sockaddr_un address;
    struct vfile *old;
    struct vfile *connected = nil;
    if (!p || !copy_sockaddr(&address, user_address)) return -EINVAL;
    old = fdfile(fd);
    if (!old) return -EBADF;
    for (;;) {
        if (unix_connect(old, &address, &connected)) {
            old->close(old);
            p->fds[fd].file = connected;
            connected->_nonblock = (p->fds[fd].flags & O_NONBLOCK) != 0;
            return 0;
        }
        if (old->_nonblock) return -EAGAIN;
        threadsleep(1);
    }
}

static long syssocketpair(const struct socketpair_args *user_args) {
    struct process *p = proc();
    struct socketpair_args args;
    struct vfile *left = nil;
    struct vfile *right = nil;
    int fds[2];
    if (!p || !user_args || !copyin(&args, user_args, sizeof(args)) ||
        args.domain != AF_UNIX || args.type != SOCK_STREAM || args.protocol != 0 ||
        !args.fds) return -EINVAL;
    if (!unix_socketpair(&left, &right)) return -ENOMEM;
    fds[0] = p->alloc(p, left, OREAD | OWRITE);
    if (fds[0] < 0) { left->close(left); right->close(right); return -EMFILE; }
    fds[1] = p->alloc(p, right, OREAD | OWRITE);
    if (fds[1] < 0) { p->close(p, fds[0]); right->close(right); return -EMFILE; }
    if (!copyout((void *)args.fds, fds, sizeof(fds))) {
        p->close(p, fds[0]); p->close(p, fds[1]); return -EFAULT;
    }
    return 0;
}

static long sysmkfifo(const char *user_path) {
    char path[VFS_PATH];
    if (!root || !user_path || !copyinstr(path, user_path, sizeof(path))) return -EFAULT;
    if (!root->realpath(root, path, path, sizeof(path))) return -EINVAL;
    return pipe_mkfifo(path) ? 0 : -ENOMEM;
}

static long syspoll(struct pollfd *user_fds, uint32_t count, int32_t timeout_ms) {
    struct process *p = proc();
    struct pollfd fds[64];
    uint32_t limit;
    uint32_t start;
    uint32_t wait_ticks;

    if (!p || (!user_fds && count) || count > countof(fds) || timeout_ms < -1)
        return -EINVAL;
    limit = count * sizeof(fds[0]);
    if (count && !copyin(fds, user_fds, limit)) return -EFAULT;
    start = ticks();
    wait_ticks = timeout_ms > 0 ? ((uint32_t)timeout_ms + 9u) / 10u : 0;

    for (;;) {
        long ready = 0;
        for (uint32_t i = 0; i < count; i++) {
            struct vfile *file;
            fds[i].revents = 0;
            if (fds[i].fd < 0) continue;
            if (fds[i].fd >= PROCESS_FDS || !p->used(p, fds[i].fd)) {
                fds[i].revents = POLLNVAL;
                ready++;
                continue;
            }
            file = p->file(p, fds[i].fd);
            if (!file) {
                fds[i].revents = POLLNVAL;
            } else if (file->ready) {
                fds[i].revents = file->ready(file, fds[i].events);
            } else {
                fds[i].revents = fds[i].events & (POLLIN | POLLOUT);
            }
            if (fds[i].revents) ready++;
        }
        if (ready || timeout_ms == 0 ||
            (timeout_ms > 0 && ticks() - start >= wait_ticks)) {
            if (count && !copyout(user_fds, fds, limit)) return -EFAULT;
            return ready;
        }
        threadsleep(1);
    }
}

long syslseek(int fd, long offset, int whence) {
    struct process *p = proc();
    struct vfile *file = fdfile(fd);
    size_t result;

    if (p && p->isdir(p, fd)) {
        return -EISDIR;
    }
    if (!file) {
        return -EBADF;
    }
    if (!file->seek) {
        return -ESPIPE;
    }

    if (!file->seek(file, offset, whence, &result)) {
        return -EINVAL;
    }
    return (long)result;
}

static long dupfd(struct process *p, int oldfd, int newfd, int minimum) {
    if (!p || oldfd < 0 || oldfd >= PROCESS_FDS || !p->used(p, oldfd)) {
        return -EBADF;
    }

    if (newfd >= 0) {
        if (newfd >= PROCESS_FDS) {
            return -EBADF;
        }
        if (oldfd == newfd) {
            return newfd;
        }
    } else {
        if (minimum < 0 || minimum >= PROCESS_FDS) {
            return -EINVAL;
        }
        for (int fd = minimum; fd < PROCESS_FDS; fd++) {
            if (!p->used(p, fd)) {
                newfd = fd;
                break;
            }
        }
        if (newfd < 0) {
            return -EMFILE;
        }
    }

    if (p->fds[oldfd].type == FD_FILE) {
        struct vfile *file = p->file(p, oldfd);
        if (!file) {
            return -EBADF;
        }
        file->retain(file);
        if (p->bind(p, newfd, file, p->flags(p, oldfd)) < 0) {
            file->close(file);
            return -EBADF;
        }
        p->fds[newfd].fdflags = 0;
        return newfd;
    }

    if (p->fds[oldfd].type == FD_DIR) {
        const char *path = p->path(p, oldfd);
        struct vdir *dir = p->dir(p, oldfd);
        if (!dir) {
            return -EBADF;
        }
        dir->retain(dir);
        if (p->binddir(p, newfd, dir, path, p->flags(p, oldfd)) < 0) {
            dir->close(dir);
            return -EBADF;
        }
        p->fds[newfd].fdflags = 0;
        return newfd;
    }

    return -EBADF;
}

long sysdup(int fd) {
    return dupfd(proc(), fd, -1, 0);
}

long syspipe(int fds[2]) {
    struct process *p = proc();
    struct vfile *read_end;
    struct vfile *write_end;
    int pair[2];

    if (!p) {
        return -ENODEV;
    }
    if (!fds) {
        return -EFAULT;
    }
    if (!pipeopen(&read_end, &write_end)) {
        return -ENOMEM;
    }

    pair[0] = p->alloc(p, read_end, OREAD);
    if (pair[0] < 0) {
        read_end->close(read_end);
        write_end->close(write_end);
        return -EMFILE;
    }

    pair[1] = p->alloc(p, write_end, OWRITE);
    if (pair[1] < 0) {
        p->close(p, pair[0]);
        write_end->close(write_end);
        return -EMFILE;
    }

    if (!copyout(fds, pair, sizeof(pair))) {
        p->close(p, pair[0]);
        p->close(p, pair[1]);
        return -EFAULT;
    }

    return 0;
}


long sysdup2(int oldfd, int newfd) {
    return dupfd(proc(), oldfd, newfd, 0);
}

long sysfcntl(int fd, uint32_t command, uintptr_t arg) {
    struct process *p = proc();

    if (!p || fd < 0 || fd >= PROCESS_FDS || !p->used(p, fd)) {
        return -EBADF;
    }

    switch (command) {
        case F_DUPFD:
            return dupfd(p, fd, -1, (int)arg);
        case F_GETFD:
            return (long)(p->fds[fd].fdflags & FD_CLOEXEC);
        case F_SETFD:
            p->fds[fd].fdflags = (arg & FD_CLOEXEC) ? FD_CLOEXEC : 0;
            return 0;
        case F_GETFL:
            return (long)p->fds[fd].flags;
        case F_SETFL: {
            struct vfile *file = p->file(p, fd);
            if (!file) return -EBADF;
            p->fds[fd].flags = (p->fds[fd].flags & ~(uint32_t)O_NONBLOCK) |
                               ((uint32_t)arg & O_NONBLOCK);
            file->_nonblock = (p->fds[fd].flags & O_NONBLOCK) != 0;
            return 0;
        }
        default:
            return -EINVAL;
    }
}


long sysioctl(int fd, uint32_t request, uintptr_t arg) {
    struct process *p = proc();
    struct vfile *file = fdfile(fd);
    const char *path;
    long result;

    if ((request == TIOCGWINSZ || request == TIOCSWINSZ ||
         request == TCGETS || request == TCSETS) && file) {
        struct winsize size;
        if (request == TIOCGWINSZ) {
            if (!arg || !pty_getwinsize(file, &size) || !copyout((void *)arg, &size, sizeof(size))) return -ENOTTY;
            return 0;
        }
        if (request == TIOCSWINSZ) {
            if (!arg || !copyin(&size, (const void *)arg, sizeof(size)) || !pty_setwinsize(file, &size)) return -ENOTTY;
            return 0;
        }
        {
            struct termios termios;
            if (request == TCGETS) {
                if (!arg || !pty_gettermios(file, &termios) || !copyout((void *)arg, &termios, sizeof(termios))) return -ENOTTY;
                return 0;
            }
            if (!arg || !copyin(&termios, (const void *)arg, sizeof(termios)) || !pty_settermios(file, &termios)) return -ENOTTY;
            return 0;
        }
    }

    if (request == FB_BLIT) {
        struct framebuffer *fb = framebuffer_get();
        struct fb_blit blit;
        uint32_t row[256];

        if (!root || !p || fd < 0 || fd >= PROCESS_FDS || !p->used(p, fd) ||
            strcmp(p->path(p, fd), "/dev/fb0") != 0 || !fb || !fb->ready(fb) ||
            !arg || !copyin(&blit, (const void *)arg, sizeof(blit)) ||
            !blit.w || !blit.h || blit.x >= fb->width || blit.y >= fb->height ||
            blit.w > fb->width - blit.x || blit.h > fb->height - blit.y ||
            blit.pitch < blit.w * 4u) {
            return -EINVAL;
        }
        for (uint32_t y = 0; y < blit.h; y++) {
            uint32_t done = 0;
            while (done < blit.w) {
                uint32_t count = min(blit.w - done, 256u);
                uintptr_t src = blit.pixels + (size_t)y * blit.pitch + (size_t)done * 4u;
                if (!copyin(row, (const void *)src, (size_t)count * 4u)) return -EFAULT;
                memcpy(fb->address + (size_t)(blit.y + y) * fb->pitch +
                       (size_t)(blit.x + done) * 4u, row, (size_t)count * 4u);
                done += count;
            }
        }
        return 0;
    }

    if (request == FB_GETMAP) {
        struct framebuffer *fb = framebuffer_get();
        struct fbmap map;
        const uintptr_t user_base = 0xB0000000u;
        uintptr_t physical;
        uintptr_t aligned;
        uintptr_t offset;
        size_t pages;

        if (!root || !p || fd < 0 || fd >= PROCESS_FDS || !p->used(p, fd) ||
            strcmp(p->path(p, fd), "/dev/fb0") != 0 || !fb || !fb->ready(fb) || !arg) {
            return -ENOTTY;
        }
        physical = fb->physical;
        aligned = physical & ~(uintptr_t)(PAGE_SIZE - 1u);
        offset = physical - aligned;
        pages = (fb->bytes + (size_t)offset + PAGE_SIZE - 1u) / PAGE_SIZE;
        if (user_base + pages * PAGE_SIZE >= PAGE_USER_TOP) return -ENOMEM;
        for (size_t i = 0; i < pages; i++) {
            if (!pagemapin(processspace(p), user_base + i * PAGE_SIZE,
                           aligned + i * PAGE_SIZE, PAGE_WRITE | PAGE_USER)) {
                return -ENOMEM;
            }
        }
        memset(&map, 0, sizeof(map));
        map.address = (uint32_t)(user_base + offset);
        map.width = fb->width;
        map.height = fb->height;
        map.pitch = fb->pitch;
        map.bytes = (uint32_t)fb->bytes;
        map.bpp = fb->bpp;
        map.red_pos = fb->red_position;
        map.red_size = fb->red_mask_size;
        map.green_pos = fb->green_position;
        map.green_size = fb->green_mask_size;
        map.blue_pos = fb->blue_position;
        map.blue_size = fb->blue_mask_size;
        return copyout((void *)arg, &map, sizeof(map)) ? 0 : -EFAULT;
    }

    if (!root || !p || fd < 0 || fd >= PROCESS_FDS || !p->used(p, fd)) {
        return !p || fd < 0 || fd >= PROCESS_FDS || !p->used(p, fd) ? -EBADF : -ENODEV;
    }

    path = p->path(p, fd);
    if (!path) {
        return -ENOTTY;
    }

    if (request == AUDIO_GETINFO) {
        struct audioinfo info;
        if (!arg) {
            return -EFAULT;
        }
        result = root->ioctl(root, path, request, (uintptr_t)&info);
        if (result < 0) {
            return result;
        }
        return copyout((void *)arg, &info, sizeof(info)) ? result : -EFAULT;
    }

    result = root->ioctl(root, path, request, arg);
    return result;
}

long syswaitpid(int pid, int *status) {
    int code = 0;
    long result;

    if (status && userprocess()) {
        if (!userrange((uintptr_t)status, sizeof(*status)) || !pagegetin(processspace(proc()), (uintptr_t)status)) {
            return -EFAULT;
        }
    }

    result = processwait(pid, &code);
    if (result < 0) {
        return -ECHILD;
    }
    if (status && !copyout(status, &code, sizeof(code))) {
        return -EFAULT;
    }
    return result;
}

long sysexecve(const char *path) {
    char kpath[VFS_PATH];

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }
    sync_cwd();
    return execreplace(root, kpath) < 0 ? -ENOEXEC : 0;
}


long sysspawn(const char *command, const char *envblock, size_t envsize) {
    struct execresult result;
    char kcommand[VFS_PATH];
    char kenv[PROCESS_ENV_TEXT + 1u];
    const char *env = nil;

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(kcommand, command, sizeof(kcommand))) {
        return -EFAULT;
    }

    if (envblock) {
        if (envsize == 0 || envsize > PROCESS_ENV_TEXT || !copyin(kenv, envblock, envsize)) {
            return -EFAULT;
        }
        kenv[envsize] = '\0';
        env = kenv;
    }

    sync_cwd();
    if (!execspawnenv(root, kcommand, env, env ? envsize : 0, processcurrent(), &result)) {
        KLOG("exec", "spawn syscall failed command=%s error=%s", kcommand, execerror());
        return -ENOEXEC;
    }

    return (long)result.pid;
}


long sysstat(const char *path, struct vstat *out) {
    struct vstat st;
    char kpath[VFS_PATH];

    if (!root) {
        return -ENODEV;
    }
    if (!out || !copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }

    sync_cwd();
    if (!root->stat(root, kpath, &st)) {
        return -ENOENT;
    }

    return copyout(out, &st, sizeof(st)) ? 0 : -EFAULT;
}

long sysgetdents(int fd, struct sysdirent *entries, size_t count) {
    struct process *p = proc();
    struct vdir *dir;
    struct ventry entry;
    size_t used = 0;

    if (!p) {
        return -ENODEV;
    }
    if (!entries && count) {
        return -EFAULT;
    }

    dir = p->dir(p, fd);
    if (!dir) {
        return p->used(p, fd) ? -ENOTDIR : -EBADF;
    }

    while (used < count && dir->read(dir, &entry)) {
        struct sysdirent out;
        strncpy(out.name, entry.name, sizeof(out.name));
        out.type = (uint32_t)entry.type;
        out.size = (uint32_t)entry.size;
        if (!copyout(&entries[used], &out, sizeof(out))) {
            return used ? (long)used : -EFAULT;
        }
        used++;
    }

    return (long)used;
}

long syschdir(const char *path) {
    char kpath[VFS_PATH];

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }
    sync_cwd();
    if (!root->chdir(root, kpath)) {
        return -ENOENT;
    }
    save_cwd();
    return 0;
}

long systime(struct datetime *out) {
    struct datetime now;

    if (!out) {
        return -EFAULT;
    }

    rtc(&now);
    return copyout(out, &now, sizeof(now)) ? 0 : -EFAULT;
}


long sysuname(struct utsname *out) {
    struct utsname name;

    if (!out) {
        return -EFAULT;
    }

    memset(&name, 0, sizeof(name));
    strncpy(name.sysname, "Monarch", sizeof(name.sysname));
    strncpy(name.nodename, "monarch", sizeof(name.nodename));
    strncpy(name.release, MONARCH_VERSION, sizeof(name.release));
    strncpy(name.version, MONARCH_CODENAME, sizeof(name.version));
    strncpy(name.machine, MONARCH_ARCH, sizeof(name.machine));

    return copyout(out, &name, sizeof(name)) ? 0 : -EFAULT;
}

long syscwd(char *buffer, size_t size) {
    struct process *p = proc();
    char cwd[VFS_PATH];
    size_t amount;

    if (!p) {
        return -ENODEV;
    }
    if (!buffer || !size) {
        return -EFAULT;
    }

    strncpy(cwd, processcwd(p), sizeof(cwd));
    amount = min(size, strlen(cwd) + 1u);
    if (amount > sizeof(cwd)) {
        amount = sizeof(cwd);
    }
    return copyout(buffer, cwd, amount) ? 0 : -EFAULT;
}

long sysgetpid(void) {
    struct process *p = proc();
    return p ? (long)p->pid : -1;
}

long sysmount(const char *source, const char *target, const char *type) {
    char ksource[VFS_PATH];
    char ktarget[VFS_PATH];
    char ktype[16];

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(ksource, source, sizeof(ksource)) ||
        !copyinstr(ktarget, target, sizeof(ktarget)) ||
        !copyinstr(ktype, type, sizeof(ktype))) {
        return -EFAULT;
    }

    sync_cwd();
    return root->mountfs(root, ksource, ktarget, ktype) ? 0 : -EINVAL;
}

long sysumount(const char *target) {
    char ktarget[VFS_PATH];

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(ktarget, target, sizeof(ktarget))) {
        return -EFAULT;
    }

    sync_cwd();
    return root->unmount(root, ktarget) ? 0 : -EINVAL;
}

long sysmounts(struct mountent *entries, size_t count) {
    struct mountent temp[16];
    size_t max = min(count, countof(temp));
    size_t got;

    if (!root) {
        return -ENODEV;
    }
    if (!entries && count) {
        return -EFAULT;
    }

    got = root->mountentries(root, temp, max);
    if (got && !copyout(entries, temp, got * sizeof(temp[0]))) {
        return -EFAULT;
    }
    return (long)got;
}

long sysyield(void) {
    yield();
    return 0;
}

long syssleep(uint32_t milliseconds) {
    threadsleep(milliseconds);
    return 0;
}

long sysmkdir(const char *path) {
    char kpath[VFS_PATH];

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }
    sync_cwd();
    return root->mkdir(root, kpath) ? 0 : -EEXIST;
}

long sysrmdir(const char *path) {
    char kpath[VFS_PATH];
    struct vstat st;

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }
    sync_cwd();
    if (!root->stat(root, kpath, &st)) {
        return -ENOENT;
    }
    if (st.type != VFS_DIR) {
        return -ENOTDIR;
    }
    return root->rmdir(root, kpath) ? 0 : -ENOTEMPTY;
}

long syscreate(const char *path) {
    char kpath[VFS_PATH];

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }
    sync_cwd();
    return root->create(root, kpath) ? 0 : -EIO;
}

long sysunlink(const char *path) {
    char kpath[VFS_PATH];

    if (!root) {
        return -ENODEV;
    }
    if (!copyinstr(kpath, path, sizeof(kpath))) {
        return -EFAULT;
    }
    sync_cwd();
    if (root->realpath(root, kpath, kpath, sizeof(kpath)) && unix_unlink(kpath))
        return 0;
    if (root->realpath(root, kpath, kpath, sizeof(kpath)) && pipe_unlink(kpath))
        return 0;
    return root->unlink(root, kpath) ? 0 : -ENOENT;
}

void sysexit(int status) {
    struct process *p = proc();
    if (p) {
        p->exit_code = status;
    }
    threadexit();
}

long syscalln(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    struct process *p = proc();
    long result;
    unused(d);
    unused(p);

    if (number == SYS_EXIT) {
        KLOG("syscall", "pid=%u exit(%d)", p ? p->pid : 0, (int)a);
        sysexit((int)a);
    }

    switch (number) {
        case SYS_READ:
            result = sysread((int)a, (void *)b, (size_t)c);
            break;
        case SYS_WRITE:
            result = syswrite((int)a, (const void *)b, (size_t)c);
            break;
        case SYS_OPEN:
            result = sysopen((const char *)a, (uint32_t)b);
            break;
        case SYS_CLOSE:
            result = sysclose((int)a);
            break;
        case SYS_LSEEK:
            result = syslseek((int)a, (long)b, (int)c);
            break;
        case SYS_WAITPID:
            result = syswaitpid((int)a, (int *)b);
            break;
        case SYS_EXECVE:
            result = sysexecve((const char *)a);
            break;
        case SYS_SPAWN:
            result = sysspawn((const char *)a, (const char *)b, (size_t)c);
            break;
        case SYS_UNAME:
            result = sysuname((struct utsname *)a);
            break;
        case SYS_STAT:
            result = sysstat((const char *)a, (struct vstat *)b);
            break;
        case SYS_GETDENTS:
            result = sysgetdents((int)a, (struct sysdirent *)b, (size_t)c);
            break;
        case SYS_CHDIR:
            result = syschdir((const char *)a);
            break;
        case SYS_TIME:
            result = systime((struct datetime *)a);
            break;
        case SYS_CWD:
            result = syscwd((char *)a, (size_t)b);
            break;
        case SYS_GETPID:
            result = sysgetpid();
            break;
        case SYS_MOUNT:
            result = sysmount((const char *)a, (const char *)b, (const char *)c);
            break;
        case SYS_UMOUNT:
            result = sysumount((const char *)a);
            break;
        case SYS_MOUNTS:
            result = sysmounts((struct mountent *)a, (size_t)b);
            break;
        case SYS_YIELD:
            result = sysyield();
            break;
        case SYS_SLEEP:
            result = syssleep((uint32_t)a);
            break;
        case SYS_MKDIR:
            result = sysmkdir((const char *)a);
            break;
        case SYS_RMDIR:
            result = sysrmdir((const char *)a);
            break;
        case SYS_DUP:
            result = sysdup((int)a);
            break;
        case SYS_PIPE:
            result = syspipe((int *)a);
            break;
        case SYS_POLL:
            result = syspoll((struct pollfd *)a, (uint32_t)b, (int32_t)c);
            break;
        case SYS_SELECT:
            result = sysselect((const struct select_args *)a);
            break;
        case SYS_MKFIFO:
            result = sysmkfifo((const char *)a);
            break;
        case SYS_SOCKETPAIR:
            result = syssocketpair((const struct socketpair_args *)a);
            break;
        case SYS_SOCKET:
            result = syssocket((uint32_t)a, (uint32_t)b, (uint32_t)c);
            break;
        case SYS_BIND:
            result = sysbind((int)a, (const struct sockaddr_un *)b);
            break;
        case SYS_LISTEN:
            result = syslisten((int)a, (uint32_t)b);
            break;
        case SYS_ACCEPT:
            result = sysaccept((int)a);
            break;
        case SYS_CONNECT:
            result = sysconnect((int)a, (const struct sockaddr_un *)b);
            break;
        case SYS_PTYPAIR:
            result = sysptypair((int *)a);
            break;
        case SYS_BRK:
            result = sysbrk(a);
            break;
        case SYS_FCNTL:
            result = sysfcntl((int)a, (uint32_t)b, c);
            break;
        case SYS_IOCTL:
            result = sysioctl((int)a, (uint32_t)b, c);
            break;
        case SYS_DUP2:
            result = sysdup2((int)a, (int)b);
            break;
        case SYS_CREATE:
            result = syscreate((const char *)a);
            break;
        case SYS_UNLINK:
            result = sysunlink((const char *)a);
            break;
        default:
            result = -ENOSYS;
            break;
    }

    if (result < 0) {
        /* Non-blocking input uses EAGAIN as its normal "no event yet"
           result. Do not flood the kernel log once per polling tick. */
        int quiet_input = number == SYS_IOCTL &&
            (b == KBD_GETEVENT || b == MOUSE_GETINFO || b == MOUSE_GETEVENT) &&
            -result == EAGAIN;
        int quiet_nonblocking =
            ((number == SYS_ACCEPT || number == SYS_CONNECT) && -result == EAGAIN) ||
            (number == SYS_UNLINK && -result == ENOENT) ||
            ((number == SYS_READ || number == SYS_WRITE) && -result == EAGAIN &&
             p && (int)a >= 0 && (int)a < PROCESS_FDS && p->used(p, (int)a) &&
             p->file(p, (int)a) && p->file(p, (int)a)->_nonblock);
        if (!quiet_input && !quiet_nonblocking) {
            KLOG("syscall", "pid=%u syscall=%u error=%d", p ? p->pid : 0, number, (int)-result);
        }
    }
    return result;
}


long syscallgate(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    return syscalln(number, a, b, c, d);
}

long sysint(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uintptr_t result;
    __asm__ volatile (
        "push %%ebx\n\t"
        "push %%esi\n\t"
        "push %%ecx\n\t"
        "push %%edx\n\t"
        "mov %2, %%ebx\n\t"
        "mov %5, %%esi\n\t"
        "int $0x80\n\t"
        "pop %%edx\n\t"
        "pop %%ecx\n\t"
        "pop %%esi\n\t"
        "pop %%ebx"
        : "=a"(result)
        : "a"(number), "r"(a), "c"(b), "d"(c), "r"(d)
        : "memory", "cc"
    );
    return (long)result;
}
