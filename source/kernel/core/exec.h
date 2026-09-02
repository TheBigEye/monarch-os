/**
 * @file exec.h
 * @brief Program execution interface built on the ELF loader and scheduler.
 */

#ifndef MONARCH_KERNEL_CORE_EXEC_H
#define MONARCH_KERNEL_CORE_EXEC_H 1

#include "base/api/monarch.h"
#include "kernel/fs/vfs.h"
#include "kernel/scheduler/process.h"

struct execresult {
    uint32_t pid;
    uint32_t tid;
    uintptr_t entry;
    uintptr_t start;
    uintptr_t end;
};

int execspawn(struct vfs *fs, const char *path, struct process *parent, struct execresult *out);
int execspawnenv(struct vfs *fs, const char *path, const char *envblock, size_t envsize, struct process *parent, struct execresult *out);
long execreplace(struct vfs *fs, const char *path);
const char *execerror(void);

#endif /* MONARCH_KERNEL_CORE_EXEC_H */
