#include "kernel/fs/stream.h"

void filestream(struct filestream *stream, const char *name, char *data, size_t size) {
    stream->name = name;
    stream->data = data;
    stream->size = size;
}
