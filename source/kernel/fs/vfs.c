#include "kernel/fs/vfs.h"
#include "base/lib/path.h"
#include "kernel/memory/heap.h"
#include "kernel/fs/pipe.h"

struct resolved {
    struct vmount *mount;
    char full[VFS_PATH];
    char sub[VFS_PATH];
};

static enum vnodetype btype(enum bfstype value) {
    return value == BFS_DIR ? VFS_DIR : value == BFS_FILE ? VFS_FILE : VFS_UNKNOWN;
}

static enum vnodetype dtype(enum devfstype value) {
    return value == DEVFS_DIR ? VFS_DIR : value == DEVFS_FILE ? VFS_FILE : VFS_UNKNOWN;
}

static enum vnodetype ftype(enum fat32_node_type value) {
    return value == FAT32_DIR ? VFS_DIR : value == FAT32_FILE ? VFS_FILE : VFS_UNKNOWN;
}

static enum vnodetype etype(enum ext2_node_type value) {
    return value == EXT2_DIR ? VFS_DIR : value == EXT2_FILE ? VFS_FILE : VFS_UNKNOWN;
}

static enum vnodetype ftype12(enum fat12_node_type value) {
    return value == FAT12_DIR ? VFS_DIR : value == FAT12_FILE ? VFS_FILE : VFS_UNKNOWN;
}

static int normalize(struct vfs *self, const char *path, char *out, size_t size) {
    return path_normalize(self ? self->_cwd : "/", path, out, size);
}

static int match(const char *mount, const char *path) {
    size_t len = strlen(mount);

    if (strcmp(mount, "/") == 0) {
        return 1;
    }

    return strncmp(path, mount, len) == 0 && (path[len] == '\0' || path[len] == '/');
}

static int resolve(struct vfs *self, const char *path, struct resolved *out) {
    size_t best = 0;

    if (!self || !out) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    if (!normalize(self, path, out->full, sizeof(out->full))) {
        return 0;
    }

    for (unsigned i = 0; i < self->_mount_count; i++) {
        size_t len = strlen(self->_mounts[i].path);
        if (match(self->_mounts[i].path, out->full) && len >= best) {
            best = len;
            out->mount = &self->_mounts[i];
        }
    }

    if (!out->mount) {
        return 0;
    }

    if (strcmp(out->mount->path, "/") == 0) {
        strncpy(out->sub, out->full, sizeof(out->sub));
    } else if (strcmp(out->full, out->mount->path) == 0) {
        strncpy(out->sub, "/", sizeof(out->sub));
    } else {
        strncpy(out->sub, out->full + strlen(out->mount->path), sizeof(out->sub));
    }

    return 1;
}

static int isbfs(struct resolved *r) {
    return r->mount && strcmp(r->mount->kind, "bfs") == 0;
}

static int isdevfs(struct resolved *r) {
    return r->mount && strcmp(r->mount->kind, "devfs") == 0;
}

static int isfat32(struct resolved *r) {
    return r->mount && strcmp(r->mount->kind, "fat32") == 0;
}

static int isext2(struct resolved *r) {
    return r->mount && strcmp(r->mount->kind, "ext2") == 0;
}

static int isfat12(struct resolved *r) {
    return r->mount && strcmp(r->mount->kind, "fat12") == 0;
}

static int mount_impl(struct vfs *self, const char *path, const char *kind, void *filesystem) {
    struct vmount *mount;

    if (!self || !path || !kind || !filesystem || self->_mount_count >= VFS_MOUNTS) {
        return 0;
    }

    for (unsigned i = 0; i < self->_mount_count; i++) {
        if (strcmp(self->_mounts[i].path, path) == 0) {
            return 0;
        }
    }

    mount = &self->_mounts[self->_mount_count++];
    strncpy(mount->path, path, sizeof(mount->path));
    mount->kind = kind;
    mount->filesystem = filesystem;
    return 1;
}

