#include "kernel/module/initrd.h"
#include "base/lib/cpio.h"
#include "kernel/memory/heap.h"

static struct initrd *active;

static void parents(struct vfs *fs, const char *path) {
    char temp[VFS_PATH];
    size_t len = strlen(path);

    if (len >= sizeof(temp)) {
        return;
    }

    strcpy(temp, path);

    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            fs->mkdir(fs, temp);
            *p = '/';
        }
    }
}

static int import_file(struct vfs *fs, const char *path, const uint8_t *data, size_t size) {
    struct vfile *file = fs->open(fs, path, "w");
    int ok;

    if (!file) {
        return 0;
    }

    ok = file->write(file, (const char *)data, size) == (int)size;
    file->close(file);
    return ok;
}

const uint8_t *initrdfind(struct initrd *self, const char *path, size_t *out_size) {
    if (!self || !path || !self->_image || !self->_size) {
        return nil;
    }
    return cpio_newc_find(self->_image, self->_size, path, out_size);
}

struct initrd *initrdactive(void) {
    return active;
}

struct load_ctx {
    struct vfs *fs;
    const char *where;
    unsigned imported;
};

static int import_entry(void *ctx, const struct cpio_newc_entry *entry) {
    struct load_ctx *load = ctx;
    char path[VFS_PATH];

    if (!load || !load->fs || !load->where || !entry || !entry->name) {
        return 0;
    }

    snprintf(path, sizeof(path), "%s/%s", load->where, entry->name);
    parents(load->fs, path);

    if ((entry->mode & CPIO_MODE_MASK) == CPIO_MODE_DIR) {
        load->fs->mkdir(load->fs, path);
        return 1;
    }

    if ((entry->mode & CPIO_MODE_MASK) == CPIO_MODE_FILE) {
        if (!import_file(load->fs, path, entry->data, entry->size)) {
            return 0;
        }
        load->imported++;
    }

    return 1;
}

static int load_impl(struct initrd *self, struct vfs *fs, const char *where) {
    struct load_ctx ctx;

    if (!self || !fs || !where || !self->_image || !self->_size) {
        return 0;
    }

    fs->mkdir(fs, where);
    memset(&ctx, 0, sizeof(ctx));
    ctx.fs = fs;
    ctx.where = where;

    if (!cpio_newc_each(self->_image, self->_size, import_entry, &ctx)) {
        return 0;
    }

    self->_count = ctx.imported;
    return 1;
}

static struct blockdevice *device_impl(struct initrd *self) {
    return self && self->_image ? &self->_disk.device : nil;
}

static size_t size_impl(struct initrd *self) {
    return self ? self->_size : 0;
}

static unsigned files_impl(struct initrd *self) {
    return self ? self->_count : 0;
}

void initrd(struct initrd *self, void *image, size_t size) {
    memset(self, 0, sizeof(*self));
    self->load = load_impl;
    self->device = device_impl;
    self->size = size_impl;
    self->files = files_impl;
    self->_image = image;
    self->_size = size;

    if (image && size) {
        ramdisk(&self->_disk, "ram0", image, size, 1);
    }

    active = self;
}
