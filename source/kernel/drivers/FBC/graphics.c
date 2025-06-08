#include "graphics.h"
#include "../VGA/video.h"

#include "../../CPU/HAL.h"
#include "../../memory/heap.h"
#include "../../memory/memory.h"



/**
 * If you are reading this, it's because you have realized that we are working
 * with planar memory and not with a linear framebuffer. And yes, it seems I
 * like to go the complicated but more elegant route. Let's say I was punishing
 * my brain to try and make it do the dirty work of correctly printing a pixel
 * on the screen. If you think the code is slow, go ahead, good luck optimizing
 * it more than I could. Haha :D
*/

// TODO: Optimize image drawing functions, we can enable all the planes at same time!!

// TODO: Add more functions to handle shapes and figures, such as triangles, hexagons,
// lines with a specific thickness, etc :)


/**
 * Sets the color of a single pixel on the screen
 *
 * @param color The color of the pixel
 * @param x The x-coordinate of the pixel
 * @param y The y-coordinate of the pixel
 */
inline void gfx_plotp(uint8_t color, uint16_t x, uint16_t y) {
    if (x >= GRAPHMODE_WIDTH || y >= GRAPHMODE_HEIGHT) return;

    // Pointer to the start of the screen memory buffer
    volatile uint8_t *SCREEN_MEMORY = GRAPHMODE_BUFFER;

    // Calculate the memory offset based on the pixel coordinates
    // Each byte represents 8 pixels, so we need to adjust for this
    SCREEN_MEMORY += (y * GRAPHMODE_WIDTH + x) >> 3;

    // Create a mask to isolate the specific bit for the pixel
    // 0x80 corresponds to the most significant bit of a byte,
    // shifting it right by (x & 0x7) or (x % 8) positions to align with the pixel position
    uint8_t mask = (0x80 >> (x & 0x7));

    writeByteToPort(SEQUENCER_INDEX, REG_SEQUENCER_MASK);
    writeByteToPort(GRAPHICS_INDEX, 0x08);
    writeByteToPort(GRAPHICS_DATA, mask); // We select only specifics bits
    writeByteToPort(SEQUENCER_DATA, 0x0F); // Enable all planes for drawing

    // Clear the current pixel bit by masking out the bit
    *SCREEN_MEMORY &= ~mask;

    // Write the color value to the selected pixel bit
    writeByteToPort(SEQUENCER_DATA, color);

    // Set the pixel bit to the new color by OR-ing the mask
    *SCREEN_MEMORY |= mask;
}


/**
 * Reads the color of a pixel from the screen memory buffer
 *
 * @param x The x-coordinate of the pixel
 * @param y The y-coordinate of the pixel
 * @return The color value of the pixel (0-15), or 0 if the
 *         coordinates are out of bounds
 */
uint8_t gfx_readp(uint16_t x, uint16_t y) {

    // We simply return a black pixel in case we exceed the screen area
    if (x >= GRAPHMODE_WIDTH || y >= GRAPHMODE_HEIGHT) {
        return 0;
    }

    // Pointer to the start of the screen memory buffer
    volatile uint8_t *SCREEN_MEMORY = GRAPHMODE_BUFFER;

    // Calculate the memory offset based on the pixel coordinates
    // Each byte represents 8 pixels, so we need to adjust for this
    SCREEN_MEMORY += (y * GRAPHMODE_WIDTH + x) >> 3;

    uint8_t color = 0x00;

    // Mask to isolate the bit corresponding to the pixel
	uint8_t mask = 0x80 >> (x & 0x7);

	writeByteToPort(GRAPHICS_INDEX, REG_GRAPHICS_MAP_READ);

    // Loop through the four planes
    for (uint8_t i = 0; i < 4; ++i) {
        writeByteToPort(GRAPHICS_DATA, i);

        // Check if the bit corresponding to the pixel
        // is set and update the color value
        if (*SCREEN_MEMORY & mask) {
            color |= (1 << i);
        }
    }

	return color;
}


/**
 * Fills the entire screen with a specified color
 *
 * @param color The color to fill the screen with
 */
void gfx_clear(uint8_t color) {
    uint32_t SCREEN_SIZE = GRAPHMODE_SIZE >> 3;

    // Enable write on all the planes
    writeByteToPort(SEQUENCER_INDEX, REG_SEQUENCER_MASK);
    writeByteToPort(SEQUENCER_DATA, 0x0F);

    // We select all the bits
    writeByteToPort(GRAPHICS_INDEX, 0x08);
    writeByteToPort(GRAPHICS_DATA, 0xFF);

    /*
    * NOTE: This is really fast, but for some reason, calling memset
    *       twice makes me think this is still not perfect.
    */

    // Clear the screen buffer to prepare for filling
    fastFastMemorySet(GRAPHMODE_BUFFER, 0x00, SCREEN_SIZE);

    // Write the color value to all pixels in the buffer
    writeByteToPort(SEQUENCER_DATA, color);

    // Fill the screen buffer with the color
    fastFastMemorySet(GRAPHMODE_BUFFER, 0xFF, SCREEN_SIZE);
}


