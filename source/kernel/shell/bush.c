/**
 * @file bush.c
 * @brief Kernel bootstrap/debug shell plus BGL demos and process launcher.
 *
 */

#include "kernel/shell/bush.h"
#include "arch/x86/cpu.h"
#include "arch/x86/paging.h"
#include "arch/x86/pit.h"
#include "arch/x86/rtc.h"
#include "arch/x86/syscall.h"
#include "drivers/video/framebuffer.h"
#include "drivers/block/block.h"
#include "drivers/char/mouse.h"
#include "base/gfx/actor.h"
#include "base/gfx/bitmap.h"
#include "base/gfx/dirty.h"
#include "base/gfx/image.h"
#include "base/gfx/mbi.h"
#include "base/gfx/font.h"
#include "base/gfx/render.h"
#include "base/gfx/text.h"
#include "base/gfx/shape.h"
#include "base/gfx/sprite.h"
#include "base/sys/gfx_fb.h"
#include "base/lib/hash.h"
#include "base/lib/path.h"
#include "kernel/core/debug.h"
#include "kernel/debug/fdinfo.h"
#include "kernel/core/elf.h"
#include "kernel/core/exec.h"
#include "kernel/core/version.h"
#include "kernel/memory/heap.h"
#include "kernel/memory/physical.h"
#include "kernel/scheduler/process.h"
#include "kernel/scheduler/thread.h"

static void emit(void *ctx, const char *text) {
    struct tty *term = ctx;
    term->write(term, text);
}

static void psline(void *ctx, struct process *proc) {
    struct tty *term = ctx;
    term->printf(term, "%u %-8s ppid=%u exit=%d cr3=%p image=%p..%p cwd=%s %s\n",
        proc->pid,
        processstate(proc->state),
        proc->ppid,
        proc->exit_code,
        (void *)proc->space,
        (void *)proc->image_start,
        (void *)proc->image_end,
        processcwd(proc),
        proc->name);
}

static void threadline(void *ctx, struct thread *thread) {
    struct tty *term = ctx;
    term->printf(term, "%u %-8s pid=%u %s\n",
        thread->tid,
        threadstate(thread->state),
        thread->process ? thread->process->pid : 0,
        thread->name);
}

static void worker(void *arg) {
    unused(arg);
    for (int i = 0; i < 5; i++) {
        char line[96];
        struct thread *self = threadcurrent();
        snprintf(line, sizeof(line), "[thread %s pid=%u] step %d\n",
            self ? self->name : "?",
            self && self->process ? self->process->pid : 0,
            i);
        syswrite(1, line, strlen(line));
        threadsleep(100);
    }
    syswrite(1, "[thread] done\n", 14);
}

static void gfxdemo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    self->_tty->_console->move(self->_tty->_console, 0, 0);
    render_framebuffer(&r, fb);
    r.clear(&r, 0x080c14);

    for (uint32_t y = 0; y < fb->height; y++) {
        uint8_t g = (uint8_t)((y * 255u) / fb->height);
        r.rect(&r, 0, y, fb->width, 1, ((uint32_t)(g / 3u) << 8) | g);
    }

    r.rect(&r, 40, 40, fb->width - 80, fb->height - 80, 0x141420);
    r.rect(&r, 56, 56, fb->width - 112, fb->height - 112, 0x508cdc);
    r.rect(&r, 72, 72, fb->width - 144, fb->height - 144, 0x101018);
    r.line(&r, 72, 72, (int)fb->width - 72, (int)fb->height - 72, 0xffcc55);
    r.line(&r, (int)fb->width - 72, 72, 72, (int)fb->height - 72, 0xffcc55);
    r.text(&r, "Monarch BGL linear framebuffer", 96, 96, 0xffffff, 0x101018);
    r.text(&r, "No planar VGA. No pain.", 96, 112, 0x55ffcc, 0x101018);
    r.text(&r, "BGL framebuffer demo complete. type clear to restore console.", 16, 16, 0xffffff, 0x000000);
    self->_tty->_console->move(self->_tty->_console, 3, 0);
}

static char *readall(const char *path, size_t *out_size) {
    struct vstat st;
    long fd;
    long count;
    char *buffer;

    if (sysstat(path, &st) < 0 || st.type != VFS_FILE) {
        return nil;
    }

    buffer = kmalloc(st.size + 1u);
    if (!buffer) {
        return nil;
    }

    fd = sysopen(path, OREAD);
    if (fd < 0) {
        kfree(buffer);
        return nil;
    }

    count = sysread((int)fd, buffer, st.size);
    sysclose((int)fd);
    if (count < 0) {
        kfree(buffer);
        return nil;
    }

    buffer[count] = '\0';
    if (out_size) {
        *out_size = (size_t)count;
    }
    return buffer;
}

static void imagedemo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    char *buffer;
    size_t size;
    struct surface *img;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    buffer = readall("/initrd/share/gradient.ppm", &size);
    if (!buffer) {
        self->_tty->write(self->_tty, "cannot read /initrd/share/gradient.ppm\n");
        return;
    }

    img = image_ppm(buffer, size);
    kfree(buffer);
    if (!img) {
        self->_tty->write(self->_tty, "cannot parse ppm image\n");
        return;
    }

    self->_tty->_console->move(self->_tty->_console, 0, 0);
    render_framebuffer(&r, fb);
    r.clear(&r, 0x000000);
    for (uint32_t y = 0; y < fb->height; y += img->height) {
        for (uint32_t x = 0; x < fb->width; x += img->width) {
            r.blit(&r, img, x, y);
        }
    }
    r.text(&r, "/initrd/share/gradient.ppm", 16, 16, 0xffffff, 0x000000);
    r.text(&r, "PPM image rendered from initrd. type clear to restore console.", 16, 32, 0xffffff, 0x000000);
    surface_destroy(img);
    self->_tty->_console->move(self->_tty->_console, 5, 0);
}

static struct surface *loadbmpfile(struct bush *self, const char *path) {
    char *buffer;
    size_t size;
    struct surface *img;

    buffer = readall(path, &size);
    if (!buffer) {
        self->_tty->printf(self->_tty, "cannot read %s\n", path);
        return nil;
    }

    img = bitmap_load(buffer, size);
    kfree(buffer);
    if (!img) {
        self->_tty->printf(self->_tty, "cannot parse %s\n", path);
    }
    return img;
}

static struct surface *loadmbifile(struct bush *self, const char *path) {
    char *buffer;
    size_t size;
    struct surface *img;

    buffer = readall(path, &size);
    if (!buffer) {
        self->_tty->printf(self->_tty, "cannot read %s\n", path);
        return nil;
    }

    img = mbi_load(buffer, size);
    kfree(buffer);
    if (!img) {
        self->_tty->printf(self->_tty, "cannot parse %s\n", path);
    }
    return img;
}

static void bmpdemo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    struct surface *img;
    uint32_t x;
    uint32_t y;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    img = loadbmpfile(self, "/initrd/share/butter.bmp");
    if (!img) {
        return;
    }

    self->_tty->_console->move(self->_tty->_console, 0, 0);
    render_framebuffer(&r, fb);
    r.clear(&r, 0x000000);
    x = fb->width > img->width ? (fb->width - img->width) / 2u : 0;
    y = fb->height > img->height ? (fb->height - img->height) / 2u : 0;
    r.blit(&r, img, x, y);
    r.text(&r, "/initrd/share/butter.bmp", 16, 16, 0xffffff, 0x000000);
    r.text(&r, "BMP image rendered from initrd. type clear to restore console.", 16, 32, 0xffffff, 0x000000);
    self->_tty->_console->move(self->_tty->_console, (y + img->height + 16u) / 8u, 0);
    surface_destroy(img);
}

