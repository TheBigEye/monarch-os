#include "kernel/fs/devfs.h"

struct device_entry {
    const char *name;
    int readable;
    int writable;
    const char *description;
};

static const struct device_entry devices[] = {
    { "console",  1, 1, "console character device\n" },
    { "serial",   1, 1, "serial character device\n" },
    { "keyboard", 1, 0, "keyboard character device\n" },
    { "mouse",    1, 0, "mouse character device\n" },
    { "fb0",      1, 1, "linear framebuffer byte device\n" },
    { "fbinfo",   1, 0, "linear framebuffer metadata\n" },
    { "audio",    0, 1, "fixed s16le stereo 48000 Hz audio output\n" },
    { "audioinfo", 1, 0, "audio device metadata\n" },
    { "null",     1, 1, "null device\n" },
    { "zero",     1, 0, "zero device\n" },
};

static const char *name(const char *path) {
    if (!path || !*path || strcmp(path, "/") == 0) {
        return "";
    }
    while (*path == '/') {
        path++;
    }
    return path;
}

static struct blockdevice *findblock(struct devfs *self, const char *path) {
    const char *n = name(path);

    if (!self || !n || !*n) {
        return nil;
    }

    for (unsigned i = 0; i < self->_block_count; i++) {
        if (self->_blocks[i] && strcmp(n, self->_blocks[i]->name) == 0) {
            return self->_blocks[i];
        }
    }
    return nil;
}

static const struct device_entry *find(const char *path) {
    const char *n = name(path);

    for (size_t i = 0; i < countof(devices); i++) {
        if (strcmp(n, devices[i].name) == 0) {
            return &devices[i];
        }
    }

    return nil;
}

static int isfb0(const char *path) {
    return strcmp(name(path), "fb0") == 0;
}

static int isfbinfo(const char *path) {
    return strcmp(name(path), "fbinfo") == 0;
}

static int isaudio(const char *path) {
    return strcmp(name(path), "audio") == 0;
}

static int isaudioinfo(const char *path) {
    return strcmp(name(path), "audioinfo") == 0;
}

static size_t fbinfo_text(struct devfs *self, char *buffer, size_t size) {
    struct framebuffer *fb = self ? self->_framebuffer : nil;

    if (!buffer || !size) {
        return 0;
    }

    if (!fb || !fb->ready(fb)) {
        snprintf(buffer, size, "fb0 ready=0\n");
    } else {
        snprintf(buffer, size,
            "fb0 ready=1 width=%u height=%u pitch=%u bpp=%u bytes=%u red_pos=%u red_size=%u green_pos=%u green_size=%u blue_pos=%u blue_size=%u\n",
            fb->width,
            fb->height,
            fb->pitch,
            fb->bpp,
            (unsigned)fb->bytes,
            fb->red_position,
            fb->red_mask_size,
            fb->green_position,
            fb->green_mask_size,
            fb->blue_position,
            fb->blue_mask_size);
    }

    return strlen(buffer);
}

static size_t audioinfo_text(struct devfs *self, char *buffer, size_t size) {
    const char *text = ac97_info(self ? self->_ac97 : nil, buffer, size);
    return strlen(text);
}

static void audioinfo_fill(struct devfs *self, struct audioinfo *out) {
    struct ac97 *audio = self ? self->_ac97 : nil;

    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (!audio || !audio->present) {
        return;
    }

    /* Refresh AC'97's software view of the ring before copying counters. */
    (void)audioinfo_text(self, self->_buffer, sizeof(self->_buffer));

    out->ready = audio->dma_ready ? 1u : 0u;
    out->format = AUDIO_FMT_S16LE;
    out->sample_rate = audio->sample_rate;
    out->channels = 2;
    out->bits_per_sample = 16;
    out->buffer_bytes = (uint32_t)audio->buffer_bytes;
    out->queued = audio->queued;
    out->running = audio->running ? 1u : 0u;
    out->completed = audio->completed;
    out->underruns = audio->underruns;
    out->errors = audio->errors;
}

