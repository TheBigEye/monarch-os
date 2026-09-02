#ifndef MONARCH_BASE_GFX_ACTOR_H
#define MONARCH_BASE_GFX_ACTOR_H 1

/**
 * @file actor.h
 * @brief Small helper for moving sprites in demos.
 *
 * An actor is not a full game object system.  It is a lightweight convenience
 * structure for demos: it stores position, velocity, size, and a sprite pointer.
 * The helper functions know how to move the actor, bounce it inside a rectangle,
 * mark its old/new dirty rectangles, and draw it.
 */

#include "base/gfx/dirty.h"
#include "base/gfx/sprite.h"

/** A moving sprite with rectangular bounds. */
struct actor {
    /** Current left coordinate. */
    int x;
    /** Current top coordinate. */
    int y;
    /** Horizontal velocity in pixels per step. */
    int vx;
    /** Vertical velocity in pixels per step. */
    int vy;
    /** Width used for bounds and dirty rectangles. */
    uint32_t w;
    /** Height used for bounds and dirty rectangles. */
    uint32_t h;
    /** Sprite drawn at this actor's position. */
    struct sprite *sprite;
};

/** Initialise an actor with position, velocity, dimensions, and sprite. */
void actor_init(struct actor *self, struct sprite *sprite, int x, int y, int vx, int vy, uint32_t w, uint32_t h);

/** Return this actor's current rectangle. */
struct rect actor_rect(struct actor *self);

/** Move the actor once and bounce it against `bounds_w`/`bounds_h`. */
void actor_step(struct actor *self, int bounds_w, int bounds_h);

/** Add old and new actor rectangles to a dirty list while stepping the actor. */
void actor_dirty(struct dirty *dirty, struct actor *self, int bounds_w, int bounds_h);

/** Draw the actor's sprite using a renderer. */
void actor_draw(struct actor *self, struct renderer *renderer);

#endif /* MONARCH_BASE_GFX_ACTOR_H */
