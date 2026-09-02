/**
 * @file sys.c
 * @brief Userspace syscall wrappers, errno, environment and console output.
 */

#include "base/usr/sys.h"

int errno;
char **environ;

static long sysret(long value) {
    if (value < 0) {
        errno = (int)-value;
        return -1;
    }

    /*
     * Match normal Unix libc behaviour: successful calls do not clear errno.
     * errno is only meaningful immediately after a function reports failure.
     */
    return value;
}

const char *strerror(int code) {
    switch (code) {
        case 0: return "ok";
        case EPERM: return "operation not permitted";
        case ENOENT: return "no such file or directory";
        case ESRCH: return "no such process";
        case EINTR: return "interrupted";
        case EIO: return "i/o error";
        case ENOEXEC: return "exec format error";
        case EBADF: return "bad file descriptor";
        case ECHILD: return "no child process";
        case EAGAIN: return "try again";
        case ENOMEM: return "out of memory";
        case EACCES: return "permission denied";
        case EFAULT: return "bad address";
        case EBUSY: return "busy";
        case EEXIST: return "already exists";
        case ENODEV: return "no such device";
        case ENOTDIR: return "not a directory";
        case EISDIR: return "is a directory";
        case EINVAL: return "invalid argument";
        case EMFILE: return "too many open files";
        case ENOTTY: return "not a tty";
        case ESPIPE: return "illegal seek";
        case ENOSYS: return "function not implemented";
        case ENOTEMPTY: return "directory not empty";
        default: return "unknown error";
    }
}

void perror(const char *prefix) {
    if (prefix && *prefix) {
        eputs(prefix);
        eputs(": ");
    }
    eputs(strerror(errno));
    eputch('\n');
}

const char *getenv(const char *name) {
    size_t len;

    if (!name || !environ) {
        return nil;
    }

    len = strlen(name);
    for (char **env = environ; *env; env++) {
        if (strncmp(*env, name, len) == 0 && (*env)[len] == '=') {
            return *env + len + 1u;
        }
    }
    return nil;
}


long syscall4(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
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

void exit(int status) {
    syscall4(SYS_EXIT, (uintptr_t)status, 0, 0, 0);
    for (;;) {
        syscall4(SYS_YIELD, 0, 0, 0, 0);
    }
}

long read(int fd, void *buffer, size_t size) {
    return sysret(syscall4(SYS_READ, (uintptr_t)fd, (uintptr_t)buffer, (uintptr_t)size, 0));
}

long write(int fd, const void *buffer, size_t size) {
    return sysret(syscall4(SYS_WRITE, (uintptr_t)fd, (uintptr_t)buffer, (uintptr_t)size, 0));
}

long open(const char *path, uint32_t flags) {
    return sysret(syscall4(SYS_OPEN, (uintptr_t)path, (uintptr_t)flags, 0, 0));
}

long close(int fd) {
    return sysret(syscall4(SYS_CLOSE, (uintptr_t)fd, 0, 0, 0));
}

long lseek(int fd, long offset, int whence) {
    return sysret(syscall4(SYS_LSEEK, (uintptr_t)fd, (uintptr_t)offset, (uintptr_t)whence, 0));
}

long dup(int fd) {
    return sysret(syscall4(SYS_DUP, (uintptr_t)fd, 0, 0, 0));
}

long pipe(int fds[2]) {
    return sysret(syscall4(SYS_PIPE, (uintptr_t)fds, 0, 0, 0));
}

long dup2(int oldfd, int newfd) {
    return sysret(syscall4(SYS_DUP2, (uintptr_t)oldfd, (uintptr_t)newfd, 0, 0));
}

long fcntl(int fd, uint32_t command, uintptr_t arg) {
    return sysret(syscall4(SYS_FCNTL, (uintptr_t)fd, (uintptr_t)command, arg, 0));
}


long ioctl(int fd, uint32_t request, void *arg) {
    return sysret(syscall4(SYS_IOCTL, (uintptr_t)fd, (uintptr_t)request, (uintptr_t)arg, 0));
}

int isatty(int fd) {
    int saved = errno;
    if (ioctl(fd, TCGETS, nil) == 0) {
        return 1;
    }
    if (errno == ENOTTY) {
        errno = saved;
    }
    return 0;
}

long waitpid(int pid, int *status) {
    return sysret(syscall4(SYS_WAITPID, (uintptr_t)pid, (uintptr_t)status, 0, 0));
}

long execve(const char *path) {
    return sysret(syscall4(SYS_EXECVE, (uintptr_t)path, 0, 0, 0));
}

long spawn(const char *command) {
    return sysret(syscall4(SYS_SPAWN, (uintptr_t)command, 0, 0, 0));
}

long spawnenv(const char *command, const char *envblock, size_t envsize) {
    return sysret(syscall4(SYS_SPAWN, (uintptr_t)command, (uintptr_t)envblock, (uintptr_t)envsize, 0));
}

long poll(struct pollfd *fds, uint32_t count, int32_t timeout_ms) {
    return sysret(syscall4(SYS_POLL, (uintptr_t)fds, (uintptr_t)count,
                           (uintptr_t)timeout_ms, 0));
}

long select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            struct timeval *timeout) {
    struct select_args args;
    args.nfds = nfds;
    args.readfds = (uintptr_t)readfds;
    args.writefds = (uintptr_t)writefds;
    args.exceptfds = (uintptr_t)exceptfds;
    args.timeout = (uintptr_t)timeout;
    return sysret(syscall4(SYS_SELECT, (uintptr_t)&args, 0, 0, 0));
}