static int stat_impl(struct devfs *self, const char *path, struct devfsstat *out) {
    const struct device_entry *entry;
    struct blockdevice *block;

    if (!out) {
        return 0;
    }

    memset(out, 0, sizeof(*out));

    if (!path || !*path || strcmp(path, "/") == 0) {
        out->type = DEVFS_DIR;
        return 1;
    }

    block = findblock(self, path);
    if (block) {
        out->type = DEVFS_FILE;
        out->size = block->size(block);
        return 1;
    }

    if (isfb0(path)) {
        struct framebuffer *fb = self->_framebuffer;
        if (!fb || !fb->ready(fb)) {
            return 0;
        }
        out->type = DEVFS_FILE;
        out->size = fb->bytes;
        return 1;
    }

    if (isfbinfo(path)) {
        out->type = DEVFS_FILE;
        out->size = fbinfo_text(self, self->_buffer, sizeof(self->_buffer));
        return 1;
    }

    if (isaudioinfo(path)) {
        out->type = DEVFS_FILE;
        out->size = audioinfo_text(self, self->_buffer, sizeof(self->_buffer));
        return 1;
    }

    entry = find(path);
    if (!entry) {
        return 0;
    }

    out->type = DEVFS_FILE;
    out->size = 0;
    return 1;
}

static const char *describe_impl(struct devfs *self, const char *path) {
    const struct device_entry *entry;
    struct blockdevice *block;

    block = findblock(self, path);
    if (block) {
        snprintf(self->_buffer, sizeof(self->_buffer),
            "%s block device: %u sectors, %u bytes, %s%s\n",
            block->name,
            block->sectors,
            (unsigned)block->size(block),
            block->readonly ? "readonly" : "writable",
            (block->flags & BLOCK_PARTITION) ? " partition" : "");
        return self->_buffer;
    }

    if (isfb0(path) || isfbinfo(path)) {
        (void)fbinfo_text(self, self->_buffer, sizeof(self->_buffer));
        return self->_buffer;
    }

    if (isaudioinfo(path)) {
        (void)audioinfo_text(self, self->_buffer, sizeof(self->_buffer));
        return self->_buffer;
    }

    entry = find(path);
    if (!entry || !entry->readable) {
        return nil;
    }

    strncpy(self->_buffer, entry->description, sizeof(self->_buffer));
    return self->_buffer;
}