static void sprite_demo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    struct surface *img;
    struct sprite *spr;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    img = loadbmpfile(self, "/initrd/share/bigeye.bmp");
    if (!img) {
        return;
    }

    spr = sprite_create(img, 1);
    if (!spr) {
        surface_destroy(img);
        return;
    }

    render_framebuffer(&r, fb);
    r.clear(&r, 0x202020);
    for (uint32_t y = 0; y < fb->height; y += 32) {
        for (uint32_t x = 0; x < fb->width; x += 32) {
            r.rect(&r, x, y, 32, 32, ((x / 32 + y / 32) & 1) ? 0x303040 : 0x101018);
        }
    }

    spr->key(spr, 0x000000, 1);
    spr->draw(spr, &r, 40, 80);
    spr->scale(spr, &r, 560, 120, 160, 160);
    r.text(&r, "sprite: colorkey black + scaled copy", 16, 16, 0xffffff, 0x000000);
    self->_tty->_console->move(self->_tty->_console, 5, 0);
    spr->destroy(spr);
}

static void alpha_demo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    struct surface *img;
    struct sprite *spr;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    img = loadbmpfile(self, "/initrd/share/bigeye.bmp");
    if (!img) {
        return;
    }

    spr = sprite_create(img, 1);
    if (!spr) {
        surface_destroy(img);
        return;
    }

    render_framebuffer(&r, fb);
    r.clear(&r, 0x102030);
    r.rect(&r, 0, 0, fb->width / 2, fb->height, 0x802020);
    r.rect(&r, fb->width / 2, 0, fb->width / 2, fb->height, 0x208020);
    spr->alpha(spr, 128);
    spr->draw(spr, &r, (fb->width - img->width) / 2u, (fb->height - img->height) / 2u);
    r.text(&r, "alpha: 50% blended sprite", 16, 16, 0xffffff, 0x000000);
    self->_tty->_console->move(self->_tty->_console, 5, 0);
    spr->destroy(spr);
}

static void clip_demo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    struct surface *img;
    struct sprite *spr;
    uint32_t cx = 110;
    uint32_t cy = 90;
    uint32_t cw = fb ? fb->width - 220u : 0;
    uint32_t ch = fb ? fb->height - 180u : 0;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    img = loadbmpfile(self, "/initrd/share/bigeye.bmp");
    if (!img) {
        return;
    }

    spr = sprite_create(img, 1);
    if (!spr) {
        surface_destroy(img);
        return;
    }

    render_framebuffer(&r, fb);
    r.clear(&r, 0x080808);
    for (uint32_t y = 0; y < fb->height; y += 32) {
        for (uint32_t x = 0; x < fb->width; x += 32) {
            r.rect(&r, x, y, 32, 32, ((x / 32u + y / 32u) & 1u) ? 0x202028 : 0x101014);
        }
    }

    r.clip(&r, (int32_t)cx, (int32_t)cy, cw, ch);
    spr->key(spr, 0x000000, 1);
    spr->draw(spr, &r, 20, 110);
    spr->source(spr, 145, 80, 220, 220);
    spr->scale(spr, &r, 505, 155, 230, 230);
    r.no_clip(&r);

    r.line(&r, (int)cx, (int)cy, (int)(cx + cw), (int)cy, 0xffcc55);
    r.line(&r, (int)cx, (int)(cy + ch), (int)(cx + cw), (int)(cy + ch), 0xffcc55);
    r.line(&r, (int)cx, (int)cy, (int)cx, (int)(cy + ch), 0xffcc55);
    r.line(&r, (int)(cx + cw), (int)cy, (int)(cx + cw), (int)(cy + ch), 0xffcc55);
    r.text(&r, "clip + source rect + scaled crop", 16, 16, 0xffffff, 0x000000);
    self->_tty->_console->move(self->_tty->_console, 5, 0);
    spr->destroy(spr);
}

static void mbidemo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    char *buffer;
    size_t size;
    struct surface *img;
    uint32_t x;
    uint32_t y;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    buffer = readall("/initrd/share/bigeye.mbi", &size);
    if (!buffer) {
        self->_tty->write(self->_tty, "cannot read /initrd/share/bigeye.mbi\n");
        return;
    }

    img = mbi_load(buffer, size);
    kfree(buffer);
    if (!img) {
        self->_tty->write(self->_tty, "cannot parse mbi image\n");
        return;
    }

    self->_tty->_console->move(self->_tty->_console, 0, 0);
    render_framebuffer(&r, fb);
    r.clear(&r, 0x000000);
    x = fb->width > img->width ? (fb->width - img->width) / 2u : 0;
    y = fb->height > img->height ? (fb->height - img->height) / 2u : 0;
    r.blit(&r, img, x, y);
    r.text(&r, "/initrd/share/bigeye.mbi", 16, 16, 0xffffff, 0x000000);
    r.text(&r, "MBI framebuffer-native image rendered from initrd.", 16, 32, 0xffffff, 0x000000);
    self->_tty->_console->move(self->_tty->_console, (y + img->height + 16u) / 8u, 0);
    surface_destroy(img);
}

static struct surface *native_surface(struct framebuffer *fb, struct surface *src) {
    struct surface *native;

    if (!src || !fb) {
        return src;
    }

    native = gfx_surface_convert(fb, src);
    if (native) {
        surface_destroy(src);
        return native;
    }

    return src;
}

