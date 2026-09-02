/**
 * @file button.c
 * @brief Wing bevel button widget.
 */

#include "button.h"
#include "assets.h"
#include "label.h"

static int hit(struct Button *, int32_t, int32_t);
static void paint(struct Button *, struct renderer *);

static void draw_rect(struct renderer *renderer, int32_t x, int32_t y,
                      uint32_t w, uint32_t h, uint32_t color) {
    int32_t x1 = x + (int32_t)w;
    int32_t y1 = y + (int32_t)h;
    if (!renderer || !renderer->target || !w || !h) return;
    if (x < 0) { w = x1 > 0 ? (uint32_t)x1 : 0; x = 0; }
    if (y < 0) { h = y1 > 0 ? (uint32_t)y1 : 0; y = 0; }
    if (x >= (int32_t)renderer->target->width || y >= (int32_t)renderer->target->height) return;
    if (x + (int32_t)w > (int32_t)renderer->target->width) w = renderer->target->width - (uint32_t)x;
    if (y + (int32_t)h > (int32_t)renderer->target->height) h = renderer->target->height - (uint32_t)y;
    if (w && h) renderer->rect(renderer, (uint32_t)x, (uint32_t)y, w, h, color);
}

void Button_init(struct Button *self, const uint8_t *glyph) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->hit = hit;
    self->paint = paint;
    self->glyph = glyph;
}
static int hit(struct Button *button, int32_t x, int32_t y) {
    return button && !button->disabled && x >= button->frame.x && y >= button->frame.y &&
        x < button->frame.x + (int32_t)button->frame.w &&
        y < button->frame.y + (int32_t)button->frame.h;
}
static void paint(struct Button *button, struct renderer *renderer) {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;

    if (!button || !renderer) return;
    unused(wing_corner_mask);
    x = button->frame.x;
    y = button->frame.y;
    w = button->frame.w;
    h = button->frame.h;
    draw_rect(renderer, x, y, w, h, button->disabled ? WING_SHADOW : WING_FACE);
    draw_rect(renderer, x, y, w, 2, button->disabled ? WING_SHADOW : (button->pressed ? WING_SHADOW : WING_HILITE));
    draw_rect(renderer, x, y, 2, h, button->disabled ? WING_SHADOW : (button->pressed ? WING_SHADOW : WING_HILITE));
    draw_rect(renderer, x, y + (int32_t)h - 2, w, 2, button->disabled ? WING_DARK : (button->pressed ? WING_HILITE : WING_DARK));
    draw_rect(renderer, x + (int32_t)w - 2, y, 2, h, button->disabled ? WING_DARK : (button->pressed ? WING_HILITE : WING_DARK));
    if (button->glyph && x >= 0 && y >= 0) {
        uint32_t gx = (uint32_t)x + (w - WING_ASSET_W) / 2u + (button->pressed ? 1u : 0u);
        uint32_t gy = (uint32_t)y + (h - WING_ASSET_H) / 2u + (button->pressed ? 1u : 0u);
        for (uint32_t row = 0; row < WING_ASSET_H; row++) {
            for (uint32_t col = 0; col < WING_ASSET_W; col++) {
                if (button->glyph[row * WING_ASSET_W + col]) renderer->pixel(renderer, gx + col, gy + row, WING_DARK);
            }
        }
    } else if (button->label && x + 8 >= 0 && y + 7 >= 0) {
        renderer->text(renderer, button->label, (uint32_t)(x + 8), (uint32_t)(y + 7), WING_DARK, WING_FACE);
    }
}
