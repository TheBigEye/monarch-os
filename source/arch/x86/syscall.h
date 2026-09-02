/**
 * @file syscall.h
 * @brief Monarch int 0x80 kernel-side dispatcher and helper prototypes.
 */

#ifndef MONARCH_ARCH_X86_SYSCALL_H
#define MONARCH_ARCH_X86_SYSCALL_H 1

#include "base/api/monarch.h"
#include "kernel/fs/vfs.h"

/*
 * Public syscall numbers, open flags and descriptor flags live in
 * base/api/abi.h, included through monarch.h.  This header only declares the
 * kernel-side syscall dispatcher and helper entry points.
 */

struct sysdirent {
    char name[VFS_NAME];
    uint32_t type;
    uint32_t size;
};

void syscall(struct vfs *fs);
long syscalln(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d);
long syscallgate(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d);
long sysint(uint32_t number, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d);

long sysopen(const char *path, uint32_t flags);
long sysread(int fd, void *buffer, size_t size);
long syswrite(int fd, const void *buffer, size_t size);
long sysclose(int fd);
long syslseek(int fd, long offset, int whence);
long sysdup(int fd);
long syspipe(int fds[2]);
long sysdup2(int oldfd, int newfd);
long sysfcntl(int fd, uint32_t command, uintptr_t arg);
long sysioctl(int fd, uint32_t request, uintptr_t arg);
long syswaitpid(int pid, int *status);
long sysexecve(const char *path);
long sysspawn(const char *command, const char *envblock, size_t envsize);
long sysstat(const char *path, struct vstat *out);
long sysgetdents(int fd, struct sysdirent *entries, size_t count);
long syschdir(const char *path);
long systime(struct datetime *out);
long sysuname(struct utsname *out);
long syscwd(char *buffer, size_t size);
long sysgetpid(void);
long sysmount(const char *source, const char *target, const char *type);
long sysumount(const char *target);
long sysmounts(struct mountent *entries, size_t count);
long sysyield(void);
long syssleep(uint32_t milliseconds);
long sysmkdir(const char *path);
long sysrmdir(const char *path);
long syscreate(const char *path);
long sysunlink(const char *path);
void sysexit(int status) __attribute__((noreturn));

#endif /* MONARCH_ARCH_X86_SYSCALL_H */
