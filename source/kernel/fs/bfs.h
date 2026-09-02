#ifndef MONARCH_KERNEL_FS_BFS_H
#define MONARCH_KERNEL_FS_BFS_H 1

#include "base/api/monarch.h"

#define BFS_NAME 32u
#define BFS_PATH 256u
#define BFS_TEXT 4096u

enum bfstype {
    BFS_NONE = 0,
    BFS_FILE = 1,
    BFS_DIR = 2
};

struct bfsstat {
    enum bfstype type;
    size_t size;
};

typedef void (*bfsiter)(void *ctx, const char *name, enum bfstype type, size_t size);

struct bfsfile {
    char name[BFS_NAME];
    char *data;
    size_t size;
    struct bfsfile *next;
};

struct bfsnode {
    char name[BFS_NAME];
    struct bfsnode *parent;
    struct bfsnode *child;
    struct bfsnode *next;
    struct bfsfile *files;
};

struct bfs {
    int (*mkdir)(struct bfs *self, const char *path);
    int (*rmdir)(struct bfs *self, const char *path);
    int (*touch)(struct bfs *self, const char *path);
    int (*write)(struct bfs *self, const char *path, const char *text);
    int (*writebytes)(struct bfs *self, const char *path, const void *data, size_t size);
    const char *(*read)(struct bfs *self, const char *path);
    const uint8_t *(*data)(struct bfs *self, const char *path, size_t *size);
    int (*remove)(struct bfs *self, const char *path);
    int (*cd)(struct bfs *self, const char *path);
    const char *(*pwd)(struct bfs *self, char *buffer, size_t size);
    int (*stat)(struct bfs *self, const char *path, struct bfsstat *out);
    void (*each)(struct bfs *self, const char *path, bfsiter iter, void *ctx);
    struct bfsnode *_root;
    struct bfsnode *_cwd;
};

void bfs(struct bfs *self);

#endif /* MONARCH_KERNEL_FS_BFS_H */