static void release_mount_slot(struct vfs *self, struct vmount *mount) {
    if (!self || !mount || !mount->filesystem || !mount->kind) {
        return;
    }

    if (strcmp(mount->kind, "fat32") == 0) {
        for (unsigned i = 0; i < VFS_FAT32_MOUNTS; i++) {
            if (mount->filesystem == &self->_fat32_mounts[i]) {
                self->_fat32_used[i] = 0;
                memset(&self->_fat32_mounts[i], 0, sizeof(self->_fat32_mounts[i]));
                return;
            }
        }
    }

    if (strcmp(mount->kind, "fat12") == 0) {
        for (unsigned i = 0; i < VFS_FAT12_MOUNTS; i++) {
            if (mount->filesystem == &self->_fat12_mounts[i]) {
                self->_fat12_used[i] = 0;
                memset(&self->_fat12_mounts[i], 0, sizeof(self->_fat12_mounts[i]));
                return;
            }
        }
    }

    if (strcmp(mount->kind, "ext2") == 0) {
        for (unsigned i = 0; i < VFS_EXT2_MOUNTS; i++) {
            if (mount->filesystem == &self->_ext2_mounts[i]) {
                self->_ext2_used[i] = 0;
                memset(&self->_ext2_mounts[i], 0, sizeof(self->_ext2_mounts[i]));
                return;
            }
        }
    }
}

static int unmount_impl(struct vfs *self, const char *target) {
    char full[VFS_PATH];

    if (!self || !target || !normalize(self, target, full, sizeof(full)) || strcmp(full, "/") == 0) {
        return 0;
    }

    for (unsigned i = 0; i < self->_mount_count; i++) {
        if (strcmp(self->_mounts[i].path, full) == 0) {
            release_mount_slot(self, &self->_mounts[i]);
            for (unsigned j = i + 1; j < self->_mount_count; j++) {
                self->_mounts[j - 1] = self->_mounts[j];
            }
            self->_mount_count--;
            memset(&self->_mounts[self->_mount_count], 0, sizeof(self->_mounts[self->_mount_count]));
            return 1;
        }
    }

    return 0;
}

static size_t mountentries_impl(struct vfs *self, struct mountent *entries, size_t count) {
    size_t used = 0;

    if (!self || (!entries && count)) {
        return 0;
    }

    while (used < count && used < self->_mount_count) {
        strncpy(entries[used].path, self->_mounts[used].path, sizeof(entries[used].path));
        strncpy(entries[used].type, self->_mounts[used].kind ? self->_mounts[used].kind : "", sizeof(entries[used].type));
        used++;
    }
    return used;
}

static struct blockdevice *block_impl(struct vfs *self, const char *path) {
    struct resolved r;

    if (!resolve(self, path, &r)) {
        return nil;
    }

    if (isdevfs(&r)) {
        return ((struct devfs *)r.mount->filesystem)->block(r.mount->filesystem, r.sub);
    }
    return nil;
}

static int mountfs_impl(struct vfs *self, const char *source, const char *target, const char *type) {
    struct blockdevice *device;
    char full[VFS_PATH];

    if (!self || !source || !target || !type || !normalize(self, target, full, sizeof(full))) {
        return 0;
    }

    device = self->block(self, source);
    if (!device) {
        return 0;
    }

    if (strcmp(type, "fat32") == 0) {
        for (unsigned i = 0; i < VFS_FAT32_MOUNTS; i++) {
            if (!self->_fat32_used[i]) {
                if (!fat32(&self->_fat32_mounts[i], device)) {
                    return 0;
                }
                if (!self->mount(self, full, "fat32", &self->_fat32_mounts[i])) {
                    return 0;
                }
                self->_fat32_used[i] = 1;
                return 1;
            }
        }
        return 0;
    }

    if (strcmp(type, "fat12") == 0) {
        for (unsigned i = 0; i < VFS_FAT12_MOUNTS; i++) {
            if (!self->_fat12_used[i]) {
                if (!fat12(&self->_fat12_mounts[i], device)) return 0;
                if (!self->mount(self, full, "fat12", &self->_fat12_mounts[i])) return 0;
                self->_fat12_used[i] = 1;
                return 1;
            }
        }
        return 0;
    }

    if (strcmp(type, "ext2") == 0) {
        for (unsigned i = 0; i < VFS_EXT2_MOUNTS; i++) {
            if (!self->_ext2_used[i]) {
                if (!ext2(&self->_ext2_mounts[i], device)) {
                    return 0;
                }
                if (!self->mount(self, full, "ext2", &self->_ext2_mounts[i])) {
                    return 0;
                }
                self->_ext2_used[i] = 1;
                return 1;
            }
        }
        return 0;
    }

    return 0;
}

