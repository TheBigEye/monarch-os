/**
 * @file sprite.c
 * @brief Sprite object implementation. A sprite wraps a surface and optional draw state such as source rectangle, color key transparency, alpha, and ownership.
 */

#include "base/gfx/sprite.h"

static const struct rect *src(struct sprite *self) {
    return self->_has_source ? &self->_source : nil;
}

static void draw_impl(struct sprite *self, struct renderer *renderer, uint32_t x, uint32_t y) {
    if (!self || !renderer || !self->_surface) {
        return;
    }

    if (self->_alpha != 255) {
        renderer->blit_alpha_rect(renderer, self->_surface, src(self), x, y, self->_alpha);
    } else if (self->_keyed) {
        renderer->blit_key_rect(renderer, self->_surface, src(self), x, y, self->_key);
    } else {
        renderer->blit_rect(renderer, self->_surface, src(self), x, y);
    }
}

static void scale_impl(struct sprite *self, struct renderer *renderer, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!self || !renderer || !self->_surface) {
        return;
    }

    if (self->_keyed) {
        renderer->scale_key_rect(renderer, self->_surface, src(self), x, y, w, h, self->_key);
    } else {
        renderer->scale_rect(renderer, self->_surface, src(self), x, y, w, h);
    }
}

static void source_impl(struct sprite *self, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    if (!self) {
        return;
    }
    self->_source.x = x;
    self->_source.y = y;
    self->_source.w = w;
    self->_source.h = h;
    self->_has_source = 1;
}

static void full_impl(struct sprite *self) {
    if (self) {
        self->_has_source = 0;
    }
}

static void key_impl(struct sprite *self, uint32_t color, int enabled) {
    if (!self) {
        return;
    }
    self->_key = color;
    self->_keyed = enabled ? 1 : 0;
}

static void alpha_impl(struct sprite *self, uint8_t alpha) {
    if (self) {
        self->_alpha = alpha;
    }
}

static void destroy_impl(struct sprite *self) {
    if (!self) {
        return;
    }
    if (self->_owned && self->_surface) {
        surface_destroy(self->_surface);
    }
    gfx_free(self);
}

void sprite_init(struct sprite *self, struct surface *surface) {
    memset(self, 0, sizeof(*self));
    self->draw = draw_impl;
    self->scale = scale_impl;
    self->source = source_impl;
    self->full = full_impl;
    self->key = key_impl;
    self->alpha = alpha_impl;
    self->destroy = destroy_impl;
    self->_surface = surface;
    self->_alpha = 255;
}

struct sprite *sprite_create(struct surface *surface, int take_ownership) {
    struct sprite *self = gfx_alloc(sizeof(*self));
    if (!self) {
        return nil;
    }
    sprite_init(self, surface);
    self->_owned = take_ownership ? 1 : 0;
    return self;
}
