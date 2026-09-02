#ifndef MONARCH_KERNEL_FS_STREAM_H
#define MONARCH_KERNEL_FS_STREAM_H 1

#include "base/api/monarch.h"

struct filestream {
    const char *name;
    char *data;
    size_t size;
};

void filestream(struct filestream *stream, const char *name, char *data, size_t size);

#endif /* MONARCH_KERNEL_FS_STREAM_H */