static void mounts_impl(struct vfs *self, void (*emit)(void *ctx, const char *text), void *ctx) {
    char line[128];

    if (!self || !emit) {
        return;
    }

    for (unsigned i = 0; i < self->_mount_count; i++) {
        snprintf(line, sizeof(line), "%s on %s\n", self->_mounts[i].kind, self->_mounts[i].path);
        emit(ctx, line);
    }
}

static int stat_impl(struct vfs *self, const char *path, struct vstat *out) {
    struct resolved r;

    if (!out || !resolve(self, path, &r)) {
        return 0;
    }

    memset(out, 0, sizeof(*out));

    if (isbfs(&r)) {
        struct bfsstat st;
        if (!((struct bfs *)r.mount->filesystem)->stat(r.mount->filesystem, r.sub, &st)) {
            return 0;
        }
        out->type = btype(st.type);
        out->size = st.size;
        return 1;
    }

    if (isdevfs(&r)) {
        struct devfsstat st;
        if (!((struct devfs *)r.mount->filesystem)->stat(r.mount->filesystem, r.sub, &st)) {
            return 0;
        }
        out->type = dtype(st.type);
        out->size = st.size;
        return 1;
    }

    if (isfat32(&r)) {
        struct fat32_stat st;
        if (!((struct fat32 *)r.mount->filesystem)->stat(r.mount->filesystem, r.sub, &st)) {
            return 0;
        }
        out->type = ftype(st.type);
        out->size = st.size;
        return 1;
    }

    if (isfat12(&r)) {
        struct fat12_stat st;
        if (!((struct fat12 *)r.mount->filesystem)->stat(r.mount->filesystem, r.sub, &st)) return 0;
        out->type = ftype12(st.type);
        out->size = st.size;
        out->atime = st.atime;
        out->ctime = st.ctime;
        out->mtime = st.mtime;
        return 1;
    }

    if (isext2(&r)) {
        struct ext2_stat st;
        if (!((struct ext2 *)r.mount->filesystem)->stat(r.mount->filesystem, r.sub, &st)) {
            return 0;
        }
        out->type = etype(st.type);
        out->size = st.size;
        out->atime = st.atime;
        out->ctime = st.ctime;
        out->mtime = st.mtime;
        return 1;
    }

    return 0;
}

static int mkdir_impl(struct vfs *self, const char *path) {
    struct resolved r;
    if (!resolve(self, path, &r)) {
        return 0;
    }
    if (isbfs(&r)) {
        return ((struct bfs *)r.mount->filesystem)->mkdir(r.mount->filesystem, r.sub);
    }
    if (isfat32(&r)) {
        return ((struct fat32 *)r.mount->filesystem)->mkdir(r.mount->filesystem, r.sub);
    }
    if (isfat12(&r)) {
        return ((struct fat12 *)r.mount->filesystem)->mkdir(r.mount->filesystem, r.sub);
    }
    if (isext2(&r)) {
        return ((struct ext2 *)r.mount->filesystem)->mkdir(r.mount->filesystem, r.sub);
    }
    return 0;
}

