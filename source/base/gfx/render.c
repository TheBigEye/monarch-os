/**
 * @file render.c
 * @brief High-level drawing methods for GFX surfaces.
 */

#include "base/gfx/render.h"
#include "base/gfx/font.h"
#include "base/gfx/text.h"

static uint32_t width(struct renderer *self) {
    return self->target->width;
}

static uint32_t height(struct renderer *self) {
    return self->target->height;
}

static int ready_impl(struct renderer *self) {
    return self && self->target && self->target->pixels && self->target->width && self->target->height;
}

static int inside(struct renderer *self, uint32_t x, uint32_t y) {
    if (!ready_impl(self) || x >= width(self) || y >= height(self)) {
        return 0;
    }

    if (self->clipped) {
        if ((int32_t)x < self->cliprect.x || (int32_t)y < self->cliprect.y) {
            return 0;
        }
        if (x >= (uint32_t)(self->cliprect.x + (int32_t)self->cliprect.w) ||
            y >= (uint32_t)(self->cliprect.y + (int32_t)self->cliprect.h)) {
            return 0;
        }
    }

    return 1;
}

static int native_32(struct surface *s) {
    return s && s->fast_rgb32;
}

static uint32_t get_pixel(struct renderer *self, uint32_t x, uint32_t y) {
    if (!inside(self, x, y)) {
        return 0;
    }
    if (native_32(self->target)) {
        uint8_t *p = self->target->pixels + (size_t)y * self->target->pitch + x * 4u;
        return *(uint32_t *)p & 0x00FFFFFFu;
    }
    return surface_get(self->target, x, y);
}

static void put_pixel(struct renderer *self, uint32_t x, uint32_t y, uint32_t rgb) {
    if (!inside(self, x, y)) {
        return;
    }
    if (native_32(self->target)) {
        uint8_t *p = self->target->pixels + (size_t)y * self->target->pitch + x * 4u;
        *(uint32_t *)p = rgb & 0x00FFFFFFu;
        return;
    }
    surface_set(self->target, x, y, rgb);
}

static struct rect full(struct surface *surface, const struct rect *src) {
    struct rect r;

    if (src) {
        r = *src;
    } else {
        r.x = 0;
        r.y = 0;
        r.w = surface ? surface->width : 0;
        r.h = surface ? surface->height : 0;
    }

    if (!surface) {
        r.w = 0;
        r.h = 0;
        return r;
    }

    if (r.x < 0) {
        uint32_t cut = (uint32_t)(-r.x);
        r.x = 0;
        r.w = r.w > cut ? r.w - cut : 0;
    }
    if (r.y < 0) {
        uint32_t cut = (uint32_t)(-r.y);
        r.y = 0;
        r.h = r.h > cut ? r.h - cut : 0;
    }
    if ((uint32_t)r.x >= surface->width || (uint32_t)r.y >= surface->height) {
        r.w = 0;
        r.h = 0;
        return r;
    }
    if ((uint32_t)r.x + r.w > surface->width) {
        r.w = surface->width - (uint32_t)r.x;
    }
    if ((uint32_t)r.y + r.h > surface->height) {
        r.h = surface->height - (uint32_t)r.y;
    }

    return r;
}

static void clear_impl(struct renderer *self, uint32_t rgb) {
    if (ready_impl(self)) {
        surface_fill(self->target, rgb);
    }
}

static void pixel_impl(struct renderer *self, uint32_t x, uint32_t y, uint32_t rgb) {
    put_pixel(self, x, y, rgb);
}

static void rect_impl(struct renderer *self, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb) {
    uint32_t x0 = x;
    uint32_t y0 = y;
    uint32_t x1 = x + w;
    uint32_t y1 = y + h;

    if (!ready_impl(self) || !w || !h) return;
    if (x0 >= width(self) || y0 >= height(self)) return;
    if (x1 > width(self)) x1 = width(self);
    if (y1 > height(self)) y1 = height(self);
    if (self->clipped) {
        int32_t cx0 = self->cliprect.x;
        int32_t cy0 = self->cliprect.y;
        int32_t cx1 = cx0 + (int32_t)self->cliprect.w;
        int32_t cy1 = cy0 + (int32_t)self->cliprect.h;
        if (cx0 > 0 && x0 < (uint32_t)cx0) x0 = (uint32_t)cx0;
        if (cy0 > 0 && y0 < (uint32_t)cy0) y0 = (uint32_t)cy0;
        if (cx1 < (int32_t)x1) x1 = (uint32_t)cx1;
        if (cy1 < (int32_t)y1) y1 = (uint32_t)cy1;
    }
    if (x1 <= x0 || y1 <= y0) return;
    for (uint32_t py = y0; py < y1; py++) {
        for (uint32_t px = x0; px < x1; px++) {
            put_pixel(self, px, py, rgb);
        }
    }
}

