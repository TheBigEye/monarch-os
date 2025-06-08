#include "surface.h"

#include "../drivers/FBC/graphics.h"
#include "../memory/memory.h"
#include "../memory/heap.h"

// BGL - Bitmap Graphics API
// I like SDL so ... let's make a simple version of it
// Imagine make videogames using this ... LOL

// Helper function to clip rectangles
static bool clipRects(Rect* src, Rect* dst, Rect* clip) {
    int16_t dx = clip->x - dst->x;
    int16_t dy = clip->y - dst->y;

    if (dx > 0) {
        src->x += dx;
        src->w -= dx;
        dst->x = clip->x;
    }
    if (dy > 0) {
        src->y += dy;
        src->h -= dy;
        dst->y = clip->y;
    }

    if (dst->x + src->w > clip->x + clip->w) {
        src->w = clip->x + clip->w - dst->x;
    }
    if (dst->y + src->h > clip->y + clip->h) {
        src->h = clip->y + clip->h - dst->y;
    }

    return (src->w > 0 && src->h > 0);
}


/**
 * @brief Create a new empty surface.
 *
 * @param width Width of the surface in pixels.
 * @param height Height of the surface in pixels.
 *
 * @return Pointer to the created Surface structure, or NULL on failure.
 */
Surface* bglCreateSurface(uint16_t width, uint16_t height) {
    Surface* surface = (Surface*) memoryAllocateBlock(sizeof(Surface));
    if (!surface) return NULL;

    // Calculate pitch (bytes per row) - 4bpp means width/2 bytes
    uint16_t pitch = (width + 1) >> 1;

    surface->pixels = (uint8_t *) memoryAllocateBlock(pitch * height);
    if (!surface->pixels) {
        memoryFreeBlock(surface);
        return NULL;
    }

    surface->w = width;
    surface->h = height;
    surface->pitch = pitch;
    surface->bpp = 4;
    surface->blend_mode = BGL_BLEND_NONE;
    surface->flags = BGL_SURFACE_OWNED;
    surface->colorkey = 0;
    surface->parent = NULL;

    // Set default clip rect to full surface
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = width;
    surface->clip_rect.h = height;

    return surface;
}


/**
 * @brief Create a new surface from existing pixel data.
 *
 * @param pixels Pointer to the pixel data array.
 * @param width Width of the surface in pixels.
 * @param height Height of the surface in pixels.
 *
 * @return Pointer to the created Surface structure, or NULL on failure.
 */
Surface* bglCreateSurfaceFrom(uint8_t* pixels, uint16_t width, uint16_t height) {
    Surface* surface = (Surface*) bglCreateSurface(width, height);
    if (!surface) return NULL;

    // Copy pixel data
    fastFastMemoryCopy(surface->pixels, pixels, surface->pitch * height);
    return surface;
}


/**
 * @brief Create a sprite surface from an existing surface.
 *
 * @note Sprites are sub-surfaces of existing surfaces.
 *
 * @param parent Pointer to the parent surface.
 * @param rect Rectangle defining the area of the sprite.
 *
 * @return Pointer to the created sprite surface, or NULL on failure.
 */
Surface* bglCreateSprite(Surface* parent, Rect rect) {
    if (!parent) {
        return NULL;
    }

    Surface* sprite = (Surface*) memoryAllocateBlock(sizeof(Surface));
    if (!sprite) return NULL;

    // Copy properties from parent
    sprite->w = rect.w;
    sprite->h = rect.h;
    sprite->bpp = parent->bpp;
    sprite->blend_mode = parent->blend_mode;
    sprite->flags = BGL_SURFACE_SPRITE;
    sprite->colorkey = parent->colorkey;
    sprite->parent = parent;

    // Calculate correct offset and pitch for sprite
    uint16_t bytesPerRow = (rect.w + 1) >> 1;  // Width in bytes (4bpp)
    sprite->pitch = bytesPerRow;  // Use actual sprite width for pitch

    // Create a new pixel buffer for the sprite
    sprite->pixels = (uint8_t*) memoryAllocateBlock(sprite->pitch * rect.h);
    if (!sprite->pixels) {
        memoryFreeBlock(sprite);
        return NULL;
    }
    sprite->flags |= BGL_SURFACE_OWNED;  // Mark that we own this pixel buffer

    // Copy pixel data from parent surface
    uint8_t* srcRow = parent->pixels + ((rect.y * parent->pitch) + (rect.x >> 1));
    uint8_t* dstRow = sprite->pixels;

    for (uint16_t y = 0; y < rect.h; y++) {
        fastFastMemoryCopy(dstRow, srcRow, bytesPerRow);
        srcRow += parent->pitch;
        dstRow += sprite->pitch;
    }

    // Set clip rect to full sprite size
    sprite->clip_rect.x = 0;
    sprite->clip_rect.y = 0;
    sprite->clip_rect.w = rect.w;
    sprite->clip_rect.h = rect.h;

    return sprite;
}


/**
 * @brief Destroy a surface and free its resources.
 */
