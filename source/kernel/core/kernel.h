#ifndef MONARCH_KERNEL_CORE_KERNEL_H
#define MONARCH_KERNEL_CORE_KERNEL_H 1

#include "boot/bootloader.h"
#include "drivers/block/harddisk.h"
#include "drivers/block/floppy.h"
#include "drivers/block/partition.h"
#include "drivers/char/console.h"
#include "drivers/char/keyboard.h"
#include "drivers/char/mouse.h"
#include "drivers/char/serial.h"
#include "drivers/char/tty.h"
#include "drivers/sound/ac97.h"
#include "drivers/sound/speaker.h"
#include "drivers/video/framebuffer.h"
#include "kernel/fs/bfs.h"
#include "kernel/fs/devfs.h"
#include "kernel/fs/vfs.h"
#include "kernel/memory/physical.h"
#include "kernel/module/initrd.h"

struct kernel {
    void (*run)(struct kernel *self);
    void (*stop)(struct kernel *self) __attribute__((noreturn));

    struct bootinfo boot;
    struct console console;
    struct serial serial;
    struct keyboard keyboard;
    struct mouse mouse;
    struct tty tty;
    struct speaker speaker;
    struct ac97 ac97;
    struct floppy_controller floppy;
    struct framebuffer framebuffer;
    struct harddisk harddisks[2];
    struct partition partitions[8];
    struct physical physical;
    struct bfs rootfs;
    struct devfs devfs;
    struct fat32 fatfs;
    struct ext2 ext2fs;
    struct initrd initrd;
    struct vfs fs;
};

extern struct kernel monarch;

void kernel(uint32_t magic, uintptr_t info) __attribute__((noreturn));

#endif /* MONARCH_KERNEL_CORE_KERNEL_H */
