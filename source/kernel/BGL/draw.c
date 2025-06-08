#include "draw.h"
#include "../memory/memory.h"
#include "../memory/heap.h"


// Helper function to merge overlapping rectangles
static void mergeRects(Rect* r1, Rect* r2) {
    uint16_t x1 = MIN(r1->x, r2->x);
    uint16_t y1 = MIN(r1->y, r2->y);
    uint16_t x2 = MAX(r1->x + r1->w, r2->x + r2->w);
    uint16_t y2 = MAX(r1->y + r1->h, r2->y + r2->h);

    r1->x = x1;
    r1->y = y1;
    r1->w = x2 - x1;
    r1->h = y2 - y1;
}


/**
 * @brief Create a new dirty rectangle list.
 */
DirtyRectList* bglCreateDirtyRectList(uint16_t max_rects) {
    if (max_rects > MAX_DIRTY_RECTS) max_rects = MAX_DIRTY_RECTS;
    DirtyRectList* list = (DirtyRectList*) memoryAllocateBlock(sizeof(DirtyRectList));
    if (list) {
        list->count = 0;
        list->max = max_rects;
    }
    return list;
}


/**
 * @brief Destroy a dirty rectangle list.
 */
void bglDestroyDirtyRectList(DirtyRectList* list) {
    if (list) memoryFreeBlock(list);
}


/**
 * @brief Add a dirty rectangle to the list.
 *
 * @param list The dirty rectangle list.
 * @param rect The rectangle to add.
 */
void bglAddDirtyRect(DirtyRectList* list, Rect* rect) {
    if (!list || !rect || list->count >= list->max) return;

    // Try to merge with existing rectangles
    for (uint16_t i = 0; i < list->count; i++) {
        Rect* current = &list->rects[i];
        // Check if rectangles overlap
        if (!(current->x + current->w < rect->x ||
              current->x > rect->x + rect->w ||
              current->y + current->h < rect->y ||
              current->y > rect->y + rect->h)) {
            mergeRects(current, rect);
            return;
        }
    }

    // Add as new rectangle if no merge possible
    list->rects[list->count++] = *rect;
}


/**
 * @brief Clear the dirty rectangle list.
 */
void bglClearDirtyRects(DirtyRectList* list) {
    if (list) list->count = 0;
}


/**
 * @brief Update and redraw the dirty rectangles on the destination surface.
 */
inline void bglUpdateRects(Surface* dst, Surface* src, DirtyRectList* list) {
    if (!dst || !src || !list) return;

    for (uint16_t i = 0; i < list->count; i++) {
        Rect* rect = &list->rects[i];
        bglBlit(src, rect, dst, rect);
    }
}


/**
 * @brief Update a specific rectangle on the destination surface.
 */
inline void bglUpdateRect(Surface* dst, Surface* src, Rect* rect) {
    if (!dst || !src || !rect) return;

    bglBlit(src, rect, dst, rect);
}


/**
 * @brief Set a pixel in the surface.
 *
 * @param surface The surface to set the pixel on.
 * @param x The x coordinate of the pixel.
 * @param y The y coordinate of the pixel.
 * @param color The color to set the pixel to.
 */
inline void bglSetPixel(Surface* surface, uint16_t x, uint16_t y, uint8_t color) {
    if (!surface || x >= surface->w || y >= surface->h) return;

    uint32_t offset = (y * surface->pitch) + (x >> 1);

    if (x & 1) {
        surface->pixels[offset] = (surface->pixels[offset] & 0xF0) | color;
    } else {
        surface->pixels[offset] = (surface->pixels[offset] & 0x0F) | (color << 4);
    }
}


/**
 * @brief Get a pixel from the surface.
 *
 * @param surface The surface to get the pixel from.
 * @param x The x coordinate of the pixel.
 * @param y The y coordinate of the pixel.
 * @return The color of the pixel.
 */
