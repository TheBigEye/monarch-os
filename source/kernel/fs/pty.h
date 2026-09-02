#ifndef MONARCH_KERNEL_FS_PTY_H
#define MONARCH_KERNEL_FS_PTY_H 1

#include "kernel/fs/vfs.h"

/* First PTY milestone: a bidirectional master/slave byte stream. */
int pty_pair(struct vfile **master, struct vfile **slave);
int pty_getwinsize(struct vfile *file, struct winsize *out);
int pty_setwinsize(struct vfile *file, const struct winsize *size);
int pty_gettermios(struct vfile *file, struct termios *out);
int pty_settermios(struct vfile *file, const struct termios *termios);

#endif /* MONARCH_KERNEL_FS_PTY_H */
