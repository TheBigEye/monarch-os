/**
 * @file console.h
 * @brief Console object API for text output, colors and cursor control.
 */

#ifndef MONARCH_DRIVERS_CHAR_CONSOLE_H
#define MONARCH_DRIVERS_CHAR_CONSOLE_H 1

#include "base/api/monarch.h"
#include "base/gfx/font.h"
#include "drivers/video/framebuffer.h"

#define CONSOLE_WIDTH 80u
#define CONSOLE_HEIGHT 25u
#define FONT_WIDTH FONT8X16_WIDTH
#define FONT_HEIGHT FONT8X16_HEIGHT

#define VGA_BLACK 0x0u
#define VGA_BLUE 0x1u
#define VGA_GREEN 0x2u
#define VGA_CYAN 0x3u
#define VGA_RED 0x4u
#define VGA_MAGENTA 0x5u
#define VGA_BROWN 0x6u
#define VGA_LIGHT_GREY 0x7u
#define VGA_DARK_GREY 0x8u
#define VGA_LIGHT_BLUE 0x9u
#define VGA_LIGHT_GREEN 0xAu
#define VGA_LIGHT_CYAN 0xBu
#define VGA_LIGHT_RED 0xCu
#define VGA_LIGHT_MAGENTA 0xDu
#define VGA_YELLOW 0xEu
#define VGA_WHITE 0xFu

struct console {
    void (*clear)(struct console *self);
    void (*put)(struct console *self, char ch);
    void (*write)(struct console *self, const char *text);
    int (*printf)(struct console *self, const char *fmt, ...);
    void (*move)(struct console *self, unsigned row, unsigned column);
    void (*color)(struct console *self, uint8_t foreground, uint8_t background);
    void (*cursor)(struct console *self, int visible);
    void (*cursorcolor)(struct console *self, uint8_t foreground, uint8_t background);
    unsigned (*rows)(struct console *self);
    unsigned (*columns)(struct console *self);

    volatile uint16_t *_buffer;
    uint32_t *_shadow;
    struct framebuffer *_fb;
    unsigned _row;
    unsigned _column;
    unsigned _rows;
    unsigned _columns;
    uint8_t _color;
    uint32_t _fg;
    uint32_t _bg;
    uint8_t _default_foreground;
    uint8_t _default_background;
    uint32_t _cursor_fg;
    uint32_t _cursor_bg;
    int _cursor_custom;
    int _bold;
    int _underline;

    int _ansi_state;
    unsigned _ansi_value;
    unsigned _ansi_count;
    unsigned _ansi_params[8];
    char _osc[40];
    unsigned _osc_len;

    int _cursor_enabled;
    int _cursor_on;
    int _cursor_drawn;
    unsigned _cursor_row;
    unsigned _cursor_column;
    uint32_t _cursor_last;
};

void console(struct console *self);
void console_tick(void);

#endif /* MONARCH_DRIVERS_CHAR_CONSOLE_H */