inline uint8_t bglGetPixel(Surface* surface, uint16_t x, uint16_t y) {
    if (!surface || !surface->pixels || x >= surface->w || y >= surface->h) return 0;

    uint32_t offset = (y * surface->pitch) + (x >> 1);
    uint8_t pixel = surface->pixels[offset];

    return (x & 1) ? (pixel & 0x0F) : (pixel >> 4);
}



void bglDrawLine(Surface* surface, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color) {
    uint16_t dx = ABS((x2 - x1));
    uint16_t dy = ABS((y2 - y1));
    int8_t sx = x1 < x2 ? 1 : -1;
    int8_t sy = y1 < y2 ? 1 : -1;
    uint16_t err = dx - dy;

    uint16_t e2 = 0;

    while (1) {
        bglSetPixel(surface, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;

        e2 = err << 1;

        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}


void bglDrawRect(Surface* surface, Rect* rect, uint8_t color) {
    if (!surface || !rect) return;

    bglDrawLine(surface, rect->x, rect->y, rect->x + rect->w, rect->y, color);
    bglDrawLine(surface, rect->x, rect->y + rect->h, rect->x + rect->w, rect->y + rect->h, color);
    bglDrawLine(surface, rect->x, rect->y, rect->x, rect->y + rect->h, color);
    bglDrawLine(surface, rect->x + rect->w, rect->y, rect->x + rect->w, rect->y + rect->h, color);
}


void bglFillRect(Surface* surface, Rect* rect, uint8_t color) {
    if (!surface || !rect) return;

    uint16_t start_x = MAX(rect->x, 0);
    uint16_t start_y = MAX(rect->y, 0);
    uint16_t end_x = MIN(rect->x + rect->w, surface->w);
    uint16_t end_y = MIN(rect->y + rect->h, surface->h);

    for (uint16_t y = start_y; y < end_y; y++) {
        for (uint16_t x = start_x; x < end_x; x++) {
            bglSetPixel(surface, x, y, color);
        }
    }
}


// Drawing circles without sqrt - using midpoint circle algorithm
void bglDrawCircle(Surface* surface, uint16_t x0, uint16_t y0, uint16_t radius, uint8_t color) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        bglSetPixel(surface, x0 + x, y0 + y, color);
        bglSetPixel(surface, x0 + y, y0 + x, color);
        bglSetPixel(surface, x0 - y, y0 + x, color);
        bglSetPixel(surface, x0 - x, y0 + y, color);
        bglSetPixel(surface, x0 - x, y0 - y, color);
        bglSetPixel(surface, x0 - y, y0 - x, color);
        bglSetPixel(surface, x0 + y, y0 - x, color);
        bglSetPixel(surface, x0 + x, y0 - y, color);

        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }

        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}


void bglFillCircle(Surface* surface, uint16_t x, uint16_t y, uint16_t radius, uint8_t color) {
    for (int16_t i = -radius; i <= radius; i++) {
        for (int16_t j = -radius; j <= radius; j++) {
            if (((j * j) + (i * i)) < ((radius * radius) - radius)) {
                bglSetPixel(surface, x + j, y + i, color);
            }
        }
    }
}


// Surface transformation functions
void bglScrollSurface(Surface* surface, Rect* rect, int16_t dx, int16_t dy) {
    if (!surface || !rect) return;

    Surface* temp = bglCreateSurface(rect->w, rect->h);
    if (!temp) return;

    // Copy region to temp surface
    bglBlit(surface, rect, temp, NULL);

    // Clear original region
    uint8_t zero = 0;
    bglFillRect(surface, rect, zero);

    // Calculate new position
    Rect dst = {
        rect->x + dx,
        rect->y + dy,
        rect->w,
        rect->h
    };

    // Copy back with offset
    bglBlit(temp, NULL, surface, &dst);
    bglDestroySurface(temp);
}


