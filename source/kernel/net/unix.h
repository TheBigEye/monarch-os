#ifndef MONARCH_KERNEL_NET_UNIX_H
#define MONARCH_KERNEL_NET_UNIX_H 1

#include "kernel/fs/vfs.h"

int unix_socketpair(struct vfile **left, struct vfile **right);
int unix_socket(struct vfile **out);
int unix_bind(struct vfile *file, const struct sockaddr_un *address);
int unix_listen(struct vfile *file, uint32_t backlog);
int unix_connect(struct vfile *file, const struct sockaddr_un *address,
                 struct vfile **connected);
struct vfile *unix_accept(struct vfile *file);
int unix_unlink(const char *path);

#endif /* MONARCH_KERNEL_NET_UNIX_H */