static int rmdir_impl(struct vfs *self, const char *path) {
    struct resolved r;
    if (!resolve(self, path, &r)) {
        return 0;
    }
    if (isbfs(&r)) {
        return ((struct bfs *)r.mount->filesystem)->rmdir(r.mount->filesystem, r.sub);
    }
    if (isfat32(&r)) {
        return ((struct fat32 *)r.mount->filesystem)->rmdir(r.mount->filesystem, r.sub);
    }
    if (isfat12(&r)) {
        return ((struct fat12 *)r.mount->filesystem)->rmdir(r.mount->filesystem, r.sub);
    }
    if (isext2(&r)) {
        return ((struct ext2 *)r.mount->filesystem)->rmdir(r.mount->filesystem, r.sub);
    }
    return 0;
}

static int create_impl(struct vfs *self, const char *path) {
    struct resolved r;
    struct vstat st;

    if (!resolve(self, path, &r)) {
        return 0;
    }

    if (self->stat(self, path, &st)) {
        return st.type == VFS_FILE;
    }

    if (isbfs(&r)) {
        return ((struct bfs *)r.mount->filesystem)->touch(r.mount->filesystem, r.sub);
    }
    if (isfat32(&r)) {
        return ((struct fat32 *)r.mount->filesystem)->create(r.mount->filesystem, r.sub);
    }
    if (isfat12(&r)) {
        return ((struct fat12 *)r.mount->filesystem)->create(r.mount->filesystem, r.sub);
    }
    if (isext2(&r)) {
        return ((struct ext2 *)r.mount->filesystem)->create(r.mount->filesystem, r.sub);
    }

    return 0;
}

static int write_impl(struct vfs *self, const char *path, const char *text) {
    struct resolved r;

    if (!text || !resolve(self, path, &r)) {
        return 0;
    }

    if (isbfs(&r)) {
        struct bfs *fs = r.mount->filesystem;
        if (!fs->read(fs, r.sub) && !fs->touch(fs, r.sub)) {
            return 0;
        }
        return fs->write(fs, r.sub, text);
    }

    if (isdevfs(&r)) {
        return ((struct devfs *)r.mount->filesystem)->write(r.mount->filesystem, r.sub, 0, text, strlen(text)) >= 0;
    }

    return 0;
}

static const char *read_impl(struct vfs *self, const char *path) {
    struct resolved r;

    if (!resolve(self, path, &r)) {
        return nil;
    }

    if (isbfs(&r)) {
        return ((struct bfs *)r.mount->filesystem)->read(r.mount->filesystem, r.sub);
    }

    if (isdevfs(&r)) {
        return ((struct devfs *)r.mount->filesystem)->describe(r.mount->filesystem, r.sub);
    }

    return nil;
}

static int ioctl_impl(struct vfs *self, const char *path, uint32_t request, uintptr_t arg) {
    struct resolved r;

    if (!resolve(self, path, &r)) {
        return -ENOENT;
    }

    if (isdevfs(&r)) {
        return ((struct devfs *)r.mount->filesystem)->ioctl(r.mount->filesystem, r.sub, request, arg);
    }

    return -ENOTTY;
}

static int unlink_impl(struct vfs *self, const char *path) {
    struct resolved r;
    if (!resolve(self, path, &r)) {
        return 0;
    }
    if (isbfs(&r)) {
        return ((struct bfs *)r.mount->filesystem)->remove(r.mount->filesystem, r.sub);
    }
    if (isfat32(&r)) {
        return ((struct fat32 *)r.mount->filesystem)->unlink(r.mount->filesystem, r.sub);
    }
    if (isfat12(&r)) {
        return ((struct fat12 *)r.mount->filesystem)->unlink(r.mount->filesystem, r.sub);
    }
    if (isext2(&r)) {
        return ((struct ext2 *)r.mount->filesystem)->unlink(r.mount->filesystem, r.sub);
    }
    return 0;
}