/**
 * @brief Draws a bitmap on the screen
 *
 * @param pixels    Pointer to the bitmap pixel data
 * @param x         X coordinate of the top left corner of the bitmap
 * @param y         Y coordinate of the top left corner of the bitmap
 * @param w         Width of the bitmap in pixels
 * @param h         Height of the bitmap in pixels
 */
void gfx_draw(uint8_t *pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Pointer to the start of the screen memory.
    uint8_t *SCREEN_MEMORY = GRAPHMODE_BUFFER;

    // Allocate a temporary buffer to store the bitmap in planar format (1 bit per pixel per plane).
    // The size is (w * h) / 8 bytes.
    uint8_t *BITMAP_BUFFER = (uint8_t *) memoryAllocateBlock(((w * h) / 8));

    // Calculate the starting offset in screen memory for the drawing position.
    SCREEN_MEMORY += (y * GRAPHMODE_WIDTH + x) >> 3;

    // For the input packed format, each row occupies (w + 1) / 2 bytes (2 pixels per byte).
    uint32_t row_bytes = (w + 1) >> 1;

    // Loop over each color plane.
    for (uint8_t plane = 0; plane < 4; plane++) {
        uint8_t mask = 1 << plane;

        // Select the current color plane for reading.
        writeByteToPort(GRAPHICS_INDEX, REG_GRAPHICS_MAP_READ);
        writeByteToPort(GRAPHICS_DATA, plane);

        // Set the write mask for the current plane.
        writeByteToPort(SEQUENCER_INDEX, REG_SEQUENCER_MASK);
        writeByteToPort(SEQUENCER_DATA, mask);

        // Process each row of the bitmap.
        for (uint32_t row = 0; row < h; row++) {
            // Calculate the starting offset into the packed pixels array for this row.
            uint32_t row_offset = row * row_bytes;

            // Calculate the starting pointer in the temporary buffer for this row.
            // The temporary buffer stores 1 bit per pixel, so each row takes (w + 7) / 8 bytes.
            uint8_t *screen_row = BITMAP_BUFFER + (row * w) / 8;

            // (Optional) Clear the row in the temporary buffer.
            fastFastMemorySet(screen_row, 0, (w + 7) >> 3);

            // Process each pixel in the row.
            for (uint32_t column = 0; column < w; column++) {
                // For the packed 4bpp image, each byte holds two pixels.
                uint32_t byte_index = row_offset + (column >> 1);
                uint8_t packed = pixels[byte_index];
                uint8_t color;

                // (column & 1) == 0?
                if (column & 1) {
                    // Odd column: extract the low nibble.
                    color = packed & 0x0F;
                } else {
                    // Even column: extract the high nibble.
                    color = packed >> 4;
                }

                // Determine the bit to set/clear in the temporary buffer.
                uint8_t bit = 1 << (7 - (column & 7));

                // Set or clear the bit based on the current plane's bit in the color.
                if (color & mask) {
                    *(screen_row + (column >> 3)) |= bit;  // Set bit in buffer.
                } else {
                    *(screen_row + (column >> 3)) &= ~bit;   // Clear bit in buffer.
                }
            }
        }

        // Calculate the starting offset in screen memory for the bitmap.
        uint8_t *screen_offset = SCREEN_MEMORY; // Already adjusted for x and y.

        // Copy the entire temporary buffer to the corresponding location in screen memory.
        for (uint32_t row = 0; row < h; row++) {
            uint8_t *destination = screen_offset + (row * GRAPHMODE_WIDTH) / 8;
            uint8_t *source = BITMAP_BUFFER + (row * w) / 8;
            fastFastMemoryCopy(destination, source, w >> 3);
        }
    }

    // Free the temporary buffer.
    memoryFreeBlock(BITMAP_BUFFER);
}


void gfx_plotl(uint8_t color, uint16_t fx, uint16_t fy, uint16_t sx, uint16_t sy) {
    int16_t dx = ABS((sx - fx)); // delta X
    int16_t dy = -ABS((sy - fy)); // delta Y
    int8_t ix = (fx < sx) ? 1 : -1; // sign of X direction
    int8_t iy = (fy < sy) ? 1 : -1; // sign of Y direction

    // errors
    int16_t fe = dx + dy;
    int16_t se;

    while (1) {
        gfx_plotp(color, fx, fy);
        if (fx == sx && fy == sy) break;

        // se = fe * 2;
        se = fe + fe;
        if (se >= dy) {
            fe += dy;
            fx += ix;
        }
        if (se <= dx) {
            fe += dx;
            fy += iy;
        }
    }
}


