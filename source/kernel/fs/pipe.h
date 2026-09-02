/**
 * @file pipe.h
 * @brief Kernel pipe factory returning read/write vfile endpoints.
 */

#ifndef MONARCH_KERNEL_FS_PIPE_H
#define MONARCH_KERNEL_FS_PIPE_H 1

#include "kernel/fs/vfs.h"

int pipeopen(struct vfile **read_end, struct vfile **write_end);
struct vfile *pipe_named_open(const char *path, const char *mode);
int pipe_mkfifo(const char *path);
int pipe_unlink(const char *path);

#endif /* MONARCH_KERNEL_FS_PIPE_H */