static void boingdemo(struct bush *self) {
    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    struct renderer out;
    struct surface *screen = nil;
    struct surface *raw_background = nil;
    struct surface *background = nil;
    struct surface *raw_butterfly = nil;
    struct surface *butter64 = nil;
    struct surface *butter96 = nil;
    struct surface *circle = nil;
    struct surface *square = nil;
    struct surface *triangle = nil;
    struct surface *label = nil;
    struct surface *cursor = nil;
    struct sprite *eye64_sprite = nil;
    struct sprite *eye96_sprite = nil;
    struct sprite *circle_sprite = nil;
    struct sprite *square_sprite = nil;
    struct sprite *triangle_sprite = nil;
    struct sprite *label_sprite = nil;
    struct sprite *cursor_sprite = nil;
    struct keyevent event;
    struct mouse *m = mouseget();
    struct mouseevent mevent;
    int cursor_x = m ? m->x(m) : 8;
    int cursor_y = m ? m->y(m) : 8;
    int old_cursor_x = cursor_x;
    int old_cursor_y = cursor_y;
    struct dirty dirtylist;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    raw_background = loadbmpfile(self, "/initrd/share/myhill.bmp");
    raw_butterfly = loadmbifile(self, "/initrd/share/butter.mbi");
    if (!raw_background || !raw_butterfly) {
        goto cleanup;
    }

    background = native_surface(fb, surface_scale(raw_background, fb->width, fb->height));
    butter64 = native_surface(fb, surface_scale(raw_butterfly, 64, 64));
    butter96 = native_surface(fb, surface_scale(raw_butterfly, 96, 96));
    circle = native_surface(fb, shape_circle(56, 0xff2020));
    square = native_surface(fb, shape_rect(64, 64, 0x20dd40));
    triangle = native_surface(fb, shape_triangle(76, 64, 0xffdd20));
    label = native_surface(fb, shape_text("Monarch BGL Boing - press any key", 0xffffff, 0x000000));
    cursor = native_surface(fb, shape_cursor());

    screen = gfx_surface_native(fb, fb->width, fb->height);

    if (!screen || !background || !butter64 || !butter96 || !circle || !square || !triangle || !label || !cursor) {
        goto cleanup;
    }

    eye64_sprite = sprite_create(butter64, 1); butter64 = nil;
    eye96_sprite = sprite_create(butter96, 1); butter96 = nil;
    circle_sprite = sprite_create(circle, 1); circle = nil;
    square_sprite = sprite_create(square, 1); square = nil;
    triangle_sprite = sprite_create(triangle, 1); triangle = nil;
    label_sprite = sprite_create(label, 1); label = nil;
    cursor_sprite = sprite_create(cursor, 1); cursor = nil;

    if (!eye64_sprite || !eye96_sprite || !circle_sprite || !square_sprite || !triangle_sprite || !label_sprite || !cursor_sprite) {
        goto cleanup;
    }

    eye64_sprite->key(eye64_sprite, 0x000000, 1);
    eye96_sprite->key(eye96_sprite, 0x000000, 1);
    circle_sprite->key(circle_sprite, 0x000000, 1);
    triangle_sprite->key(triangle_sprite, 0x000000, 1);
    label_sprite->key(label_sprite, 0x000000, 1);
    cursor_sprite->key(cursor_sprite, 0x000000, 1);

    struct actor actors[9];
    actor_init(&actors[0], eye64_sprite,  40,  42,  3,  2, 64, 64);
    actor_init(&actors[1], eye64_sprite, 360,  80, -2,  3, 64, 64);
    actor_init(&actors[2], eye96_sprite, 620, 250, -4, -2, 96, 96);
    actor_init(&actors[3], eye96_sprite, 170, 390,  2, -3, 96, 96);
    actor_init(&actors[4], eye64_sprite, 500, 430,  4, -3, 64, 64);
    actor_init(&actors[5], circle_sprite, 100, 160,  3,  4, 56, 56);
    actor_init(&actors[6], square_sprite, 620,  90, -4,  3, 64, 64);
    actor_init(&actors[7], triangle_sprite, 360, 420,  5, -4, 76, 64);
    actor_init(&actors[8], label_sprite, 220,  22,  2,  2, label_sprite->_surface->width, 8);

    render_framebuffer(&out, fb);
    render_target(&r, screen);
    dirty_init(&dirtylist);
    self->_tty->_console->move(self->_tty->_console, 0, 0);
    r.blit(&r, background, 0, 0);
    for (unsigned i = 0; i < countof(actors); i++) {
        actor_draw(&actors[i], &r);
    }
    cursor_sprite->draw(cursor_sprite, &r, (uint32_t)cursor_x, (uint32_t)cursor_y);
    out.blit(&out, screen, 0, 0);

    while (!self->_tty->_keyboard->poll(self->_tty->_keyboard, &event)) {
        dirty_clear(&dirtylist);
        dirty_add(&dirtylist, rect_clip(rect_make(old_cursor_x, old_cursor_y, 16, 24), fb->width, fb->height));

        if (m && m->poll(m, &mevent)) {
            cursor_x = mevent.x;
            cursor_y = mevent.y;
        }
        dirty_add(&dirtylist, rect_clip(rect_make(cursor_x, cursor_y, 16, 24), fb->width, fb->height));

        for (unsigned i = 0; i < countof(actors); i++) {
            actor_dirty(&dirtylist, &actors[i], (int)fb->width, (int)fb->height);
        }

        old_cursor_x = cursor_x;
        old_cursor_y = cursor_y;

        for (uint32_t i = 0; i < dirtylist.count; i++) {
            struct rect rect = rect_clip(dirtylist.rects[i], fb->width, fb->height);
            if (rect.w && rect.h) {
                r.blit_rect(&r, background, &rect, (uint32_t)rect.x, (uint32_t)rect.y);
            }
        }

        for (unsigned i = 0; i < countof(actors); i++) {
            actor_draw(&actors[i], &r);
        }
        cursor_sprite->draw(cursor_sprite, &r, (uint32_t)cursor_x, (uint32_t)cursor_y);

        for (uint32_t i = 0; i < dirtylist.count; i++) {
            struct rect rect = rect_clip(dirtylist.rects[i], fb->width, fb->height);
            if (rect.w && rect.h) {
                out.blit_rect(&out, screen, &rect, (uint32_t)rect.x, (uint32_t)rect.y);
            }
        }

        threadsleep(16);
    }

cleanup:
    if (cursor_sprite) cursor_sprite->destroy(cursor_sprite);
    if (label_sprite) label_sprite->destroy(label_sprite);
    if (triangle_sprite) triangle_sprite->destroy(triangle_sprite);
    if (square_sprite) square_sprite->destroy(square_sprite);
    if (circle_sprite) circle_sprite->destroy(circle_sprite);
    if (eye96_sprite) eye96_sprite->destroy(eye96_sprite);
    if (eye64_sprite) eye64_sprite->destroy(eye64_sprite);
    if (cursor) surface_destroy(cursor);
    if (label) surface_destroy(label);
    if (triangle) surface_destroy(triangle);
    if (square) surface_destroy(square);
    if (circle) surface_destroy(circle);
    if (butter96) surface_destroy(butter96);
    if (butter64) surface_destroy(butter64);
    if (raw_butterfly) surface_destroy(raw_butterfly);
    if (screen) surface_destroy(screen);
    if (background) surface_destroy(background);
    if (raw_background) surface_destroy(raw_background);

    self->_tty->clear(self->_tty);
    self->_tty->write(self->_tty, "boing stopped\n");
}

static char *arg(char *line) {
    char *spaceptr = strchr(line, ' ');
    if (!spaceptr) {
        return nil;
    }
    *spaceptr++ = '\0';
    return trim(spaceptr);
}

static void printbytes(struct bush *self, char *buffer, long count) {
    int binary = 0;

    if (count < 0) {
        self->_tty->write(self->_tty, "read failed\n");
        return;
    }

    if (count == 0) {
        self->_tty->write(self->_tty, "eof\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        if ((buffer[i] < ' ' || buffer[i] > '~') && buffer[i] != '\n' && buffer[i] != '\t') {
            binary = 1;
        }
    }

    if (binary) {
        self->_tty->printf(self->_tty, "%d bytes:", (int)count);
        for (int i = 0; i < count; i++) {
            self->_tty->printf(self->_tty, " %02x", (unsigned char)buffer[i]);
        }
        self->_tty->write(self->_tty, "\n");
    } else {
        char text[1025];
        long safe = min((size_t)count, sizeof(text) - 1u);
        memcpy(text, buffer, (size_t)safe);
        text[safe] = '\0';
        self->_tty->printf(self->_tty, "%s\n", text);
    }
}

static int resolvecommand(const char *input, char *out, size_t size);

static void runprogram(struct bush *self, const char *path, int foreground, int verbose) {
    struct execresult result;
    char command[VFS_PATH];
    int status = 0;
    long waited;

    if (!path) {
        self->_status = 2;
        self->_tty->write(self->_tty, foreground ? "usage: runelf PATH [ARGS]\n" : "usage: exec PATH [ARGS]\n");
        return;
    }

    strncpy(command, path, sizeof(command));
    {
        char resolved[VFS_PATH];
        if (resolvecommand(command, resolved, sizeof(resolved))) {
            strncpy(command, resolved, sizeof(command));
        }
    }

    KLOG("shell", "%s command=%s", foreground ? "run" : "exec", command);
    if (!execspawn(self->_fs, command, processcurrent(), &result)) {
        KLOG("shell", "%s failed: %s", foreground ? "run" : "exec", execerror());
        self->_tty->printf(self->_tty, "%s: %s\n", foreground ? "runelf" : "exec", execerror());
        self->_status = 127;
        return;
    }

    if (verbose) {
        self->_tty->printf(self->_tty, "%s pid=%u tid=%u entry=%p image=%p..%p\n",
            foreground ? "runelf" : "exec",
            result.pid,
            result.tid,
            (void *)result.entry,
            (void *)result.start,
            (void *)result.end);
    }

    KLOG("shell", "%s pid=%u tid=%u entry=%p image=%p..%p", foreground ? "run" : "exec", result.pid, result.tid, (void *)result.entry, (void *)result.start, (void *)result.end);

    if (!foreground) {
        self->_status = 0;
        return;
    }

    waited = processwait((int)result.pid, &status);
    if (waited < 0) {
        self->_status = 1;
        KLOG("shell", "wait pid=%u failed", result.pid);
        self->_tty->write(self->_tty, "wait failed\n");
    } else {
        self->_status = status;
        if (verbose || status != 0) {
            self->_tty->printf(self->_tty, "pid=%d exit=%d\n", (int)waited, status);
        }
        KLOG("shell", "wait pid=%d exit=%d", (int)waited, status);
    }
}



static int readpath(char *buffer, size_t size) {
    long fd;
    long count;

    if (!buffer || !size) {
        return 0;
    }

    fd = sysopen("/initrd/etc/path", OREAD);
    if (fd >= 0) {
        count = sysread((int)fd, buffer, size - 1u);
        sysclose((int)fd);
        if (count > 0) {
            buffer[count] = '\0';
            return 1;
        }
    }

    strncpy(buffer, "/initrd/bin", size);
    return 1;
}