static int read_impl(struct devfs *self, const char *path, size_t offset, char *buffer, size_t size) {
    const struct device_entry *entry;
    const char *description;
    struct blockdevice *block;
    size_t count;

    if (!buffer || !size) {
        return -1;
    }

    block = findblock(self, path);
    if (block) {
        size_t total = block->size(block);
        if (offset >= total) {
            return 0;
        }
        count = min(size, total - offset);
        if (!block->readbytes || block->readbytes(block, offset, buffer, count) < 0) {
            return -1;
        }
        return (int)count;
    }

    if (isfb0(path)) {
        struct framebuffer *fb = self->_framebuffer;
        if (!fb || !fb->ready(fb)) {
            return -1;
        }
        if (offset >= fb->bytes) {
            return 0;
        }
        count = min(size, fb->bytes - offset);
        memcpy(buffer, fb->address + offset, count);
        return (int)count;
    }

    if (isfbinfo(path)) {
        size_t total = fbinfo_text(self, self->_buffer, sizeof(self->_buffer));
        if (offset >= total) {
            return 0;
        }
        count = min(size, total - offset);
        memcpy(buffer, self->_buffer + offset, count);
        return (int)count;
    }

    if (isaudioinfo(path)) {
        size_t total = audioinfo_text(self, self->_buffer, sizeof(self->_buffer));
        if (offset >= total) {
            return 0;
        }
        count = min(size, total - offset);
        memcpy(buffer, self->_buffer + offset, count);
        return (int)count;
    }

    entry = find(path);
    if (!entry || !entry->readable) {
        return -1;
    }

    if (strcmp(entry->name, "keyboard") == 0) {
        buffer[0] = self->_keyboard->read(self->_keyboard);
        return 1;
    }

    if (strcmp(entry->name, "mouse") == 0) {
        if (!self->_mouse || !self->_mouse->ready(self->_mouse)) {
            return -1;
        }
        snprintf(buffer, size, "x=%d y=%d z=%d buttons=%u packets=%u errors=%u id=%u\n",
            self->_mouse->x(self->_mouse),
            self->_mouse->y(self->_mouse),
            self->_mouse->z(self->_mouse),
            self->_mouse->buttons(self->_mouse),
            self->_mouse->packets(self->_mouse),
            self->_mouse->errors(self->_mouse),
            self->_mouse->id(self->_mouse));
        if (offset >= strlen(buffer)) {
            return 0;
        }
        count = min(size, strlen(buffer) - offset);
        memmove(buffer, buffer + offset, count);
        return (int)count;
    }

    if (strcmp(entry->name, "null") == 0) {
        return 0;
    }

    if (strcmp(entry->name, "zero") == 0) {
        memset(buffer, 0, size);
        return (int)size;
    }

    description = entry->description;
    if (offset >= strlen(description)) {
        return 0;
    }
    count = min(size, strlen(description) - offset);
    memcpy(buffer, description + offset, count);
    return (int)count;
}

static int write_impl(struct devfs *self, const char *path, size_t offset, const char *buffer, size_t size) {
    const struct device_entry *entry = find(path);
    size_t count;

    if (!buffer && size) {
        return -1;
    }

    if (isfb0(path)) {
        struct framebuffer *fb = self->_framebuffer;
        if (!fb || !fb->ready(fb)) {
            return -1;
        }
        if (offset >= fb->bytes) {
            return 0;
        }
        count = min(size, fb->bytes - offset);
        memcpy(fb->address + offset, buffer, count);
        return (int)count;
    }

    if (isaudio(path)) {
        unused(offset);
        if (!self->_ac97 || !self->_ac97->present || !self->_ac97->play) {
            return -1;
        }
        return self->_ac97->play(self->_ac97, buffer, size);
    }

    if (!entry || !entry->writable || !buffer) {
        return -1;
    }

    if (strcmp(entry->name, "console") == 0) {
        for (size_t i = 0; i < size; i++) {
            self->_console->put(self->_console, buffer[i]);
        }
        return (int)size;
    }

    if (strcmp(entry->name, "serial") == 0) {
        for (size_t i = 0; i < size; i++) {
            self->_serial->put(self->_serial, buffer[i]);
        }
        return (int)size;
    }

    if (strcmp(entry->name, "null") == 0) {
        return (int)size;
    }

    return -1;
}

