#include "font.h"

#include "../memory/memory.h"
#include "../memory/heap.h"

/**
 * @brief Create a font from bitmap data
 *
 * @param font_data Pointer to the font bitmap data, an array of bytes.
 * @param char_width Width of each character in pixels.
 * @param char_height Height of each character in pixels.
 * @param scale Scaling factor for the font (1 for no scaling).
 * @return Pointer to the created Font structure, or NULL on failure.
 *
 * @note
 * - The font data should be in a format where each byte represents a row
 *   of pixels for each character, and should be 1bpp (1 bit per pixel),
 *   where each bit represents a pixel (1 for foreground, 0 for background).
 */
Font* bglCreateFont(uint8_t* font_data, uint8_t char_width, uint8_t char_height, uint8_t scale) {
    Font* font = (Font*) memoryAllocateBlock(sizeof(Font));

    // Check if the allocation was successful
    if (!font) {
        return NULL;
    }

    // If no scale is provided, use 1
    if (!scale) {
        scale = 1;
    }

    // Update font properties
    font->data = font_data;
    font->char_width = char_width;
    font->char_height = char_height;
    font->total_chars = 256;
    font->scale = scale;

    return font;
}


/**
 * @brief Destroy a font and free its resources.
 */
void bglDestroyFont(Font* font) {
    if (!font) return;

    memoryFreeBlock(font);
}


/**
 * @brief Renders a single character using the same algorithm as gfx_plotc
 *
 * @param surface Destination surface
 * @param font Font to use
 * @param character Character to render
 * @param x X coordinate
 * @param y Y coordinate
 * @param fg_color Foreground color
 * @param bg_color Background color
 * @param scale Character scaling factor
 */
static void bglRenderChar(Surface* surface, Font* font, unsigned char character, uint16_t x, uint16_t y, uint8_t fg_color, uint8_t bg_color, uint8_t scale) {
    // Get the glyph for the character from the font
    uint8_t *glyph = (character * font->char_height) + font->data;

    // Loop through each pixel of the character
    for (uint8_t cy = 0; cy < font->char_height; cy++) {
        uint8_t row = glyph[cy];

        uint16_t yy = cy * scale;

        for (uint8_t cx = 0; cx < font->char_width; cx++) {
            // Create a mask for the current pixel
            uint8_t mask = 1 << (7 - cx);

            uint16_t xx = cx * scale;

            // Determine the color based on the bit value
            uint8_t color = (row & mask) ? fg_color : bg_color;

            // Apply scaling by drawing multiple pixels for each original pixel
            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    bglSetPixel(surface, x + xx + sx, y + yy + sy, color);
                }
            }
        }
    }
}


/**
 * @brief Render a text and return a surface with the rendered text.
 *
 * @param font Pointer to the Font structure.
 * @param text Pointer to the string to render.
 * @param fg_color Foreground color (text color).
 * @param bg_color Background color.
 *
 * @return Pointer to the created Surface with the rendered text, or NULL on failure.
 */
Surface* bglRenderText(Font* font, const char* text, uint8_t fg_color, uint8_t bg_color) {
    if (!font || !text) return NULL;

    uint16_t length = strlen(text);
    uint16_t scaled_width = font->char_width * font->scale;
    uint16_t scaled_height = font->char_height * font->scale;

    Surface* surface = bglCreateSurface(length * scaled_width, scaled_height);
    if (!surface) return NULL;

    // Fill with background color
    bglFillSurface(surface, bg_color);

    // Is useless if the fg and bg are the same, so
    // we return a fast filled surface instead...
    if (fg_color == bg_color) {
        return surface;
    }

    // For each char in the text...
    for (uint16_t i = 0; i < length; i++) {
        // Calculate the position to draw the character
        uint16_t x = i * scaled_width;

        // Render the character
        bglRenderChar(surface, font, text[i], x, 0, fg_color, bg_color, font->scale);
    }

    return surface;
}


/**
 * @brief Render a text and return a scaled surface with the rendered text.
 *
 * @param font Pointer to the Font structure.
 * @param text Pointer to the string to render.
 * @param fg_color Foreground color (text color).
 * @param bg_color Background color.
 * @param scale Scaling factor for the text (applied on top of font's scale).
 *
 * @return Pointer to the created Surface with the rendered text, or NULL on failure.
 */
Surface* bglRenderTextScaled(Font* font, const char* text, uint8_t fg_color, uint8_t bg_color, float scale) {
    if (!font || !text) return NULL;

    uint16_t length = strlen(text);
    uint16_t effective_scale = (uint8_t)(font->scale * scale);

    if (effective_scale < 1) effective_scale = 1;

    uint16_t scaled_width = font->char_width * effective_scale;
    uint16_t scaled_height = font->char_height * effective_scale;

    Surface* surface = bglCreateSurface(length * scaled_width, scaled_height);
    if (!surface) return NULL;

    // Fill with background color
    bglFillSurface(surface, bg_color);

    // Is useless if the fg and bg are the same, so
    // we return a fast filled surface instead...
    if (fg_color == bg_color) {
        return surface;
    }

    // For each char in the text...
    for (uint16_t i = 0; i < length; i++) {
        // Calculate the position to draw the character
        uint16_t x = i * scaled_width;

        // Render the character
        bglRenderChar(surface, font, text[i], x, 0, fg_color, bg_color, effective_scale);
    }

    return surface;
}


/**
 * @brief Draw text on a surface.
 *
 * @param surface Pointer to the destination surface.
 * @param font Pointer to the Font structure.
 * @param text Pointer to the string to draw.
 * @param dstrect Pointer to the destination rectangle.
 * @param fg_color Foreground color (text color).
 * @param bg_color Background color.
 */
void bglDrawText(Surface* surface, Font* font, const char* text, Rect* dstrect, uint8_t fg_color, uint8_t bg_color) {
    if (!surface || !font || !text || !dstrect) return;

    Surface* text_surface = bglRenderText(font, text, fg_color, bg_color);
    if (!text_surface) return;

    bglBlit(text_surface, NULL, surface, dstrect);
    bglDestroySurface(text_surface);
}


/**
 * @brief Get the size of the text when rendered with the specified font.
 *
 * @param font Pointer to the Font structure.
 * @param text Pointer to the string to measure.
 * @param width Pointer to store the calculated width.
 * @param height Pointer to store the calculated height.
 */
void bglGetTextSize(Font* font, const char* text, uint16_t* width, uint16_t* height) {
    if (!font || !text || !width || !height) return;

    *width = strlen(text) * font->char_width * font->scale;
    *height = font->char_height * font->scale;
}