void bglResizeSurface(Surface* surface, uint16_t new_width, uint16_t new_height) {
    if (!surface || !surface->pixels) return;

    // Create a new pixel buffer
    uint16_t new_pitch = (new_width + 1) >> 1;
    uint8_t* new_pixels = (uint8_t*) memoryAllocateBlock(new_pitch * new_height);
    if (!new_pixels) return;

    // Calculate scaling factors
    float x_scale = (float)new_width / surface->w;
    float y_scale = (float)new_height / surface->h;

    // Resize the surface using nearest neighbor scaling
    for (uint16_t y = 0; y < new_height; y++) {
        for (uint16_t x = 0; x < new_width; x++) {
            uint16_t src_x = (uint16_t)(x / x_scale);
            uint16_t src_y = (uint16_t)(y / y_scale);

            // Get the pixel color from the original surface
            uint8_t color = bglGetPixel(surface, src_x, src_y);

            // Set the pixel color in the new buffer
            uint32_t offset = (y * new_pitch) + (x >> 1);
            if (x & 1) {
                new_pixels[offset] = (new_pixels[offset] & 0xF0) | color;
            } else {
                new_pixels[offset] = (new_pixels[offset] & 0x0F) | (color << 4);
            }
        }
    }

    // Free the old pixel data
    if (surface->flags & BGL_SURFACE_OWNED) {
        memoryFreeBlock(surface->pixels);
    }

    // Update surface properties
    surface->pixels = new_pixels;
    surface->w = new_width;
    surface->h = new_height;
    surface->pitch = new_pitch;
    surface->flags |= BGL_SURFACE_OWNED;  // Mark that we own these pixels

    // Update clip rectangle
    surface->clip_rect.w = new_width;
    surface->clip_rect.h = new_height;
}


void bglFlipSurface(Surface* surface, bool vertical, bool horizontal) {
    if (!surface || !surface->pixels) return;

    // Create a temporary surface to hold the flipped data
    Surface* temp = bglCreateSurface(surface->w, surface->h);
    if (!temp) return;

    // Flip the surface
    for (uint16_t y = 0; y < surface->h; y++) {
        for (uint16_t x = 0; x < surface->w; x++) {
            uint16_t new_x = horizontal ? (surface->w - 1 - x) : x;
            uint16_t new_y = vertical ? (surface->h - 1 - y) : y;

            uint8_t color = bglGetPixel(surface, new_x, new_y);
            bglSetPixel(temp, x, y, color);
        }
    }

    // Copy the flipped data back to the original surface
    fastFastMemoryCopy(surface->pixels, temp->pixels, surface->pitch * surface->h);

    // Clean up
    bglDestroySurface(temp);
}


void bglResizeSprite(Surface* sprite, uint16_t new_width, uint16_t new_height) {
    if (!sprite || !sprite->pixels || !(sprite->flags & BGL_SURFACE_SPRITE)) return;

    // Create a new pixel buffer
    uint16_t new_pitch = (new_width + 1) >> 1;
    uint8_t* new_pixels = (uint8_t*) memoryAllocateBlock(new_pitch * new_height);
    if (!new_pixels) return;

    // Calculate scaling factors
    float x_scale = (float)new_width / sprite->w;
    float y_scale = (float)new_height / sprite->h;

    // Resize using nearest neighbor scaling
    for (uint16_t y = 0; y < new_height; y++) {
        for (uint16_t x = 0; x < new_width; x++) {
            uint16_t src_x = (uint16_t)(x / x_scale);
            uint16_t src_y = (uint16_t)(y / y_scale);

            uint8_t color = bglGetPixel(sprite, src_x, src_y);

            uint32_t offset = (y * new_pitch) + (x >> 1);
            if (x & 1) {
                new_pixels[offset] = (new_pixels[offset] & 0xF0) | color;
            } else {
                new_pixels[offset] = (new_pixels[offset] & 0x0F) | (color << 4);
            }
        }
    }

    // Free the old pixel data since sprites always own their pixels
    memoryFreeBlock(sprite->pixels);

    // Update sprite properties
    sprite->pixels = new_pixels;
    sprite->w = new_width;
    sprite->h = new_height;
    sprite->pitch = new_pitch;

    // Update clip rectangle
    sprite->clip_rect.w = new_width;
    sprite->clip_rect.h = new_height;
}