static int pathelf(const char *name, char *path, size_t size) {
    char list[128];
    char dir[VFS_PATH];
    const char *p;
    struct vstat st;

    if (!name || !path || !size) {
        return 0;
    }

    if (path_has_slash(name)) {
        strncpy(path, name, size);
        return sysstat(path, &st) == 0 && st.type == VFS_FILE;
    }

    if (!readpath(list, sizeof(list))) {
        return 0;
    }

    p = list;
    while (path_next_entry(&p, dir, sizeof(dir))) {
        if (path_join_ext(dir, name, ".elf", path, size) &&
            sysstat(path, &st) == 0 && st.type == VFS_FILE) {
            return 1;
        }
    }

    return 0;
}

static int resolvecommand(const char *input, char *out, size_t size) {
    char temp[VFS_PATH];
    char path[VFS_PATH];
    char *name;
    char *rest;

    if (!input || !out || !size) {
        return 0;
    }

    strncpy(temp, input, sizeof(temp));
    name = trim(temp);
    if (!*name) {
        return 0;
    }

    rest = arg(name);
    if (!pathelf(name, path, sizeof(path))) {
        return 0;
    }

    if (rest && *rest) {
        snprintf(out, size, "%s %s", path, rest);
    } else {
        snprintf(out, size, "%s", path);
    }
    return 1;
}

static int trybin(struct bush *self, const char *name, const char *rest) {
    char input[VFS_PATH];
    char command[VFS_PATH];

    if (!name) {
        return 0;
    }

    if (rest && *rest) {
        snprintf(input, sizeof(input), "%s %s", name, rest);
    } else {
        snprintf(input, sizeof(input), "%s", name);
    }

    if (!resolvecommand(input, command, sizeof(command))) {
        return 0;
    }

    runprogram(self, command, 1, 0);
    return 1;
}


static void command(struct bush *self, char *line);


static const char *const bush_commands[] = {
    "help", "clear", "cls", "version", "status", "debug", "ticks",
    "mem", "page", "paging", "maptest", "fb", "gfx", "image", "bmp",
    "mbi", "sprite", "alpha", "clip", "boing", "mouse",
    "beep", "ac97", "mounts", "initrd", "ram0", "fds",
    "proc", "ps", "spawn", "threads", "kthread", "yield", "nap",
    "sched", "preempt", "quantum", "int80", "elf", "runelf", "exec",
    "wait", "devwrite", "devread", "wing", "cd", "reboot", "shutdown", "halt"
};

static const char *const debug_commands[] = {
    "mem", "page", "paging", "maptest", "fb", "mouse",
    "ram0", "fds", "proc",
    "ps", "spawn", "threads", "kthread", "yield", "nap", "sched",
    "preempt", "quantum", "int80", "elf", "mounts", "initrd",
    "devwrite", "devread", "beep", "ac97", "ticks"
};

