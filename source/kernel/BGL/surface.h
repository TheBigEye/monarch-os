#ifndef _KERNEL_GRAPHICS_SURFACE_H
#define _KERNEL_GRAPHICS_SURFACE_H 1

#include "../../common/common.h"

// Surface blend modes (like SDL)
typedef enum {
    BGL_BLEND_NONE,     // No blending
    BGL_BLEND_ALPHA,    // Alpha blending (simulated with color matching)
    BGL_BLEND_ADD,      // Additive blending
    BGL_BLEND_MOD       // Modulate blending
} BlendMode;


// Surface flags
typedef enum {
    BGL_SURFACE_SPRITE = 1,      // Surface is a sprite (part of another surface)
    BGL_SURFACE_OWNED = 2,       // Surface owns its pixel data
    BGL_SURFACE_COLORKEY = 4     // Surface uses color key transparency
} SurfaceFlags;


// Surface structure (similar to SDL_Surface)
typedef struct Surface {
    uint8_t* pixels;              // Pixel data
    BlendMode blend_mode;         // Current blend mode
    SurfaceFlags flags;           // Surface flags
    struct Surface* parent;       // Parent surface for sprites

    uint16_t w, h;                // Width and height
    uint16_t pitch;               // Bytes per row

    Rect clip_rect;               // Clipping rectangle

    uint8_t bpp;                  // Bits per pixel (always 4 in our case)
    uint8_t colorkey;             // Transparent color key

} PADDED Surface;


// Create / destroy surfaces
Surface* bglCreateSurface(uint16_t width, uint16_t height);
Surface* bglCreateSurfaceFrom(uint8_t* pixels, uint16_t width, uint16_t height);
Surface* bglCreateSprite(Surface* parent, Rect rect);
void bglDestroySurface(Surface* surface);

// Surface operations
void bglFillSurface(Surface* surface, uint8_t color);
void bglBlit(Surface* src, Rect* srcrect, Surface* dst, Rect* dstrect);
void bglBlitToScreen(Surface* surface, Rect* srcrect, uint16_t x, uint16_t y);

// Surface properties
void bglSetColorKey(Surface* surface, uint8_t color);
void bglSetBlendMode(Surface* surface, BlendMode mode);
void bglSetClipRect(Surface* surface, Rect* rect);

#endif /* _KERNEL_GRAPHICS_SURFACE_H */
