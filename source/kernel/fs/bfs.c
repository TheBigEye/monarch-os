#include "kernel/fs/bfs.h"
#include "kernel/memory/heap.h"

static int valid(const char *name) {
    size_t len;
    if (!name || !*name) {
        return 0;
    }
    len = strlen(name);
    if (len >= BFS_NAME) {
        return 0;
    }
    while (*name) {
        if (strchr("\\/:;*?\"<>|", *name)) {
            return 0;
        }
        name++;
    }
    return 1;
}

static struct bfsnode *node(struct bfsnode *parent, const char *name) {
    struct bfsnode *n = kcalloc(1, sizeof(*n));
    if (!n) {
        return nil;
    }
    strncpy(n->name, name, sizeof(n->name));
    n->parent = parent;
    if (parent) {
        n->next = parent->child;
        parent->child = n;
    }
    return n;
}

static struct bfsfile *file(struct bfsnode *parent, const char *name) {
    struct bfsfile *f = kcalloc(1, sizeof(*f));
    if (!f) {
        return nil;
    }
    f->data = kcalloc(1, 1);
    if (!f->data) {
        kfree(f);
        return nil;
    }
    strncpy(f->name, name, sizeof(f->name));
    f->next = parent->files;
    parent->files = f;
    return f;
}

static struct bfsnode *findchild(struct bfsnode *parent, const char *name) {
    struct bfsnode *n = parent ? parent->child : nil;
    while (n) {
        if (strcmp(n->name, name) == 0) {
            return n;
        }
        n = n->next;
    }
    return nil;
}

static struct bfsfile *findfile(struct bfsnode *parent, const char *name) {
    struct bfsfile *f = parent ? parent->files : nil;
    while (f) {
        if (strcmp(f->name, name) == 0) {
            return f;
        }
        f = f->next;
    }
    return nil;
}

static int part(const char **path, char *out, size_t size) {
    size_t used = 0;
    const char *p = *path;

    while (*p == '/') {
        p++;
    }
    if (!*p) {
        *path = p;
        return 0;
    }
    while (*p && *p != '/') {
        if (used + 1 < size) {
            out[used++] = *p;
        }
        p++;
    }
    out[used] = '\0';
    *path = p;
    return 1;
}

static struct bfsnode *resolve(struct bfs *self, const char *path) {
    struct bfsnode *cur;
    char name[BFS_NAME];

    if (!path || !*path) {
        return self->_cwd;
    }

    cur = path[0] == '/' ? self->_root : self->_cwd;
    while (part(&path, name, sizeof(name))) {
        if (strcmp(name, ".") == 0) {
            continue;
        }
        if (strcmp(name, "..") == 0) {
            if (cur->parent) {
                cur = cur->parent;
            }
            continue;
        }
        cur = findchild(cur, name);
        if (!cur) {
            return nil;
        }
    }
    return cur;
}

static struct bfsnode *parentof(struct bfs *self, const char *path, char *leaf, size_t size) {
    const char *slash = strrchr(path, '/');
    char temp[BFS_TEXT];

    if (!slash) {
        strncpy(leaf, path, size);
        return self->_cwd;
    }

    strncpy(leaf, slash + 1, size);
    if (slash == path) {
        return self->_root;
    }

    if ((size_t)(slash - path) >= sizeof(temp)) {
        return nil;
    }
    memcpy(temp, path, (size_t)(slash - path));
    temp[slash - path] = '\0';
    return resolve(self, temp);
}

static int mkdir_impl(struct bfs *self, const char *path) {
    char name[BFS_NAME];
    struct bfsnode *parent = parentof(self, path, name, sizeof(name));
    if (!parent || !valid(name) || findchild(parent, name)) {
        return 0;
    }
    return node(parent, name) != nil;
}

static int touch_impl(struct bfs *self, const char *path) {
    char name[BFS_NAME];
    struct bfsnode *parent = parentof(self, path, name, sizeof(name));
    if (!parent || !valid(name) || findfile(parent, name)) {
        return 0;
    }
    return file(parent, name) != nil;
}

static int writebytes_impl(struct bfs *self, const char *path, const void *data, size_t size) {
    char name[BFS_NAME];
    struct bfsnode *parent = parentof(self, path, name, sizeof(name));
    struct bfsfile *f = findfile(parent, name);
    char *copy;

    if (!f || (!data && size)) {
        return 0;
    }

    copy = kcalloc(size + 1u, 1u);
    if (!copy) {
        return 0;
    }

    if (size) {
        memcpy(copy, data, size);
    }
    copy[size] = '\0';

    kfree(f->data);
    f->data = copy;
    f->size = size;
    return 1;
}