long mkfifo(const char *path) {
    return sysret(syscall4(SYS_MKFIFO, (uintptr_t)path, 0, 0, 0));
}

long socketpair(int domain, int type, int protocol, int fds[2]) {
    struct socketpair_args args;
    args.domain = (uint32_t)domain;
    args.type = (uint32_t)type;
    args.protocol = (uint32_t)protocol;
    args.fds = (uintptr_t)fds;
    return sysret(syscall4(SYS_SOCKETPAIR, (uintptr_t)&args, 0, 0, 0));
}

long socket(int domain, int type, int protocol) {
    return sysret(syscall4(SYS_SOCKET, (uintptr_t)domain, (uintptr_t)type,
                           (uintptr_t)protocol, 0));
}

long bind(int fd, const struct sockaddr_un *address) {
    return sysret(syscall4(SYS_BIND, (uintptr_t)fd, (uintptr_t)address, 0, 0));
}

long listen(int fd, uint32_t backlog) {
    return sysret(syscall4(SYS_LISTEN, (uintptr_t)fd, (uintptr_t)backlog, 0, 0));
}

long accept(int fd) {
    return sysret(syscall4(SYS_ACCEPT, (uintptr_t)fd, 0, 0, 0));
}

long connect(int fd, const struct sockaddr_un *address) {
    return sysret(syscall4(SYS_CONNECT, (uintptr_t)fd, (uintptr_t)address, 0, 0));
}

long pty_pair(int fds[2]) {
    return sysret(syscall4(SYS_PTYPAIR, (uintptr_t)fds, 0, 0, 0));
}

long brk(uintptr_t address) {
    return sysret(syscall4(SYS_BRK, address, 0, 0, 0));
}

void *sbrk(long increment) {
    static uintptr_t current;
    uintptr_t next;
    long result;
    if (!current) {
        result = brk(0);
        if (result < 0) return (void *)-1;
        current = (uintptr_t)result;
    }
    if (!increment) return (void *)current;
    next = increment > 0 ? current + (uintptr_t)increment : current - (uintptr_t)(-increment);
    result = brk(next);
    if (result < 0) return (void *)-1;
    {
        void *old = (void *)current;
        current = (uintptr_t)result;
        return old;
    }
}

long chdir(const char *path) {
    return sysret(syscall4(SYS_CHDIR, (uintptr_t)path, 0, 0, 0));
}

long gettime(struct datetime *out) {
    return sysret(syscall4(SYS_TIME, (uintptr_t)out, 0, 0, 0));
}

long uname(struct utsname *out) {
    return sysret(syscall4(SYS_UNAME, (uintptr_t)out, 0, 0, 0));
}

long getcwd(char *buffer, size_t size) {
    return sysret(syscall4(SYS_CWD, (uintptr_t)buffer, (uintptr_t)size, 0, 0));
}

long getpid(void) {
    return sysret(syscall4(SYS_GETPID, 0, 0, 0, 0));
}

long mount(const char *source, const char *target, const char *type) {
    return sysret(syscall4(SYS_MOUNT, (uintptr_t)source, (uintptr_t)target, (uintptr_t)type, 0));
}

long umount(const char *target) {
    return sysret(syscall4(SYS_UMOUNT, (uintptr_t)target, 0, 0, 0));
}

long mounts(struct mountent *entries, size_t count) {
    return sysret(syscall4(SYS_MOUNTS, (uintptr_t)entries, count, 0, 0));
}

long sleepms(uint32_t milliseconds) {
    return sysret(syscall4(SYS_SLEEP, (uintptr_t)milliseconds, 0, 0, 0));
}

long stat(const char *path, struct stat *out) {
    return sysret(syscall4(SYS_STAT, (uintptr_t)path, (uintptr_t)out, 0, 0));
}

long getdents(int fd, struct dirent *entries, size_t count) {
    return sysret(syscall4(SYS_GETDENTS, (uintptr_t)fd, (uintptr_t)entries, (uintptr_t)count, 0));
}

long mkdir(const char *path) {
    return sysret(syscall4(SYS_MKDIR, (uintptr_t)path, 0, 0, 0));
}

long rmdir(const char *path) {
    return sysret(syscall4(SYS_RMDIR, (uintptr_t)path, 0, 0, 0));
}

long create(const char *path) {
    return sysret(syscall4(SYS_CREATE, (uintptr_t)path, 0, 0, 0));
}

long unlink(const char *path) {
    return sysret(syscall4(SYS_UNLINK, (uintptr_t)path, 0, 0, 0));
}

void puts(const char *text) {
    write(1, text, strlen(text));
}

void putch(char ch) {
    write(1, &ch, 1);
}

void eputs(const char *text) {
    write(2, text, strlen(text));
}

void eputch(char ch) {
    write(2, &ch, 1);
}

void putu(uint32_t value) {
    char text[11];
    unsigned pos = sizeof(text);

    text[--pos] = '\0';
    do {
        text[--pos] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value);

    puts(&text[pos]);
}

void puti(int32_t value) {
    if (value < 0) {
        putch('-');
        putu((uint32_t)-value);
    } else {
        putu((uint32_t)value);
    }
}