void gfx_plotr(uint8_t color, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool solid) {
    gfx_plotl(color, x, y, x, y + h);
    gfx_plotl(color, x, y, x + w, y);
    gfx_plotl(color, x + w, y, x + w, y + h);
    gfx_plotl(color, x, y + h, x + w, y + h);

    if (solid) {
        for (uint16_t i = y; i < y + h; i++) {
            gfx_plotl(color, x, i, x + w, i);
        }
    }
}


void gfx_ploto(uint8_t color, uint16_t cx, uint16_t cy, uint16_t r, bool solid) {
    if (solid) {
        for (int16_t y = -r; y <= r; y++) {
            for (int16_t x = -r; x <= r; x++) {
                if ((x*x + y*y) < (r*r - r)) {
                    gfx_plotp(color, cx + x, cy + y);
                }
            }
        }
        return;
    }

    int16_t x = 0;
    int16_t y = r;
    int16_t p = 1 - r;

    while (x <= y) {
        // Draw symmetric points
        gfx_plotp(color, cx + x, cy + y);
        gfx_plotp(color, cx - x, cy + y);
        gfx_plotp(color, cx + x, cy - y);
        gfx_plotp(color, cx - x, cy - y);
        gfx_plotp(color, cx + y, cy + x);
        gfx_plotp(color, cx - y, cy + x);
        gfx_plotp(color, cx + y, cy - x);
        gfx_plotp(color, cx - y, cy - x);

        x++;
        if (p < 0) {
            p += 2 * x + 1;
        } else {
            y--;
            p += 2 * (x - y) + 1;
        }
    }
}



/**
 * Draws a character on the screen.
 *
 * @param c The character to draw.
 * @param x The x-coordinate where the character will be drawn.
 * @param y The y-coordinate where the character will be drawn.
 * @param color The color of the character. This is an 8-bit value,
 *              where the lower 4 bits represent the foreground color
 *              and the upper 4 bits represent the background color.
 */
void gfx_plotc(unsigned char character, uint16_t x, uint16_t y, uint8_t color) {
    uint8_t fgcolor = color & 0x0F;        // Extract foreground color
    uint8_t bgcolor = (color >> 4) & 0x0F; // Extract background color

    // Get the glyph for the character from the font. The glyph is an 8x8 bitmap that represents the character
    // Each byte in the glyph represents one row of 8 pixels in the character
    uint8_t *glyph = (character * 8) + small_font;

    // The 8x8 font bitmap have 64 pixels per character, sooo ...
    for (uint8_t i = 0; i < 64; i++) {

        // The hardcoded 64 could be a bad idea, but the performance is first LOL

        uint8_t cx = i & 7;       // i % 8.
        uint8_t cy = i >> 3;      // i / 8.

        // Create a mask for the current pixel, the mask is a byte where only one bit is set
        // The position of the set bit corresponds to the position of the pixel in the row
        // For example, if cx is 0, the mask will be 10000000 in binary, if cx is 7, the mask will be 00000001
        uint8_t mask = 1 << (7 - cx);

        // Draw the pixel at the corresponding position
        // If the corresponding bit in the row is set (i.e., the bitwise AND of the row and the mask is not zero),
        // then the pixel is drawn with the foreground color. Otherwise, it is drawn with the background color
        gfx_plotp((glyph[cy] & mask) ? fgcolor : bgcolor, x + cx, y + cy);
    }
}


void gfx_plots(const char *string, uint16_t x, uint16_t y, uint8_t color) {
    uint16_t ox = x;
    uint16_t width = GRAPHMODE_WIDTH << 3;
    uint16_t height = GRAPHMODE_HEIGHT << 3;

    do {
        switch (*string) {
            case '\n': y += 8; break;
            case '\t': x += 32; break;
            case '\r': x = ox; break;
            default:
                gfx_plotc(*string, x, y, color);
                x += 8;
                break;
        }
        if (x >= width) {
            x = ox;
            y += 8;
        }
    } while ((y < height) && *(string++));
}


void gfx_probe(void) {
    uint16_t x = 0;
    uint16_t y = 8;

    for (uint8_t i = 32; i != 255; i++) { // Only printable chars
        gfx_plotc((char) i, x, y, 0x10 | 0x0E);
        if (x >= GRAPHMODE_WIDTH) {
            x = 0;
            y = (y >= GRAPHMODE_HEIGHT - 1) ? 0 : (y + 9);
        }
        x += 16;
    }
}
