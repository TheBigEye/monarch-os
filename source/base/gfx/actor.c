/**
 * @file actor.c
 * @brief Small BGL actor helper. Actors combine position, velocity, size, and a sprite, which makes simple demos easier to read than manual x/y/vx/vy code everywhere.
 */

#include "base/gfx/actor.h"

void actor_init(struct actor *self, struct sprite *sprite, int x, int y, int vx, int vy, uint32_t w, uint32_t h) {
    self->x = x;
    self->y = y;
    self->vx = vx;
    self->vy = vy;
    self->w = w;
    self->h = h;
    self->sprite = sprite;
}

struct rect actor_rect(struct actor *self) {
    return rect_make(self->x, self->y, self->w, self->h);
}

void actor_step(struct actor *self, int bounds_w, int bounds_h) {
    self->x += self->vx;
    self->y += self->vy;

    if (self->x < 0) {
        self->x = 0;
        self->vx = -self->vx;
    }
    if (self->y < 0) {
        self->y = 0;
        self->vy = -self->vy;
    }
    if (self->x + (int)self->w >= bounds_w) {
        self->x = bounds_w - (int)self->w - 1;
        self->vx = -self->vx;
    }
    if (self->y + (int)self->h >= bounds_h) {
        self->y = bounds_h - (int)self->h - 1;
        self->vy = -self->vy;
    }
}

void actor_dirty(struct dirty *dirty, struct actor *self, int bounds_w, int bounds_h) {
    dirty_add(dirty, rect_clip(actor_rect(self), (uint32_t)bounds_w, (uint32_t)bounds_h));
    actor_step(self, bounds_w, bounds_h);
    dirty_add(dirty, rect_clip(actor_rect(self), (uint32_t)bounds_w, (uint32_t)bounds_h));
}

void actor_draw(struct actor *self, struct renderer *renderer) {
    if (self && self->sprite) {
        self->sprite->draw(self->sprite, renderer, (uint32_t)self->x, (uint32_t)self->y);
    }
}