static void line_impl(struct renderer *self, int x0, int y0, int x1, int y1, uint32_t rgb) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (x0 >= 0 && y0 >= 0) {
            put_pixel(self, (uint32_t)x0, (uint32_t)y0, rgb);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = err + err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void circle_impl(struct renderer *self, int cx, int cy, uint32_t radius, uint32_t rgb) {
    int r = (int)radius;
    int rr = r * r;

    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= rr && cx + x >= 0 && cy + y >= 0) {
                put_pixel(self, (uint32_t)(cx + x), (uint32_t)(cy + y), rgb);
            }
        }
    }
}

static int edge(int ax, int ay, int bx, int by, int px, int py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void triangle_impl(struct renderer *self, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t rgb) {
    int minx = min(x0, min(x1, x2));
    int maxx = max(x0, max(x1, x2));
    int miny = min(y0, min(y1, y2));
    int maxy = max(y0, max(y1, y2));
    int area = edge(x0, y0, x1, y1, x2, y2);

    if (area == 0) {
        return;
    }

    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int w0 = edge(x1, y1, x2, y2, x, y);
            int w1 = edge(x2, y2, x0, y0, x, y);
            int w2 = edge(x0, y0, x1, y1, x, y);
            if (((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) && x >= 0 && y >= 0) {
                put_pixel(self, (uint32_t)x, (uint32_t)y, rgb);
            }
        }
    }
}

static void blit_mode(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t key, int keyed, int alpha_on, uint8_t alpha) {
    struct rect r = full(surface, src);

    if (!ready_impl(self) || !surface || !r.w || !r.h) {
        return;
    }

    {
        uint32_t sx = 0;
        uint32_t sy = 0;
        uint32_t w = r.w;
        uint32_t h = r.h;
        int32_t left = (int32_t)x;
        int32_t top = (int32_t)y;
        int32_t right = left + (int32_t)w;
        int32_t bottom = top + (int32_t)h;

        /* A clipped blit must clip the source rectangle too. The old path
           tested every pixel in the complete cached window and discarded most
           of them in inside(), which made cursor updates scale with window
           size. */
        if (left < 0) { sx = (uint32_t)-left; left = 0; }
        if (top < 0) { sy = (uint32_t)-top; top = 0; }
        if (self->clipped) {
            int32_t clip_right = self->cliprect.x + (int32_t)self->cliprect.w;
            int32_t clip_bottom = self->cliprect.y + (int32_t)self->cliprect.h;
            if (left < self->cliprect.x) { sx += (uint32_t)(self->cliprect.x - left); left = self->cliprect.x; }
            if (top < self->cliprect.y) { sy += (uint32_t)(self->cliprect.y - top); top = self->cliprect.y; }
            if (right > clip_right) right = clip_right;
            if (bottom > clip_bottom) bottom = clip_bottom;
        }
        if (right > (int32_t)width(self)) right = (int32_t)width(self);
        if (bottom > (int32_t)height(self)) bottom = (int32_t)height(self);
        if (right <= left || bottom <= top || sx >= r.w || sy >= r.h) return;
        w = (uint32_t)(right - left);
        h = (uint32_t)(bottom - top);
        if (sx + w > r.w) w = r.w - sx;
        if (sy + h > r.h) h = r.h - sy;

        /* Alpha blending previously always fell through to the slow
           per-pixel loop below, even when formats matched and no key/clip
           work was needed. surface_blit_alpha has its own raw-pointer fast
           path for that case now, so route to it here too -- unless both
           a key and translucency were requested at once, which no caller
           in this codebase actually does, so the slow path below still
           covers it correctly. */
        if ((!alpha_on || !keyed) && surface_match(self->target, surface)) {
            struct rect source = {
                r.x + (int32_t)sx,
                r.y + (int32_t)sy,
                w,
                h
            };
            if (keyed) {
                surface_blit_key(self->target, surface, &source,
                                  (uint32_t)left, (uint32_t)top, key);
            } else if (alpha_on && alpha < 255) {
                surface_blit_alpha(self->target, surface, &source,
                                    (uint32_t)left, (uint32_t)top, alpha);
            } else {
                surface_copy(self->target, surface, &source,
                             (uint32_t)left, (uint32_t)top);
            }
            return;
        }

        for (uint32_t row = 0; row < h; row++) {
            for (uint32_t col = 0; col < w; col++) {
                uint32_t c = surface_get(surface, (uint32_t)r.x + sx + col,
                                         (uint32_t)r.y + sy + row);
                uint32_t dx = (uint32_t)left + col;
                uint32_t dy = (uint32_t)top + row;
                if (keyed && c == key) continue;
                if (alpha_on && alpha < 255)
                    c = pixel_blend(get_pixel(self, dx, dy), c, alpha);
                put_pixel(self, dx, dy, c);
            }
        }
    }
}

static void blit_impl(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y) {
    blit_mode(self, surface, nil, x, y, 0, 0, 0, 255);
}

static void blit_rect_impl(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y) {
    blit_mode(self, surface, src, x, y, 0, 0, 0, 255);
}

