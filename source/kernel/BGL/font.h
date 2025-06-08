#ifndef _KERNEL_GRAPHICS_TEXT_H
#define _KERNEL_GRAPHICS_TEXT_H 1

#include "surface.h"
#include "draw.h"


// Font structure
typedef struct {
    uint8_t* data;          // Font bitmap data
    uint8_t char_width;     // Width of each character
    uint8_t char_height;    // Height of each character
    uint16_t total_chars;   // Total number of characters
    uint8_t scale;          // Font scaling factor
} Font;


// Font creation/destruction
Font* bglCreateFont(uint8_t* font_data, uint8_t char_width, uint8_t char_height, uint8_t scale);
void bglDestroyFont(Font* font);

// Text rendering functions
Surface* bglRenderText(Font* font, const char* text, uint8_t fg_color, uint8_t bg_color);
Surface* bglRenderTextScaled(Font* font, const char* text, uint8_t fg_color, uint8_t bg_color, float scale);

// Quick text drawing functions
void bglDrawText(Surface* surface, Font* font, const char* text, Rect* dstrect, uint8_t fg_color, uint8_t bg_color);

// Text measurement
void bglGetTextSize(Font* font, const char* text, uint16_t* width, uint16_t* height);


#endif /* _KERNEL_GRAPHICS_TEXT_H */
