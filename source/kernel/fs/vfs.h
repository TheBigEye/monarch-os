#ifndef MONARCH_KERNEL_FS_VFS_H
#define MONARCH_KERNEL_FS_VFS_H 1

#include "base/api/monarch.h"
#include "kernel/fs/bfs.h"
#include "kernel/fs/devfs.h"
#include "kernel/fs/fat32.h"
#include "kernel/fs/fat12.h"
#include "kernel/fs/ext2.h"

#define VFS_PATH 256u
#define VFS_NAME 64u
#define VFS_MOUNTS 16u
#define VFS_FAT32_MOUNTS 4u
#define VFS_FAT12_MOUNTS 2u
#define VFS_EXT2_MOUNTS 4u

enum vnodetype {
    VFS_UNKNOWN = 0,
    VFS_FILE = 1,
    VFS_DIR = 2
};

struct vstat {
    enum vnodetype type;
    size_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
};

struct ventry {
    char name[VFS_NAME];
    enum vnodetype type;
    size_t size;
};

struct vmount {
    char path[VFS_PATH];
    const char *kind;
    void *filesystem;
};

struct vfs;

struct vfile {
    int (*read)(struct vfile *self, char *buffer, size_t size);
    int (*write)(struct vfile *self, const char *buffer, size_t size);
    int (*seek)(struct vfile *self, long offset, int whence, size_t *out);
    uint32_t (*ready)(struct vfile *self, uint32_t events);
    void (*retain)(struct vfile *self);
    void (*close)(struct vfile *self);

    struct vfs *_fs;
    char *_path;
    size_t _offset;
    int _writable;
    int _nonblock;
    unsigned _refs;
};

struct vdir {
    int (*read)(struct vdir *self, struct ventry *entry);
    void (*retain)(struct vdir *self);
    void (*close)(struct vdir *self);

    struct ventry *_entries;
    size_t _count;
    size_t _index;
    unsigned _refs;
};

struct vfs {
    int (*mount)(struct vfs *self, const char *path, const char *kind, void *filesystem);
    int (*mountfs)(struct vfs *self, const char *source, const char *target, const char *type);
    int (*unmount)(struct vfs *self, const char *target);
    size_t (*mountentries)(struct vfs *self, struct mountent *entries, size_t count);
    struct blockdevice *(*block)(struct vfs *self, const char *path);
    void (*mounts)(struct vfs *self, void (*emit)(void *ctx, const char *text), void *ctx);
    struct vfile *(*open)(struct vfs *self, const char *path, const char *mode);
    struct vdir *(*opendir)(struct vfs *self, const char *path);
    int (*stat)(struct vfs *self, const char *path, struct vstat *out);
    int (*mkdir)(struct vfs *self, const char *path);
    int (*rmdir)(struct vfs *self, const char *path);
    int (*create)(struct vfs *self, const char *path);
    int (*write)(struct vfs *self, const char *path, const char *text);
    const char *(*read)(struct vfs *self, const char *path);
    int (*ioctl)(struct vfs *self, const char *path, uint32_t request, uintptr_t arg);
    int (*unlink)(struct vfs *self, const char *path);
    int (*chdir)(struct vfs *self, const char *path);
    const char *(*cwd)(struct vfs *self, char *buffer, size_t size);
    const char *(*realpath)(struct vfs *self, const char *path, char *buffer, size_t size);
    struct vmount _mounts[VFS_MOUNTS];
    unsigned _mount_count;
    struct fat32 _fat32_mounts[VFS_FAT32_MOUNTS];
    uint8_t _fat32_used[VFS_FAT32_MOUNTS];
    struct fat12 _fat12_mounts[VFS_FAT12_MOUNTS];
    uint8_t _fat12_used[VFS_FAT12_MOUNTS];
    struct ext2 _ext2_mounts[VFS_EXT2_MOUNTS];
    uint8_t _ext2_used[VFS_EXT2_MOUNTS];
    char _cwd[VFS_PATH];
};

void vfs(struct vfs *self, struct bfs *root);

#endif /* MONARCH_KERNEL_FS_VFS_H */