static void blit_key_impl(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint32_t key) {
    blit_mode(self, surface, nil, x, y, key, 1, 0, 255);
}

static void blit_key_rect_impl(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t key) {
    blit_mode(self, surface, src, x, y, key, 1, 0, 255);
}

static void blit_alpha_impl(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint8_t alpha) {
    blit_mode(self, surface, nil, x, y, 0, 0, 1, alpha);
}

static void blit_alpha_rect_impl(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint8_t alpha) {
    blit_mode(self, surface, src, x, y, 0, 0, 1, alpha);
}

static void scale_mode(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t key, int keyed) {
    struct rect r = full(surface, src);
    uint32_t step_x;
    uint32_t step_y;

    if (!ready_impl(self) || !surface || !r.w || !r.h || !w || !h) {
        return;
    }

    /* One division per axis instead of one per output pixel. */
    step_x = (r.w << 16) / w;
    step_y = (r.h << 16) / h;

    for (uint32_t row = 0, sy_fixed = 0; row < h; row++, sy_fixed += step_y) {
        uint32_t sy = (uint32_t)r.y + (sy_fixed >> 16);
        for (uint32_t col = 0, sx_fixed = 0; col < w; col++, sx_fixed += step_x) {
            uint32_t sx = (uint32_t)r.x + (sx_fixed >> 16);
            uint32_t c = surface_get(surface, sx, sy);
            uint32_t dx = x + col;
            uint32_t dy = y + row;

            if (!inside(self, dx, dy) || (keyed && c == key)) {
                continue;
            }
            put_pixel(self, dx, dy, c);
        }
    }
}

static void scale_impl(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    scale_mode(self, surface, nil, x, y, w, h, 0, 0);
}

static void scale_rect_impl(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    scale_mode(self, surface, src, x, y, w, h, 0, 0);
}

static void scale_key_impl(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t key) {
    scale_mode(self, surface, nil, x, y, w, h, key, 1);
}

static void scale_key_rect_impl(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t key) {
    scale_mode(self, surface, src, x, y, w, h, key, 1);
}

static void text_impl(struct renderer *self, const char *value, uint32_t ux, uint32_t uy, uint32_t fg, uint32_t bg) {
    struct font *fontptr;
    int32_t x;
    int32_t y;
    int32_t ox;

    if (!ready_impl(self) || !value) return;
    fontptr = font_default();
    x = (int32_t)ux;
    y = (int32_t)uy;
    ox = x;
    while (*value) {
        if (*value == '\n') {
            x = ox;
            y += fontptr->height;
        } else if (*value == '\r') {
            x = ox;
        } else if (*value == '\t') {
            x += fontptr->width * 4u;
        } else {
            const uint8_t *glyph = fontptr->glyphs + (uint32_t)(uint8_t)*value * fontptr->height;
            for (uint32_t row = 0; row < fontptr->height; row++) {
                for (uint32_t col = 0; col < fontptr->width; col++) {
                    int32_t px = x + (int32_t)col;
                    int32_t py = y + (int32_t)row;
                    if (glyph[row] & (0x80u >> col)) put_pixel(self, (uint32_t)px, (uint32_t)py, fg);
                    else put_pixel(self, (uint32_t)px, (uint32_t)py, bg);
                }
            }
            x += fontptr->width;
        }
        value++;
    }
}

static void clip_impl(struct renderer *self, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    if (!self) {
        return;
    }
    self->cliprect.x = x;
    self->cliprect.y = y;
    self->cliprect.w = w;
    self->cliprect.h = h;
    self->clipped = 1;
}

static void no_clip_impl(struct renderer *self) {
    if (self) {
        self->clipped = 0;
    }
}

static void init(struct renderer *r) {
    struct surface view;

    /* Adapters may store a non-owning device view inside the renderer.  Keep
       that view while resetting callbacks and per-target state. */
    view = r->_view;
    memset(r, 0, sizeof(*r));
    r->_view = view;
    r->clear = clear_impl;
    r->pixel = pixel_impl;
    r->rect = rect_impl;
    r->line = line_impl;
    r->circle = circle_impl;
    r->triangle = triangle_impl;
    r->blit = blit_impl;
    r->blit_rect = blit_rect_impl;
    r->blit_key = blit_key_impl;
    r->blit_key_rect = blit_key_rect_impl;
    r->blit_alpha = blit_alpha_impl;
    r->blit_alpha_rect = blit_alpha_rect_impl;
    r->scale = scale_impl;
    r->scale_rect = scale_rect_impl;
    r->scale_key = scale_key_impl;
    r->scale_key_rect = scale_key_rect_impl;
    r->text = text_impl;
    r->clip = clip_impl;
    r->no_clip = no_clip_impl;
    r->ready = ready_impl;
}

void render_target(struct renderer *r, struct surface *target) {
    init(r);
    r->target = target;
}

void render_surface(struct renderer *r, struct surface *target) {
    render_target(r, target);
}