static int chdir_impl(struct vfs *self, const char *path) {
    struct vstat st;
    char full[VFS_PATH];

    if (!normalize(self, path, full, sizeof(full)) || !self->stat(self, full, &st) || st.type != VFS_DIR) {
        return 0;
    }

    strncpy(self->_cwd, full, sizeof(self->_cwd));
    return 1;
}

static const char *cwd_impl(struct vfs *self, char *buffer, size_t size) {
    if (!self || !buffer || !size) {
        return nil;
    }
    strncpy(buffer, self->_cwd, size);
    return buffer;
}

static const char *realpath_impl(struct vfs *self, const char *path, char *buffer, size_t size) {
    if (!self || !buffer || !size) {
        return nil;
    }
    return normalize(self, path, buffer, size) ? buffer : nil;
}

static int file_read(struct vfile *self, char *buffer, size_t size) {
    struct resolved r;
    const uint8_t *bytes;
    size_t length = 0;
    size_t count;

    if (!self || !buffer || !size) {
        return 0;
    }

    if (!resolve(self->_fs, self->_path, &r)) {
        return -1;
    }

    if (isdevfs(&r)) {
        int count = ((struct devfs *)r.mount->filesystem)->read(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) {
            self->_offset += (size_t)count;
        }
        return count;
    }

    if (isfat32(&r)) {
        int count = ((struct fat32 *)r.mount->filesystem)->read(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) {
            self->_offset += (size_t)count;
        }
        return count;
    }

    if (isfat12(&r)) {
        int count = ((struct fat12 *)r.mount->filesystem)->read(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) self->_offset += (size_t)count;
        return count;
    }

    if (isext2(&r)) {
        int count = ((struct ext2 *)r.mount->filesystem)->read(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) {
            self->_offset += (size_t)count;
        }
        return count;
    }

    if (!isbfs(&r)) {
        return -1;
    }

    bytes = ((struct bfs *)r.mount->filesystem)->data(r.mount->filesystem, r.sub, &length);
    if (!bytes) {
        return -1;
    }

    if (self->_offset >= length) {
        return 0;
    }

    count = min(size, length - self->_offset);
    memcpy(buffer, bytes + self->_offset, count);
    self->_offset += count;
    return (int)count;
}
static int file_write(struct vfile *self, const char *buffer, size_t size) {
    struct resolved r;
    const uint8_t *old;
    size_t oldlen = 0;
    size_t prefix;
    char *copy;
    int ok;

    if (!self || !self->_writable || (!buffer && size)) {
        return -1;
    }

    if (!resolve(self->_fs, self->_path, &r)) {
        return -1;
    }

    if (isdevfs(&r)) {
        int count = ((struct devfs *)r.mount->filesystem)->write(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) {
            self->_offset += (size_t)count;
        }
        return count;
    }

    if (isfat32(&r)) {
        int count = ((struct fat32 *)r.mount->filesystem)->write(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) {
            self->_offset += (size_t)count;
        }
        return count;
    }

    if (isfat12(&r)) {
        int count = ((struct fat12 *)r.mount->filesystem)->write(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) self->_offset += (size_t)count;
        return count;
    }

    if (isext2(&r)) {
        int count = ((struct ext2 *)r.mount->filesystem)->write(r.mount->filesystem, r.sub, self->_offset, buffer, size);
        if (count > 0) {
            self->_offset += (size_t)count;
        }
        return count;
    }

    if (!isbfs(&r)) {
        return -1;
    }

    old = ((struct bfs *)r.mount->filesystem)->data(r.mount->filesystem, r.sub, &oldlen);
    prefix = min(self->_offset, oldlen);

    copy = kcalloc(prefix + size + 1u, 1u);
    if (!copy) {
        return -1;
    }

    if (prefix && old) {
        memcpy(copy, old, prefix);
    }
    if (size) {
        memcpy(copy + prefix, buffer, size);
    }
    copy[prefix + size] = '\0';

    ok = ((struct bfs *)r.mount->filesystem)->writebytes(r.mount->filesystem, r.sub, copy, prefix + size);
    kfree(copy);

    if (!ok) {
        return -1;
    }

    self->_offset = prefix + size;
    return (int)size;
}
static int file_seek(struct vfile *self, long offset, int whence, size_t *out) {
    struct vstat st;
    size_t base;
    size_t next;

    if (!self) {
        return 0;
    }

    if (whence == SEEK_SET) {
        base = 0;
    } else if (whence == SEEK_CUR) {
        base = self->_offset;
    } else if (whence == SEEK_END) {
        if (!self->_fs || !self->_path || !self->_fs->stat(self->_fs, self->_path, &st) || st.type != VFS_FILE) {
            return 0;
        }
        base = st.size;
    } else {
        return 0;
    }

    if (offset < 0) {
        size_t amount = (size_t)(-(offset + 1)) + 1u;
        if (amount > base) {
            return 0;
        }
        next = base - amount;
    } else {
        size_t amount = (size_t)offset;
        if (amount > (~(size_t)0) - base) {
            return 0;
        }
        next = base + amount;
    }

    self->_offset = next;
    if (out) {
        *out = next;
    }
    return 1;
}