static int ioctl_impl(struct devfs *self, const char *path, uint32_t request, uintptr_t arg) {
    const struct device_entry *entry = find(path);

    if (findblock(self, path)) {
        return -ENOTTY;
    }
    if (!entry) {
        return -ENODEV;
    }

    switch (request) {
        case TCGETS:
            if (strcmp(entry->name, "keyboard") == 0 ||
                strcmp(entry->name, "console") == 0 ||
                strcmp(entry->name, "serial") == 0) {
                return 0;
            }
            return -ENOTTY;
        case AUDIO_GETINFO:
            if (isaudio(path) || isaudioinfo(path)) {
                audioinfo_fill(self, (struct audioinfo *)arg);
                return 0;
            }
            return -ENOTTY;
        case KBD_GETEVENT:
            if (strcmp(entry->name, "keyboard") == 0 && self->_keyboard) {
                struct keyevent event;
                if (!self->_keyboard->poll(self->_keyboard, &event)) return -EAGAIN;
                ((struct kbd_event *)arg)->code = event.code;
                ((struct kbd_event *)arg)->ascii = (uint8_t)event.ascii;
                ((struct kbd_event *)arg)->pressed = event.pressed ? 1u : 0u;
                return 0;
            }
            return -ENOTTY;
        case MOUSE_GETINFO:
            if (strcmp(entry->name, "mouse") == 0 && self->_mouse && self->_mouse->ready(self->_mouse)) {
                struct mouseinfo *info = (struct mouseinfo *)arg;
                info->x = self->_mouse->x(self->_mouse);
                info->y = self->_mouse->y(self->_mouse);
                info->z = self->_mouse->z(self->_mouse);
                info->buttons = self->_mouse->buttons(self->_mouse);
                return 0;
            }
            return -ENOTTY;
        case MOUSE_GETEVENT:
            if (strcmp(entry->name, "mouse") == 0 && self->_mouse && self->_mouse->ready(self->_mouse)) {
                struct mouseevent event;
                if (!self->_mouse->poll(self->_mouse, &event)) return -EAGAIN;
                ((struct mouse_event *)arg)->x = event.x;
                ((struct mouse_event *)arg)->y = event.y;
                ((struct mouse_event *)arg)->dx = event.dx;
                ((struct mouse_event *)arg)->dy = event.dy;
                ((struct mouse_event *)arg)->dz = event.dz;
                ((struct mouse_event *)arg)->buttons = event.buttons;
                return 0;
            }
            return -ENOTTY;
        default:
            return -EINVAL;
    }
}

static void each_impl(struct devfs *self, const char *path, devfsiter iter, void *ctx) {
    if (!iter || (path && *path && strcmp(path, "/") != 0)) {
        return;
    }

    for (size_t i = 0; i < countof(devices); i++) {
        size_t size = 0;
        if (strcmp(devices[i].name, "fb0") == 0 && self->_framebuffer && self->_framebuffer->ready(self->_framebuffer)) {
            size = self->_framebuffer->bytes;
        } else if (strcmp(devices[i].name, "fbinfo") == 0) {
            size = fbinfo_text(self, self->_buffer, sizeof(self->_buffer));
        } else if (strcmp(devices[i].name, "audioinfo") == 0) {
            size = audioinfo_text(self, self->_buffer, sizeof(self->_buffer));
        }
        iter(ctx, devices[i].name, DEVFS_FILE, size);
    }

    for (unsigned i = 0; i < self->_block_count; i++) {
        if (self->_blocks[i]) {
            iter(ctx, self->_blocks[i]->name, DEVFS_FILE, self->_blocks[i]->size(self->_blocks[i]));
        }
    }
}

static void attach_impl(struct devfs *self, struct blockdevice *device) {
    if (!self || !device || !device->name || self->_block_count >= DEVFS_BLOCKS) {
        return;
    }

    for (unsigned i = 0; i < self->_block_count; i++) {
        if (self->_blocks[i] == device || strcmp(self->_blocks[i]->name, device->name) == 0) {
            return;
        }
    }

    self->_blocks[self->_block_count++] = device;
}

static struct blockdevice *block_impl(struct devfs *self, const char *path) {
    return findblock(self, path);
}

void devfs(struct devfs *self, struct console *screen, struct serial *serial, struct keyboard *keyboard, struct mouse *mouse, struct framebuffer *fb, struct ac97 *audio) {
    memset(self, 0, sizeof(*self));
    self->stat = stat_impl;
    self->describe = describe_impl;
    self->read = read_impl;
    self->write = write_impl;
    self->ioctl = ioctl_impl;
    self->each = each_impl;
    self->attach = attach_impl;
    self->block = block_impl;
    self->_console = screen;
    self->_serial = serial;
    self->_keyboard = keyboard;
    self->_mouse = mouse;
    self->_framebuffer = fb;
    self->_ac97 = audio;
}
