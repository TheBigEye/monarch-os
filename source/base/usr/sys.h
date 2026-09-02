/**
 * @file sys.h
 * @brief Userspace syscall wrapper API and small libc-like helpers.
 */

#ifndef MONARCH_BASE_USR_SYS_H
#define MONARCH_BASE_USR_SYS_H 1

#include "base/api/monarch.h"

#ifndef MONARCH_USER_BUILD
#error "base/usr is userspace-only; kernel code must use arch/x86/syscall.h or base/api instead."
#endif

/*
 * Syscall numbers, open flags, fcntl commands and descriptor flags are part
 * of the public ABI and live in base/api/abi.h via monarch.h.
 */

#define VFS_FILE 1u
#define VFS_DIR  2u
#define DIRENT_NAME 64u

/** One directory entry returned by getdents(). */
struct dirent {
    char name[DIRENT_NAME];
    uint32_t type;
    uint32_t size;
};

/** Minimal file metadata returned by stat(). */
struct stat {
    uint32_t type;
    size_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
};

extern int errno;
extern char **environ;
const char *getenv(const char *name);
const char *strerror(int code);
void perror(const char *prefix);

/** Raw four-argument int 0x80 wrapper. Prefer typed wrappers below. */
long syscall4(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d);

/** Terminate the current process with `status`. */
void exit(int status) __attribute__((noreturn));
long read(int fd, void *buffer, size_t size);
long write(int fd, const void *buffer, size_t size);
long open(const char *path, uint32_t flags);
long close(int fd);
long lseek(int fd, long offset, int whence);
long dup(int fd);
long pipe(int fds[2]);
long dup2(int oldfd, int newfd);
long fcntl(int fd, uint32_t command, uintptr_t arg);
long ioctl(int fd, uint32_t request, void *arg);
int isatty(int fd);
long waitpid(int pid, int *status);
long execve(const char *path);
long spawn(const char *command);
long spawnenv(const char *command, const char *envblock, size_t envsize);
long poll(struct pollfd *fds, uint32_t count, int32_t timeout_ms);
long select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            struct timeval *timeout);
long mkfifo(const char *path);
long socketpair(int domain, int type, int protocol, int fds[2]);
long socket(int domain, int type, int protocol);
long bind(int fd, const struct sockaddr_un *address);
long listen(int fd, uint32_t backlog);
long accept(int fd);
long connect(int fd, const struct sockaddr_un *address);
long pty_pair(int fds[2]);
long brk(uintptr_t address);
void *sbrk(long increment);
long chdir(const char *path);
long gettime(struct datetime *out);
long uname(struct utsname *out);
long getcwd(char *buffer, size_t size);
long getpid(void);
long mount(const char *source, const char *target, const char *type);
long umount(const char *target);
long mounts(struct mountent *entries, size_t count);
long sleepms(uint32_t milliseconds);
long stat(const char *path, struct stat *out);
long getdents(int fd, struct dirent *entries, size_t count);
long mkdir(const char *path);
long rmdir(const char *path);
long create(const char *path);
long unlink(const char *path);

void puts(const char *text);
void putch(char ch);
void eputs(const char *text);
void eputch(char ch);
void putu(uint32_t value);
void puti(int32_t value);

#endif /* MONARCH_BASE_USR_SYS_H */