static int in_list(const char *name, const char *const *list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(name, list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int utility(const char *name) {
    static const char *const names[] = {
        "args", "badptr", "bmpcheck", "cat", "cksum", "clock", "dirtest", "edlin", "env", "echo", "ext2info", "fatinfo", "fdtest", "grep", "head", "irdcheck", "ll", "ls", "lsinitrd",
        "mbicheck", "mkdir", "mount", "partinfo", "pid", "pwd", "rm", "rmdir", "sleep", "stat", "touch", "tree", "true", "false", "umount", "uname", "ush", "wc",
        "write", "append", "wing"
    };

    for (unsigned i = 0; i < countof(names); i++) {
        if (strcmp(name, names[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int debugsub(const char *name) {
    return in_list(name, debug_commands, countof(debug_commands));
}

static void debughelp(struct bush *self) {
    self->_tty->write(self->_tty,
        "debug subcommands:\n"
        "  mem page paging maptest fb mouse\n"
        "  ram0\n"
        "  fds proc ps threads sched int80 elf\n"
        "  spawn kthread yield nap preempt quantum\n"
        "  mounts initrd devwrite devread beep ac97 floppy floppyformat floppyid floppyread floppywrite ticks\n"
    );
}

static void debugcmd(struct bush *self, char *rest) {
    char line[256];
    char *sub;

    if (!rest || !*rest || strcmp(rest, "help") == 0) {
        debughelp(self);
        return;
    }

    strncpy(line, rest, sizeof(line));
    sub = line;
    (void)arg(sub);

    if (!debugsub(sub)) {
        self->_tty->printf(self->_tty, "debug: unknown subcommand %s\n", sub);
        debughelp(self);
        return;
    }

    strncpy(line, rest, sizeof(line));
    command(self, line);
}
static int first_token(const char *text, char *out, size_t size) {
    size_t used = 0;

    if (!text || !out || !size) {
        return 0;
    }

    while (space(*text)) {
        text++;
    }
    while (*text && !space(*text)) {
        if (used + 1u < size) {
            out[used++] = *text;
        }
        text++;
    }
    out[used] = '\0';
    return used > 0;
}

static int shell_syntax(const char *text) {
    int quote = 0;
    int escape = 0;

    while (text && *text) {
        if (escape) {
            escape = 0;
        } else if (*text == '\\') {
            escape = 1;
        } else if (quote) {
            if (*text == quote) {
                quote = 0;
            }
        } else if (*text == '\'' || *text == '"') {
            quote = *text;
        } else if (*text == '|' || *text == '<' || *text == '>') {
            return 1;
        }
        text++;
    }

    return 0;
}

static int quote_ush_command(const char *line, char *out, size_t size) {
    const char *prefix = "/initrd/bin/ush.elf -c \"";
    size_t used = 0;

    if (!line || !out || !size || strlen(prefix) + 2u > size) {
        return 0;
    }

    strcpy(out, prefix);
    used = strlen(out);
    while (*line) {
        if (*line == '"' || *line == '\\') {
            if (used + 2u >= size) {
                return 0;
            }
            out[used++] = '\\';
        } else if (used + 1u >= size) {
            return 0;
        }
        out[used++] = *line++;
        out[used] = '\0';
    }

    if (used + 2u > size) {
        return 0;
    }
    out[used++] = '"';
    out[used] = '\0';
    return 1;
}

static int delegate_to_ush(struct bush *self, const char *line) {
    struct execresult result;
    char command[512];
    int status = 0;
    long waited;

    if (!quote_ush_command(line, command, sizeof(command))) {
        self->_status = 2;
        self->_tty->write(self->_tty, "ush delegate: command too long\n");
        return 1;
    }

    KLOG("shell", "delegate to ush: %s", line);
    if (!execspawn(self->_fs, command, processcurrent(), &result)) {
        self->_status = 127;
        KLOG("shell", "ush delegate failed: %s", execerror());
        self->_tty->printf(self->_tty, "ush: %s\n", execerror());
        return 1;
    }

    waited = processwait((int)result.pid, &status);
    if (waited < 0) {
        self->_status = 1;
        KLOG("shell", "ush delegate wait pid=%u failed", result.pid);
        self->_tty->write(self->_tty, "ush: wait failed\n");
    } else {
        self->_status = status;
        KLOG("shell", "ush delegate pid=%d exit=%d", (int)waited, status);
    }
    return 1;
}

static int should_delegate_to_ush(const char *line) {
    char name[VFS_NAME];

    if (!shell_syntax(line) || !first_token(line, name, sizeof(name))) {
        return 0;
    }

    if (!utility(name) && in_list(name, bush_commands, countof(bush_commands))) {
        return 0;
    }

    return 1;
}





#define COMPLETE_SHOW_MAX 12u

struct completion {
    unsigned count;
    unsigned shown;
    int overflow;
    char common[VFS_PATH];
    char one[VFS_PATH];
    char item[COMPLETE_SHOW_MAX][VFS_PATH];
    char suffix;
};

static int nonblank(const char *text) {
    while (text && *text) {
        if (!space(*text)) {
            return 1;
        }
        text++;
    }
    return 0;
}

static void history_add(struct bush *self, const char *line) {
    if (!self || !nonblank(line)) {
        return;
    }

    if (self->_history_count && strcmp(self->_history[self->_history_count - 1u], line) == 0) {
        return;
    }

    if (self->_history_count == BUSH_HISTORY_MAX) {
        memmove(self->_history[0], self->_history[1], sizeof(self->_history[0]) * (BUSH_HISTORY_MAX - 1u));
        self->_history_count--;
    }

    strncpy(self->_history[self->_history_count++], line, BUSH_LINE_MAX);
}

static int history_pick(void *ctx, int direction, char *buffer, size_t size) {
    struct bush *self = ctx;

    if (!self || !buffer || !size || !self->_history_count) {
        return 0;
    }

    if (direction < 0) {
        if (self->_history_view < 0) {
            self->_history_view = (int)self->_history_count;
            strncpy(self->_history_draft, buffer, sizeof(self->_history_draft));
        }
        if (self->_history_view > 0) {
            self->_history_view--;
            strncpy(buffer, self->_history[self->_history_view], size);
            return 1;
        }
        return 0;
    }

    if (self->_history_view < 0) {
        return 0;
    }
    if ((unsigned)(self->_history_view + 1) < self->_history_count) {
        self->_history_view++;
        strncpy(buffer, self->_history[self->_history_view], size);
    } else {
        self->_history_view = -1;
        strncpy(buffer, self->_history_draft, size);
    }
    return 1;
}

static void completion_add(struct completion *out, const char *text, char suffix) {
    size_t n = 0;

    if (!out || !text || !*text) {
        return;
    }

    for (unsigned i = 0; i < out->shown; i++) {
        if (strcmp(out->item[i], text) == 0) {
            return;
        }
    }

    if (out->shown < COMPLETE_SHOW_MAX) {
        strncpy(out->item[out->shown++], text, sizeof(out->item[0]));
    } else {
        out->overflow = 1;
    }

    if (out->count == 0) {
        strncpy(out->common, text, sizeof(out->common));
        strncpy(out->one, text, sizeof(out->one));
        out->suffix = suffix;
        out->count = 1;
        return;
    }

    while (out->common[n] && text[n] && out->common[n] == text[n]) {
        n++;
    }
    out->common[n] = '\0';
    out->count++;
}

static int replace_token(char *line, size_t size, size_t *used, size_t *cursor, size_t start, size_t end, const char *text) {
    size_t len;
    size_t tail;

    if (!line || !used || !cursor || !text || start > end || end > *used) {
        return 0;
    }

    len = strlen(text);
    if (*used - (end - start) + len + 1u > size) {
        return 0;
    }

    tail = *used - end;
    memmove(line + start + len, line + end, tail + 1u);
    if (len) {
        memcpy(line + start, text, len);
    }
    *used = *used - (end - start) + len;
    *cursor = start + len;
    return 1;
}

static int completion_apply(struct completion *matches, char *line, size_t size, size_t *used, size_t *cursor, size_t start, size_t end) {
    char text[VFS_PATH];
    const char *base;

    if (!matches || matches->count == 0) {
        return 0;
    }

    base = matches->count == 1u ? matches->one : matches->common;
    strncpy(text, base, sizeof(text));
    if (matches->count == 1u && matches->suffix && strlen(text) + 1u < sizeof(text)) {
        size_t len = strlen(text);
        text[len] = matches->suffix;
        text[len + 1u] = '\0';
    }

    if (strlen(text) <= end - start) {
        return 0;
    }

    return replace_token(line, size, used, cursor, start, end, text);
}

static void complete_paths(struct bush *self, const char *prefix, struct completion *matches) {
    char dir[VFS_PATH];
    char shown[VFS_PATH];
    char base[VFS_NAME];
    struct vdir *vdir;
    struct ventry entry;

    path_split_parent(prefix, dir, sizeof(dir), shown, sizeof(shown), base, sizeof(base));
    vdir = self->_fs->opendir(self->_fs, dir);
    if (!vdir) {
        return;
    }

    while (vdir->read(vdir, &entry)) {
        char text[VFS_PATH];
        if (!starts(entry.name, base)) {
            continue;
        }
        snprintf(text, sizeof(text), "%s%s", shown, entry.name);
        completion_add(matches, text, entry.type == VFS_DIR ? '/' : ' ');
    }
    vdir->close(vdir);
}

static void complete_path_commands(struct bush *self, const char *prefix, struct completion *matches) {
    char list[128];
    char dir[VFS_PATH];
    const char *p;

    if (!readpath(list, sizeof(list))) {
        return;
    }

    p = list;
    while (path_next_entry(&p, dir, sizeof(dir))) {
        struct vdir *vdir = self->_fs->opendir(self->_fs, dir);
        struct ventry entry;

        if (!vdir) {
            continue;
        }
        while (vdir->read(vdir, &entry)) {
            char name[VFS_NAME];
            if (entry.type == VFS_FILE && path_stem_suffix(entry.name, ".elf", name, sizeof(name)) && starts(name, prefix)) {
                completion_add(matches, name, ' ');
            }
        }
        vdir->close(vdir);
    }
}

static void complete_list(const char *prefix, const char *const *list, size_t count, struct completion *matches) {
    for (size_t i = 0; i < count; i++) {
        if (starts(list[i], prefix)) {
            completion_add(matches, list[i], ' ');
        }
    }
}

static size_t current_token_start(const char *line, size_t cursor) {
    size_t start = cursor;
    while (start > 0 && !space(line[start - 1u]) && line[start - 1u] != '|' && line[start - 1u] != '<' && line[start - 1u] != '>') {
        start--;
    }
    return start;
}

static int command_position(const char *line, size_t start) {
    int command = 1;

    for (size_t i = 0; i < start; i++) {
        if (line[i] == '|') {
            command = 1;
        } else if (!space(line[i])) {
            command = 0;
        }
    }
    return command;
}

static int first_word_before(const char *line, size_t start, const char *word) {
    size_t i = 0;
    size_t w = 0;
    char found[VFS_NAME];

    while (i < start && space(line[i])) {
        i++;
    }
    while (i < start && !space(line[i]) && line[i] != '|' && w + 1u < sizeof(found)) {
        found[w++] = line[i++];
    }
    found[w] = '\0';
    while (i < start && space(line[i])) {
        i++;
    }
    return strcmp(found, word) == 0 && i == start;
}

static int has_quotes(const char *text) {
    while (text && *text) {
        if (*text == '\'' || *text == '"') {
            return 1;
        }
        text++;
    }
    return 0;
}

static void completion_print(struct bush *self, const struct completion *matches) {
    if (!self || !matches || matches->count <= 1u) {
        return;
    }

    self->_tty->write(self->_tty, "\n");
    for (unsigned i = 0; i < matches->shown; i++) {
        self->_tty->write(self->_tty, matches->item[i]);
        self->_tty->write(self->_tty, "  ");
        if ((i % 3u) == 2u) {
            self->_tty->write(self->_tty, "\n");
        }
    }
    if (matches->shown && (matches->shown % 3u) != 0) {
        self->_tty->write(self->_tty, "\n");
    }
    if (matches->overflow) {
        self->_tty->write(self->_tty, "...\n");
    }
}

static int complete_line(void *ctx, char *line, size_t size, size_t *used, size_t *cursor) {
    struct bush *self = ctx;
    struct completion matches;
    char prefix[VFS_PATH];
    size_t start;
    size_t len;

    if (!self || !line || !used || !cursor || *cursor > *used) {
        return 0;
    }

    start = current_token_start(line, *cursor);
    len = *cursor - start;
    if (len >= sizeof(prefix)) {
        return 0;
    }
    memcpy(prefix, line + start, len);
    prefix[len] = '\0';
    if (has_quotes(prefix)) {
        return 0;
    }

    memset(&matches, 0, sizeof(matches));
    if (first_word_before(line, start, "debug")) {
        complete_list(prefix, debug_commands, countof(debug_commands), &matches);
    } else if (path_has_slash(prefix) || !command_position(line, start)) {
        complete_paths(self, prefix, &matches);
    } else {
        complete_list(prefix, bush_commands, countof(bush_commands), &matches);
        complete_path_commands(self, prefix, &matches);
    }

    if (completion_apply(&matches, line, size, used, cursor, start, *cursor)) {
        return 1;
    }
    if (matches.count > 1u) {
        completion_print(self, &matches);
        return 2;
    }
    return 0;
}

static void help(struct bush *self) {
    self->_tty->write(self->_tty,
        "commands:\n"
        "  help                 show this text\n"
        "  clear                clear the screen\n"
        "  version              show kernel version\n"
        "  status               show last foreground exit status\n"
        "  clock                show RTC date/time\n"
        "  cd DIR               change directory\n"
        "  runelf PATH [ARGS]   run an ELF executable and wait\n"
        "  exec PATH [ARGS]     start an ELF child in background\n"
        "  wait [PID]           wait for a child process\n"
        "  debug SUBCOMMAND     kernel/debug tools (debug help)\n"
        "  gfx image bmp mbi    framebuffer/BGL image demos\n"
        "  sprite alpha clip    BGL sprite demos\n"
        "  boing                animated BGL demo until keypress\n"
        "  ac97 [HZ MS]         AC'97 PCM-out test tone\n"
        "  reboot shutdown halt machine control\n"
        "\n"
        "external C utilities in /initrd/bin:\n"
        "  ls ll tree cat ext2info fatinfo grep head irdcheck lsinitrd mount partinfo wc echo clock pwd pid mkdir touch rm stat\n"
        "  write append edlin cksum bmpcheck mbicheck sleep args badptr fdtest dirtest true false env rmdir umount uname ush\n"
        "  seektest fbdemo fbclear fbrect fbppm fbrestore audiotone audioctl play\n"
        "  NAME|PATH [ARGS] runs ELF; complex shell syntax delegates to ush -c\n"
        "  runelf/exec are plain debug launchers; use ush for <, >, >>, 2>, and |\n"
        "line editing: arrows browse history, Tab completes commands/paths\n"
    );
}


static void command(struct bush *self, char *line) {
    char *rest;
    char original[256];

    line = trim(line);
    if (!*line) {
        return;
    }

    strncpy(original, line, sizeof(original));
    KLOG("shell", "command: %s", original);
    if (should_delegate_to_ush(original)) {
        delegate_to_ush(self, original);
        return;
    }
    if (shell_syntax(original)) {
        self->_status = 2;
        self->_tty->write(self->_tty, "bush: shell syntax belongs to userspace; use ush or external commands\n");
        return;
    }
    rest = arg(line);

    if (utility(line) && trybin(self, line, rest)) {
        return;
    }

    if (strcmp(line, "help") == 0) {
        help(self);
    } else if (strcmp(line, "clear") == 0 || strcmp(line, "cls") == 0) {
        self->_tty->clear(self->_tty);
    } else if (strcmp(line, "version") == 0) {
        self->_tty->printf(self->_tty, "%s kernel=%s version=%s arch=%s\n", MONARCH_NAME, MONARCH_KERNEL, MONARCH_VERSION, MONARCH_ARCH);
    } else if (strcmp(line, "status") == 0) {
        self->_tty->printf(self->_tty, "%d\n", self->_status);
    } else if (strcmp(line, "debug") == 0) {
        debugcmd(self, rest);
    } else if (strcmp(line, "ticks") == 0) {
        self->_tty->printf(self->_tty, "ticks=%u uptime=%u seconds\n", ticks(), uptime());
    } else if (strcmp(line, "mem") == 0) {
        self->_tty->printf(self->_tty, "heap=%s start=%p break=%p mapped=%p end=%p\n",
            kheappaged() ? "virtual" : "bootstrap",
            (void *)kheapstart(), (void *)kheapbreak(), (void *)kheapmapped(), (void *)kheapend());
        self->_tty->printf(self->_tty, "heap used=%u free=%u bytes\n", (unsigned)kused(), (unsigned)kfreebytes());
        self->_tty->printf(self->_tty, "phys limit=%p total=%u free=%u used=%u pages\n",
            (void *)pmmlimit(),
            (unsigned)pmmtotal(),
            (unsigned)pmmfreepages(),
            (unsigned)(pmmtotal() - pmmfreepages()));
    } else if (strcmp(line, "page") == 0) {
        uintptr_t page = pmmalloc();
        if (page) {
            self->_tty->printf(self->_tty, "allocated page %p, freeing it again\n", (void *)page);
            pmmfree(page);
        } else {
            self->_tty->write(self->_tty, "out of physical pages\n");
        }
    } else if (strcmp(line, "paging") == 0) {
        self->_tty->printf(self->_tty, "paging=%s cr3=%p identity(1M)=%p\n",
            pagingactive() ? "on" : "off",
            (void *)pagedirectory(),
            (void *)pageget(0x00100000u));
    } else if (strcmp(line, "maptest") == 0) {
        uintptr_t page = pmmalloc();
        uintptr_t virt = alignup(pmmlimit() + 0x400000u, 0x400000u);
        volatile uint32_t *test;
        if (virt < 0x40000000u) {
            virt = 0x40000000u;
        }
        test = (volatile uint32_t *)virt;
        if (!page) {
            self->_tty->write(self->_tty, "out of physical pages\n");
        } else if (virt >= 0xF0000000u || pageget(virt)) {
            pmmfree(page);
            self->_tty->write(self->_tty, "no safe scratch virtual address\n");
        } else if (!pagemap((uintptr_t)test, page, PAGE_WRITE)) {
            pmmfree(page);
            self->_tty->write(self->_tty, "map failed\n");
        } else {
            *test = 0xC0DEFACEu;
            self->_tty->printf(self->_tty, "virt=%p phys=%p value=%x\n", (void *)test, (void *)pageget((uintptr_t)test), (unsigned)*test);
            pageunmap((uintptr_t)test);
            pmmfree(page);
        }
    } else if (strcmp(line, "fb") == 0) {
        struct framebuffer *fb = framebuffer_get();
        if (!fb) {
            self->_tty->write(self->_tty, "no linear framebuffer available\n");
        } else {
            self->_tty->printf(self->_tty, "fb addr=%p %ux%u pitch=%u bpp=%u type=%u\n",
                (void *)fb->physical, fb->width, fb->height, fb->pitch, fb->bpp, fb->type);
        }
    } else if (strcmp(line, "gfx") == 0) {
        gfxdemo(self);
    } else if (strcmp(line, "image") == 0) {
        imagedemo(self);
    } else if (strcmp(line, "bmp") == 0) {
        bmpdemo(self);
    } else if (strcmp(line, "mbi") == 0) {
        mbidemo(self);
    } else if (strcmp(line, "sprite") == 0) {
        sprite_demo(self);
    } else if (strcmp(line, "alpha") == 0) {
        alpha_demo(self);
    } else if (strcmp(line, "clip") == 0) {
        clip_demo(self);
    } else if (strcmp(line, "boing") == 0) {
        boingdemo(self);
    } else if (strcmp(line, "mouse") == 0) {
        struct mouse *m = mouseget();
        if (m && m->ready(m)) {
            self->_tty->printf(self->_tty, "mouse id=%u x=%d y=%d z=%d buttons=%u packets=%u errors=%u\n",
                m->id(m), m->x(m), m->y(m), m->z(m), m->buttons(m), m->packets(m), m->errors(m));
        } else {
            self->_tty->write(self->_tty, "mouse unavailable\n");
        }
    } else if (strcmp(line, "beep") == 0) {
        self->_speaker->beep(self->_speaker, 880, 80);
    } else if (strcmp(line, "ac97") == 0) {
        uint32_t frequency = 440;
        uint32_t duration = 250;
        char *duration_text = rest ? arg(rest) : nil;
        if (rest && *rest) {
            frequency = (uint32_t)atoi(rest);
        }
        if (duration_text && *duration_text) {
            duration = (uint32_t)atoi(duration_text);
        }
        if (!self->_ac97 || !self->_ac97->present) {
            self->_status = 1;
            KLOG("sound", "ac97 tone unavailable");
            self->_tty->write(self->_tty, "ac97 unavailable\n");
        } else if (!self->_ac97->tone(self->_ac97, frequency, duration)) {
            self->_status = 1;
            KLOG("sound", "ac97 tone failed hz=%u ms=%u", frequency, duration);
            self->_tty->write(self->_tty, "ac97 tone failed\n");
        } else {
            self->_status = 0;
            KLOG("sound", "ac97 tone ok hz=%u ms=%u", frequency, duration);
            self->_tty->printf(self->_tty, "ac97 tone %u Hz %u ms\n", frequency, duration);
        }
    } else if (strcmp(line, "floppy") == 0) {
        int recalibrated = self->_floppy && floppy_recal(self->_floppy);
        int seeked = recalibrated && floppy_seek(self->_floppy, 0);
        self->_status = recalibrated && seeked ? 0 : 1;
        KLOG("block", "floppy positioning recalibrate=%d seek=%d irq=%u st0=%x cylinder=%u",
            recalibrated, seeked,
            self->_floppy ? self->_floppy->last_irq : 0,
            self->_floppy ? self->_floppy->last_status : 0,
            self->_floppy ? self->_floppy->last_cylinder : 0xFFu);
        self->_tty->printf(self->_tty, "floppy recalibrate=%s seek=%s irq=%u st0=%x cylinder=%u\n",
            recalibrated ? "ok" : "failed", seeked ? "ok" : "failed",
            self->_floppy ? self->_floppy->last_irq : 0,
            self->_floppy ? self->_floppy->last_status : 0,
            self->_floppy ? self->_floppy->last_cylinder : 0xFFu);
    } else if (strcmp(line, "floppyformat") == 0) {
        int ok = self->_floppy && floppy_format(self->_floppy, 0, 0);
        self->_status = ok ? 0 : 1;
        KLOG("block", "floppy format cylinder=0 head=0 ok=%d st0=%x st1=%x st2=%x", ok,
            self->_floppy ? self->_floppy->last_status : 0,
            self->_floppy ? self->_floppy->last_st1 : 0,
            self->_floppy ? self->_floppy->last_st2 : 0);
        self->_tty->printf(self->_tty, "floppy format %s\n", ok ? "ok" : "failed");
    } else if (strcmp(line, "floppywrite") == 0) {
        uint8_t sector[BLOCK_SECTOR];
        int ok = self->_floppy && floppy_read(self->_floppy, 0, sector) && floppy_write(self->_floppy, 0, sector);
        self->_status = ok ? 0 : 1;
        KLOG("block", "floppy write lba=0 ok=%d st0=%x st1=%x st2=%x", ok,
            self->_floppy ? self->_floppy->last_status : 0,
            self->_floppy ? self->_floppy->last_st1 : 0,
            self->_floppy ? self->_floppy->last_st2 : 0);
        self->_tty->printf(self->_tty, "floppy write %s\n", ok ? "ok" : "failed");
    } else if (strcmp(line, "floppyid") == 0) {
        int ok = self->_floppy && floppy_id(self->_floppy, 0);
        self->_status = ok ? 0 : 1;
        KLOG("block", "floppy read-id ok=%d st0=%x st1=%x st2=%x chs=%u/%u/%u/%u", ok,
            self->_floppy ? self->_floppy->last_status : 0,
            self->_floppy ? self->_floppy->last_st1 : 0,
            self->_floppy ? self->_floppy->last_st2 : 0,
            self->_floppy ? self->_floppy->last_cylinder : 0,
            self->_floppy ? self->_floppy->last_head : 0,
            self->_floppy ? self->_floppy->last_sector : 0,
            self->_floppy ? self->_floppy->last_size : 0);
        self->_tty->printf(self->_tty, "floppy id %s chs=%u/%u/%u/%u\n", ok ? "ok" : "failed",
            self->_floppy ? self->_floppy->last_cylinder : 0,
            self->_floppy ? self->_floppy->last_head : 0,
            self->_floppy ? self->_floppy->last_sector : 0,
            self->_floppy ? self->_floppy->last_size : 0);
    } else if (strcmp(line, "floppyread") == 0) {
        uint8_t sector[BLOCK_SECTOR];
        uint32_t lba = rest && *rest ? (uint32_t)atoi(rest) : 0u;
        int ok = self->_floppy && floppy_read(self->_floppy, lba, sector);
        self->_status = ok ? 0 : 1;
        KLOG("block", "floppy read lba=%u ok=%d irq=%u st0=%x st1=%x st2=%x chs=%u/%u/%u/%u signature=%02x%02x", lba, ok,
            self->_floppy ? self->_floppy->last_irq : 0,
            self->_floppy ? self->_floppy->last_status : 0,
            self->_floppy ? self->_floppy->last_st1 : 0,
            self->_floppy ? self->_floppy->last_st2 : 0,
            self->_floppy ? self->_floppy->last_cylinder : 0,
            self->_floppy ? self->_floppy->last_head : 0,
            self->_floppy ? self->_floppy->last_sector : 0,
            self->_floppy ? self->_floppy->last_size : 0,
            ok ? sector[510] : 0, ok ? sector[511] : 0);
        self->_tty->printf(self->_tty, "floppy read %s signature=%02x%02x\n", ok ? "ok" : "failed",
            ok ? sector[510] : 0, ok ? sector[511] : 0);
    } else if (strcmp(line, "mounts") == 0) {
        self->_fs->mounts(self->_fs, emit, self->_tty);
    } else if (strcmp(line, "initrd") == 0) {
        struct vdir *dir = self->_fs->opendir(self->_fs, "/initrd");
        struct ventry entry;
        if (!dir) {
            self->_tty->write(self->_tty, "initrd not loaded\n");
        } else {
            while (dir->read(dir, &entry)) {
                self->_tty->printf(self->_tty, "%c %-28s %u\n",
                    entry.type == VFS_DIR ? 'd' : '-', entry.name, (unsigned)entry.size);
            }
            dir->close(dir);
        }
    } else if (strcmp(line, "ram0") == 0) {
        const char *info = self->_fs->read(self->_fs, "/dev/ram0");
        self->_tty->write(self->_tty, info ? info : "ram0 unavailable\n");
    } else if (strcmp(line, "fds") == 0) {
        for (int fd = 0; fd < 32; fd++) {
            if (fdinfo_used(fd)) {
                const char *path = fdinfo_path(fd);
                self->_tty->printf(self->_tty, "%d %s flags=%x fdflags=%x refs=%u -> %s\n",
                    fd,
                    fdinfo_is_dir(fd) ? "dir " : "file",
                    fdinfo_flags(fd),
                    fdinfo_fdflags(fd),
                    fdinfo_refs(fd),
                    path ? path : "");
            }
        }
    } else if (strcmp(line, "proc") == 0) {
        struct process *proc = processcurrent();
        if (proc) {
            self->_tty->printf(self->_tty, "pid=%u ppid=%u state=%s exit=%d cr3=%p stack=%p image=%p..%p cwd=%s name=%s\n",
                proc->pid,
                proc->ppid,
                processstate(proc->state),
                proc->exit_code,
                (void *)proc->space,
                (void *)proc->stack_pointer,
                (void *)proc->image_start,
                (void *)proc->image_end,
                processcwd(proc),
                proc->name);
        }
    } else if (strcmp(line, "ps") == 0) {
        self->_tty->write(self->_tty, "PID STATE    PPID EXIT CR3      IMAGE        CWD NAME\n");
        processeach(psline, self->_tty);
    } else if (strcmp(line, "spawn") == 0) {
        struct process *proc = rest ? processspawn(rest) : nil;
        struct thread *thread = nil;
        if (proc) {
            processinherit(proc, processcurrent(), self->_fs);
            thread = threadspawnfor(proc, proc->name, worker, nil);
        }
        if (proc && thread) {
            self->_tty->printf(self->_tty, "spawned pid=%u tid=%u name=%s\n", proc->pid, thread->tid, proc->name);
        } else {
            if (proc) {
                processsetstate(proc, PROCESS_ZOMBIE);
            }
            self->_tty->write(self->_tty, "spawn failed\n");
        }
    } else if (strcmp(line, "threads") == 0) {
        self->_tty->write(self->_tty, "TID STATE    PID NAME\n");
        threadeach(threadline, self->_tty);
    } else if (strcmp(line, "kthread") == 0) {
        struct thread *thread = rest ? threadspawn(rest, worker, nil) : nil;
        if (thread) {
            self->_tty->printf(self->_tty, "thread %u ready (%s)\n", thread->tid, thread->name);
        } else {
            self->_tty->write(self->_tty, "kthread failed\n");
        }
    } else if (strcmp(line, "yield") == 0) {
        yield();
    } else if (strcmp(line, "nap") == 0) {
        uint32_t ms = rest ? (uint32_t)atoi(rest) : 250u;
        self->_tty->printf(self->_tty, "sleeping %u ms\n", ms);
        threadsleep(ms);
        self->_tty->write(self->_tty, "awake\n");
    } else if (strcmp(line, "sched") == 0) {
        self->_tty->printf(self->_tty, "soft-preempt=%s quantum=%u ticks pending=%s\n",
            schedpreempting() ? "on" : "off",
            schedquantumticks(),
            schedpending() ? "yes" : "no");
    } else if (strcmp(line, "preempt") == 0) {
        if (rest && strcmp(rest, "on") == 0) {
            schedpreempt(1);
        } else if (rest && strcmp(rest, "off") == 0) {
            schedpreempt(0);
        } else {
            self->_tty->write(self->_tty, "usage: preempt on|off\n");
        }
    } else if (strcmp(line, "quantum") == 0) {
        uint32_t value = rest ? (uint32_t)atoi(rest) : 0;
        if (value) {
            schedquantum(value);
        } else {
            self->_tty->write(self->_tty, "usage: quantum TICKS\n");
        }
    } else if (strcmp(line, "int80") == 0) {
        const char *message = "hello from int 0x80 write syscall\n";
        long fd = sysint(SYS_OPEN, (uintptr_t)"/dev/console", OWRITE, 0, 0);
        long result = fd >= 0 ? sysint(SYS_WRITE, (uintptr_t)fd, (uintptr_t)message, strlen(message), 0) : -1;
        if (fd >= 3) {
            sysint(SYS_CLOSE, (uintptr_t)fd, 0, 0, 0);
        }
        self->_tty->printf(self->_tty, "int80 fd=%d returned %d\n", (int)fd, (int)result);
    } else if (strcmp(line, "elf") == 0) {
        char *buffer;
        size_t size;
        struct elfimage image;
        if (!rest) {
            self->_tty->write(self->_tty, "usage: elf PATH\n");
        } else if (!(buffer = readall(rest, &size))) {
            self->_tty->write(self->_tty, "elf: cannot read file\n");
        } else if (!elfinfo(buffer, size, &image)) {
            self->_tty->printf(self->_tty, "elf: %s\n", elferror());
            kfree(buffer);
        } else {
            self->_tty->printf(self->_tty, "elf entry=%p range=%p..%p phnum=%u\n",
                (void *)image.entry, (void *)image.start, (void *)image.end, image.phnum);
            kfree(buffer);
        }
    } else if (strcmp(line, "runelf") == 0) {
        runprogram(self, rest, 1, 1);
    } else if (strcmp(line, "exec") == 0) {
        runprogram(self, rest, 0, 1);
    } else if (strcmp(line, "wait") == 0) {
        int status = 0;
        int pid = rest ? atoi(rest) : -1;
        long waited = processwait(pid, &status);
        if (waited < 0) {
            self->_status = 1;
            self->_tty->write(self->_tty, "wait failed\n");
        } else {
            self->_status = status;
            self->_tty->printf(self->_tty, "pid=%d exit=%d\n", (int)waited, status);
        }
    } else if (strcmp(line, "devwrite") == 0) {
        char *text;
        long fd;
        if (!rest || !(text = arg(rest))) {
            self->_tty->write(self->_tty, "usage: devwrite DEV TEXT\n");
        } else if ((fd = sysopen(rest, OWRITE)) < 0) {
            self->_tty->write(self->_tty, "devwrite failed\n");
        } else {
            long count = syswrite((int)fd, text, strlen(text));
            sysclose((int)fd);
            self->_tty->printf(self->_tty, "%s\n", count >= 0 ? "ok" : "devwrite failed");
        }
    } else if (strcmp(line, "devread") == 0) {
        char buffer[32];
        long fd = rest ? sysopen(rest, OREAD) : -1;
        if (fd < 0) {
            self->_tty->write(self->_tty, "devread failed\n");
        } else {
            long count = sysread((int)fd, buffer, sizeof(buffer));
            sysclose((int)fd);
            printbytes(self, buffer, count);
        }
    } else if (strcmp(line, "cd") == 0) {
        self->_tty->printf(self->_tty, "%s\n", rest && syschdir(rest) == 0 ? "ok" : "cd failed");
    } else if (strcmp(line, "reboot") == 0) {
        reboot();
    } else if (strcmp(line, "shutdown") == 0) {
        shutdown();
    } else if (strcmp(line, "halt") == 0) {
        halt();
    } else if (!trybin(self, line, rest)) {
        self->_status = 127;
        self->_tty->printf(self->_tty, "%s: command not found\n", line);
    }
}

static void prompt(struct bush *self) {
    char pwd[256];

    self->_fs->cwd(self->_fs, pwd, sizeof(pwd));
    if (pwd[0] == '/') {
        self->_tty->printf(self->_tty, "\033]12;red\a\033[91m/\033[31m%s\033[91m @ \033[0m", pwd + 1);
    } else {
        self->_tty->printf(self->_tty, "\033]12;red\a\033[31m%s\033[91m @ \033[0m", pwd);
    }
}

static void prompt_edit(void *ctx) {
    prompt(ctx);
}

static void run(struct bush *self) {

    const char *banner = "Welcome to Monarch!";
    const char *helptext = "Type 'help' for a list of commands.\n";

    struct framebuffer *fb = framebuffer_get();
    struct renderer r;
    struct surface *img;
    struct surface *scaled;
    uint32_t x;
    uint32_t y;

    if (!fb) {
        self->_tty->write(self->_tty, "no linear framebuffer available\n");
        return;
    }

    img = loadbmpfile(self, "/initrd/share/butter.bmp");
    if (img) {
        self->_tty->_console->move(self->_tty->_console, 0, 0);
        render_framebuffer(&r, fb);
        r.clear(&r, 0x000000);

        scaled = surface_scale(img, 160, 160);

        x = 16;
        y = 16;
        r.blit(&r, scaled, x, y);
        surface_destroy(scaled);
    }
    surface_destroy(img);

    char line[BUSH_LINE_MAX];
    struct tty_editops ops;

    ops.history = history_pick;
    ops.complete = complete_line;
    ops.prompt = prompt_edit;
    ops.ctx = self;

    self->_tty->_console->move(self->_tty->_console, 12, 1);
    self->_tty->_console->color(self->_tty->_console, VGA_YELLOW, VGA_BLACK);
    self->_tty->_console->write(self->_tty->_console, banner);

    self->_tty->_console->move(self->_tty->_console, 13, 1);
    self->_tty->_console->color(self->_tty->_console, VGA_BROWN, VGA_BLACK);
    self->_tty->_console->write(self->_tty->_console, helptext);

    self->_tty->_console->color(self->_tty->_console, VGA_LIGHT_GREY, VGA_BLACK);
    self->_tty->_console->move(self->_tty->_console, 15, 0);

    forever {
        prompt(self);
        self->_history_view = -1;
        self->_tty->edit(self->_tty, line, sizeof(line), &ops);
        history_add(self, line);
        command(self, line);
    }
}


void bush(struct bush *self, struct tty *term, struct vfs *fs, struct speaker *pc, struct ac97 *audio, struct floppy_controller *floppy) {
    memset(self, 0, sizeof(*self));
    self->run = run;
    self->_tty = term;
    self->_fs = fs;
    self->_speaker = pc;
    self->_ac97 = audio;
    self->_floppy = floppy;
}