static uint32_t file_ready(struct vfile *self, uint32_t events) {
    unused(self);
    return events & (POLLIN | POLLOUT);
}

static void file_retain(struct vfile *self) {
    if (self) {
        self->_refs++;
    }
}

static void file_close(struct vfile *self) {
    if (!self) {
        return;
    }

    if (self->_refs > 1) {
        self->_refs--;
        return;
    }

    kfree(self->_path);
    kfree(self);
}

static int writable(const char *mode) {
    return strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
}

static struct vfile *open_impl(struct vfs *self, const char *path, const char *mode) {
    struct vfile *file;
    struct vstat st;
    struct resolved r;
    int canwrite;
    char full[VFS_PATH];

    if (!self || !path || !mode) {
        return nil;
    }

    if (!normalize(self, path, full, sizeof(full))) {
        return nil;
    }
    file = pipe_named_open(full, mode);
    if (file) return file;
    if (!resolve(self, full, &r)) {
        return nil;
    }

    canwrite = writable(mode);

    if (canwrite) {
        if (!self->stat(self, full, &st)) {
            if ((!isbfs(&r) && !isfat32(&r) && !isfat12(&r) && !isext2(&r)) || !self->create(self, full)) {
                return nil;
            }
            if (!self->stat(self, full, &st)) {
                return nil;
            }
        }
        if (st.type != VFS_FILE) {
            return nil;
        }
        if (strchr(mode, 'w')) {
            if (isbfs(&r) && !self->write(self, full, "")) {
                return nil;
            }
            if (isfat32(&r) && !((struct fat32 *)r.mount->filesystem)->truncate(r.mount->filesystem, r.sub)) {
                return nil;
            }
            if (isext2(&r) && !((struct ext2 *)r.mount->filesystem)->truncate(r.mount->filesystem, r.sub)) {
                return nil;
            }
            if (isfat12(&r) && !((struct fat12 *)r.mount->filesystem)->truncate(r.mount->filesystem, r.sub)) {
                return nil;
            }
        }
    } else if (!self->stat(self, full, &st) || st.type != VFS_FILE) {
        return nil;
    }

    file = kcalloc(1, sizeof(*file));
    if (!file) {
        return nil;
    }

    file->_path = kstrdup(full);
    if (!file->_path) {
        kfree(file);
        return nil;
    }

    file->read = file_read;
    file->write = file_write;
    file->seek = file_seek;
    file->ready = file_ready;
    file->retain = file_retain;
    file->close = file_close;
    file->_fs = self;
    file->_writable = canwrite;
    file->_refs = 1;