void bglDestroySurface(Surface* surface) {
    if (!surface) return;

    // Only free pixel data if we own it
    if ((surface->flags & BGL_SURFACE_OWNED) && surface->pixels) {
        memoryFreeBlock(surface->pixels);
    }

    memoryFreeBlock(surface);
}


/**
 * @brief Fill a surface with a solid color.
 *
 * @param surface Pointer to the surface to fill.
 * @param color Color value to fill the surface with.
 */
void bglFillSurface(Surface* surface, uint8_t color) {
    if (!surface || !surface->pixels) return;

    // Pack color into byte (4bpp)
    uint8_t packed = (color << 4) | color;
    fastFastMemorySet(surface->pixels, packed, surface->pitch * surface->h);
}


/**
 * @brief Blit (copy) a source surface to a destination surface with optional clipping.
 *
 * @param src Pointer to the source surface.
 * @param srcrect Pointer to the source rectangle (NULL for full source).
 * @param dst Pointer to the destination surface.
 * @param dstrect Pointer to the destination rectangle (NULL for full destination).
 */
void bglBlit(Surface* src, Rect* srcrect, Surface* dst, Rect* dstrect) {
    if (!src || !dst) return;

    Rect sr = srcrect ? *srcrect : (Rect) {0, 0, src->w, src->h};
    Rect dr = dstrect ? *dstrect : (Rect) {0, 0, sr.w, sr.h};

    // Clip against destination surface
    if (!clipRects(&sr, &dr, &dst->clip_rect)) return;

    // Calculate starting positions
    uint32_t srcOffset = (sr.y * src->pitch) + (sr.x >> 1);
    uint32_t dstOffset = (dr.y * dst->pitch) + (dr.x >> 1);

    uint8_t* srcPtr = src->pixels + srcOffset;
    uint8_t* dstPtr = dst->pixels + dstOffset;

    // Copy rows with blending
    for (uint16_t y = 0; y < sr.h; y++) {

        if (src->blend_mode == BGL_BLEND_NONE) {
            fastFastMemoryCopy(dstPtr, srcPtr, sr.w >> 1);

        } else {
            // Handle other blend modes
            for (uint16_t x = 0; x < sr.w; x++) {
                uint8_t src_val = srcPtr[x >> 1];
                uint8_t dst_val = dstPtr[x >> 1];

                uint8_t srcColor = (x & 1) ? (src_val & 0x0F) : (src_val >> 4);

                // FIX: Compare with the lower nibble of colorkey instead of full value
                if (src->flags & BGL_SURFACE_COLORKEY && srcColor == (src->colorkey >> 4)) continue;

                uint8_t dstColor = (x & 1) ? (dst_val & 0x0F) : (dst_val >> 4);

                uint8_t finalColor = srcColor;
                switch (src->blend_mode) {
                    case BGL_BLEND_ADD:
                        finalColor = (srcColor + dstColor) & 0x0F;
                        break;

                    case BGL_BLEND_MOD:
                        finalColor = ((srcColor * dstColor) >> 4) & 0x0F;
                        break;

                    default:
                        break;
                }

                if (x & 1) {
                    dstPtr[x >> 1] = (dst_val & 0xF0) | finalColor;
                } else {
                    dstPtr[x >> 1] = (dst_val & 0x0F) | (finalColor << 4);
                }
            }
        }

        srcPtr += src->pitch;
        dstPtr += dst->pitch;
    }
}


/**
 * @brief Draw the surface pixels to the hardware screen.
 *
 * @param surface Pointer to the surface to draw.
 * @param srcrect Pointer to the source rectangle (NULL for full source).
 * @param x X coordinate on the screen.
 * @param y Y coordinate on the screen.
 */
void bglBlitToScreen(Surface* surface, Rect* srcrect, uint16_t x, uint16_t y) {
    if (!surface) return;

    Rect sr = srcrect ? *srcrect : (Rect) {0, 0, surface->w, surface->h};

    // Draw directly to screen using existing function
    gfx_draw(surface->pixels, x, y, sr.w, sr.h);
}


/**
 * @brief Set the transparency color key for a surface.
 *
 * @param surface Pointer to the surface.
 * @param color Color value to set as the color key.
 */
void bglSetColorKey(Surface* surface, uint8_t color) {
    if (!surface) return;
    // Store the full color value - will use >> 4 when comparing
    surface->colorkey = color;
    surface->flags |= BGL_SURFACE_COLORKEY;
}


/**
 * @brief Set the blend mode for a surface.
 *
 * @param surface Pointer to the surface.
 * @param mode Blend mode to set.
 */
void bglSetBlendMode(Surface* surface, BlendMode mode) {
    if (!surface) return;
    surface->blend_mode = mode;
}


/**
 * @brief Set the clipping rectangle for a surface.
 *
 * @param surface Pointer to the surface.
 * @param rect Pointer to the rectangle to set (NULL for full surface).
 */
void bglSetClipRect(Surface* surface, Rect* rect) {
    if (!surface) return;
    if (!rect) {
        surface->clip_rect.x = 0;
        surface->clip_rect.y = 0;
        surface->clip_rect.w = surface->w;
        surface->clip_rect.h = surface->h;
    } else {
        surface->clip_rect = *rect;
    }
}
