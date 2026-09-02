#ifndef MONARCH_KERNEL_FS_DEVFS_H
#define MONARCH_KERNEL_FS_DEVFS_H 1

#include "base/api/monarch.h"
#include "drivers/block/block.h"
#include "drivers/char/console.h"
#include "drivers/char/keyboard.h"
#include "drivers/char/mouse.h"
#include "drivers/char/serial.h"
#include "drivers/sound/ac97.h"
#include "drivers/video/framebuffer.h"

#define DEVFS_NAME 32u
#define DEVFS_BLOCKS 8u

enum devfstype {
    DEVFS_NONE = 0,
    DEVFS_FILE = 1,
    DEVFS_DIR = 2
};

struct devfsstat {
    enum devfstype type;
    size_t size;
};

typedef void (*devfsiter)(void *ctx, const char *name, enum devfstype type, size_t size);

struct devfs {
    int (*stat)(struct devfs *self, const char *path, struct devfsstat *out);
    const char *(*describe)(struct devfs *self, const char *path);
    int (*read)(struct devfs *self, const char *path, size_t offset, char *buffer, size_t size);
    int (*write)(struct devfs *self, const char *path, size_t offset, const char *buffer, size_t size);
    int (*ioctl)(struct devfs *self, const char *path, uint32_t request, uintptr_t arg);
    void (*each)(struct devfs *self, const char *path, devfsiter iter, void *ctx);
    void (*attach)(struct devfs *self, struct blockdevice *device);
    struct blockdevice *(*block)(struct devfs *self, const char *path);

    struct console *_console;
    struct serial *_serial;
    struct keyboard *_keyboard;
    struct mouse *_mouse;
    struct framebuffer *_framebuffer;
    struct ac97 *_ac97;
    struct blockdevice *_blocks[DEVFS_BLOCKS];
    unsigned _block_count;
    char _buffer[256];
};

void devfs(struct devfs *self, struct console *screen, struct serial *serial, struct keyboard *keyboard, struct mouse *mouse, struct framebuffer *fb, struct ac97 *audio);

#endif /* MONARCH_KERNEL_FS_DEVFS_H */