    if (strchr(mode, 'a')) {
        if (isbfs(&r)) {
            const char *text = self->read(self, full);
            file->_offset = text ? strlen(text) : 0;
        } else if ((isfat32(&r) || isext2(&r)) && self->stat(self, full, &st)) {
            file->_offset = st.size;
        }
    }

    return file;
}
struct collect {
    struct ventry *entries;
    size_t count;
    size_t index;
};

static void count_bfs(void *ctx, const char *name, enum bfstype kind, size_t size) {
    struct collect *c = ctx;
    unused(name); unused(kind); unused(size);
    c->count++;
}

static void fill_bfs(void *ctx, const char *name, enum bfstype kind, size_t size) {
    struct collect *c = ctx;
    struct ventry *entry = &c->entries[c->index++];
    strncpy(entry->name, name, sizeof(entry->name));
    entry->type = btype(kind);
    entry->size = size;
}

static void count_dev(void *ctx, const char *name, enum devfstype kind, size_t size) {
    struct collect *c = ctx;
    unused(name); unused(kind); unused(size);
    c->count++;
}

static void fill_dev(void *ctx, const char *name, enum devfstype kind, size_t size) {
    struct collect *c = ctx;
    struct ventry *entry = &c->entries[c->index++];
    strncpy(entry->name, name, sizeof(entry->name));
    entry->type = dtype(kind);
    entry->size = size;
}

static void count_fat32(void *ctx, const char *name, enum fat32_node_type kind, size_t size) {
    struct collect *c = ctx;
    unused(name); unused(kind); unused(size);
    c->count++;
}

static void fill_fat32(void *ctx, const char *name, enum fat32_node_type kind, size_t size) {
    struct collect *c = ctx;
    struct ventry *entry = &c->entries[c->index++];
    strncpy(entry->name, name, sizeof(entry->name));
    entry->type = ftype(kind);
    entry->size = size;
}

static void count_fat12(void *ctx, const char *name, enum fat12_node_type kind, size_t size) {
    struct collect *c = ctx; unused(name); unused(kind); unused(size); c->count++;
}

static void fill_fat12(void *ctx, const char *name, enum fat12_node_type kind, size_t size) {
    struct collect *c = ctx; struct ventry *entry = &c->entries[c->index++];
    strncpy(entry->name, name, sizeof(entry->name)); entry->type = ftype12(kind); entry->size = size;
}

static void count_ext2(void *ctx, const char *name, enum ext2_node_type kind, size_t size) {
    struct collect *c = ctx;
    unused(name); unused(kind); unused(size);
    c->count++;
}

static void fill_ext2(void *ctx, const char *name, enum ext2_node_type kind, size_t size) {
    struct collect *c = ctx;
    struct ventry *entry = &c->entries[c->index++];
    strncpy(entry->name, name, sizeof(entry->name));
    entry->type = etype(kind);
    entry->size = size;
}

static int childmount(struct vmount *mount, const char *dir, char *name, size_t size) {
    size_t dlen = strlen(dir);
    const char *rest;
    const char *slash;

    if (strcmp(mount->path, "/") == 0 || strcmp(mount->path, dir) == 0) {
        return 0;
    }

    if (strcmp(dir, "/") == 0) {
        rest = mount->path + 1;
    } else {
        if (strncmp(mount->path, dir, dlen) != 0 || mount->path[dlen] != '/') {
            return 0;
        }
        rest = mount->path + dlen + 1;
    }

    slash = strchr(rest, '/');
    if (slash) {
        return 0;
    }

    strncpy(name, rest, size);
    return 1;
}

static void count_mounts(struct vfs *self, const char *dir, struct collect *c) {
    char name[VFS_NAME];
    for (unsigned i = 0; i < self->_mount_count; i++) {
        if (childmount(&self->_mounts[i], dir, name, sizeof(name))) {
            c->count++;
        }
    }
}