static int write_impl(struct bfs *self, const char *path, const char *text) {
    return writebytes_impl(self, path, text, text ? strlen(text) : 0);
}

static const char *read_impl(struct bfs *self, const char *path) {
    char name[BFS_NAME];
    struct bfsnode *parent = parentof(self, path, name, sizeof(name));
    struct bfsfile *f = findfile(parent, name);
    return f ? f->data : nil;
}

static const uint8_t *data_impl(struct bfs *self, const char *path, size_t *size) {
    char name[BFS_NAME];
    struct bfsnode *parent = parentof(self, path, name, sizeof(name));
    struct bfsfile *f = findfile(parent, name);
    if (!f) {
        return nil;
    }
    if (size) {
        *size = f->size;
    }
    return (const uint8_t *)f->data;
}

static int rmdir_impl(struct bfs *self, const char *path) {
    char name[BFS_NAME];
    struct bfsnode *parent = parentof(self, path, name, sizeof(name));
    struct bfsnode **current;

    if (!parent || !name[0]) {
        return 0;
    }

    current = &parent->child;
    while (*current) {
        if (strcmp((*current)->name, name) == 0) {
            struct bfsnode *victim = *current;
            if (victim->child || victim->files) {
                return 0;
            }
            *current = victim->next;
            kfree(victim);
            return 1;
        }
        current = &(*current)->next;
    }
    return 0;
}

static int remove_impl(struct bfs *self, const char *path) {
    char name[BFS_NAME];
    struct bfsnode *parent = parentof(self, path, name, sizeof(name));
    struct bfsfile **current;

    if (!parent) {
        return 0;
    }

    current = &parent->files;
    while (*current) {
        if (strcmp((*current)->name, name) == 0) {
            struct bfsfile *victim = *current;
            *current = victim->next;
            kfree(victim->data);
            kfree(victim);
            return 1;
        }
        current = &(*current)->next;
    }
    return 0;
}

static int cd_impl(struct bfs *self, const char *path) {
    struct bfsnode *n = resolve(self, path);
    if (!n) {
        return 0;
    }
    self->_cwd = n;
    return 1;
}

static void buildpwd(struct bfsnode *nodeptr, char *buffer, size_t size) {
    char temp[BFS_TEXT];
    if (!nodeptr || !nodeptr->parent) {
        strncpy(buffer, "/", size);
        return;
    }
    buildpwd(nodeptr->parent, temp, sizeof(temp));
    if (strcmp(temp, "/") != 0) {
        strncat(temp, "/", sizeof(temp) - strlen(temp) - 1);
    }
    strncat(temp, nodeptr->name, sizeof(temp) - strlen(temp) - 1);
    strncpy(buffer, temp, size);
}

static const char *pwd_impl(struct bfs *self, char *buffer, size_t size) {
    buildpwd(self->_cwd, buffer, size);
    return buffer;
}

static int stat_impl(struct bfs *self, const char *path, struct bfsstat *out) {
    char name[BFS_NAME];
    struct bfsnode *parent;
    struct bfsfile *f;
    struct bfsnode *d;

    if (!out) {
        return 0;
    }

    memset(out, 0, sizeof(*out));

    d = resolve(self, path);
    if (d) {
        out->type = BFS_DIR;
        out->size = 0;
        return 1;
    }

    parent = parentof(self, path, name, sizeof(name));
    f = findfile(parent, name);
    if (f) {
        out->type = BFS_FILE;
        out->size = f->size;
        return 1;
    }

    return 0;
}

static void each_impl(struct bfs *self, const char *path, bfsiter iter, void *ctx) {
    struct bfsnode *dir = resolve(self, path);
    struct bfsnode *d;
    struct bfsfile *f;

    if (!dir || !iter) {
        return;
    }

    for (d = dir->child; d; d = d->next) {
        iter(ctx, d->name, BFS_DIR, 0);
    }

    for (f = dir->files; f; f = f->next) {
        iter(ctx, f->name, BFS_FILE, f->size);
    }
}

void bfs(struct bfs *self) {
    memset(self, 0, sizeof(*self));
    self->mkdir = mkdir_impl;
    self->rmdir = rmdir_impl;
    self->touch = touch_impl;
    self->write = write_impl;
    self->writebytes = writebytes_impl;
    self->read = read_impl;
    self->data = data_impl;
    self->remove = remove_impl;
    self->cd = cd_impl;
    self->pwd = pwd_impl;
    self->stat = stat_impl;
    self->each = each_impl;
    self->_root = node(nil, "/");
    self->_cwd = self->_root;
}