static void fill_mounts(struct vfs *self, const char *dir, struct collect *c) {
    char name[VFS_NAME];
    for (unsigned i = 0; i < self->_mount_count; i++) {
        if (childmount(&self->_mounts[i], dir, name, sizeof(name))) {
            struct ventry *entry = &c->entries[c->index++];
            strncpy(entry->name, name, sizeof(entry->name));
            entry->type = VFS_DIR;
            entry->size = 0;
        }
    }
}

static int dir_read(struct vdir *self, struct ventry *entry) {
    if (!self || !entry || self->_index >= self->_count) {
        return 0;
    }
    *entry = self->_entries[self->_index++];
    return 1;
}

static void dir_retain(struct vdir *self) {
    if (self) {
        self->_refs++;
    }
}

static void dir_close(struct vdir *self) {
    if (!self) {
        return;
    }

    if (self->_refs > 1) {
        self->_refs--;
        return;
    }

    kfree(self->_entries);
    kfree(self);
}

static struct vdir *opendir_impl(struct vfs *self, const char *path) {
    struct resolved r;
    struct vstat st;
    struct vdir *dir;
    struct collect c;

    if (!resolve(self, path, &r) || !self->stat(self, r.full, &st) || st.type != VFS_DIR) {
        return nil;
    }

    memset(&c, 0, sizeof(c));

    if (isbfs(&r)) {
        ((struct bfs *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, count_bfs, &c);
    } else if (isdevfs(&r)) {
        ((struct devfs *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, count_dev, &c);
    } else if (isfat32(&r)) {
        ((struct fat32 *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, count_fat32, &c);
    } else if (isfat12(&r)) {
        ((struct fat12 *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, count_fat12, &c);
    } else if (isext2(&r)) {
        ((struct ext2 *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, count_ext2, &c);
    }
    count_mounts(self, r.full, &c);

    dir = kcalloc(1, sizeof(*dir));
    if (!dir) {
        return nil;
    }

    if (c.count) {
        c.entries = kcalloc(c.count, sizeof(struct ventry));
        if (!c.entries) {
            kfree(dir);
            return nil;
        }
        c.index = 0;
        if (isbfs(&r)) {
            ((struct bfs *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, fill_bfs, &c);
        } else if (isdevfs(&r)) {
            ((struct devfs *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, fill_dev, &c);
        } else if (isfat32(&r)) {
            ((struct fat32 *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, fill_fat32, &c);
        } else if (isfat12(&r)) {
            ((struct fat12 *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, fill_fat12, &c);
        } else if (isext2(&r)) {
            ((struct ext2 *)r.mount->filesystem)->each(r.mount->filesystem, r.sub, fill_ext2, &c);
        }
        fill_mounts(self, r.full, &c);
    }

    dir->read = dir_read;
    dir->retain = dir_retain;
    dir->close = dir_close;
    dir->_entries = c.entries;
    dir->_count = c.count;
    dir->_index = 0;
    dir->_refs = 1;
    return dir;
}

void vfs(struct vfs *self, struct bfs *root) {
    memset(self, 0, sizeof(*self));
    self->mount = mount_impl;
    self->mountfs = mountfs_impl;
    self->unmount = unmount_impl;
    self->mountentries = mountentries_impl;
    self->block = block_impl;
    self->mounts = mounts_impl;
    self->open = open_impl;
    self->opendir = opendir_impl;
    self->stat = stat_impl;
    self->mkdir = mkdir_impl;
    self->rmdir = rmdir_impl;
    self->create = create_impl;
    self->write = write_impl;
    self->read = read_impl;
    self->ioctl = ioctl_impl;
    self->unlink = unlink_impl;
    self->chdir = chdir_impl;
    self->cwd = cwd_impl;
    self->realpath = realpath_impl;
    strncpy(self->_cwd, "/", sizeof(self->_cwd));
    self->mount(self, "/", "bfs", root);
}
