#ifndef MONARCH_BASE_WIN_TERMINAL_H
#define MONARCH_BASE_WIN_TERMINAL_H 1

#include "base/gfx/render.h"

typedef struct Terminal Terminal;

typedef struct TerminalCell {
    char value;
    uint32_t foreground;
    uint32_t background;
} TerminalCell;

struct Terminal {
    int (*init)(Terminal *self, uint32_t cols, uint32_t rows);
    void (*destroy)(Terminal *self);
    int (*resize)(Terminal *self, uint32_t cols, uint32_t rows);
    void (*feed)(Terminal *self, const char *data, size_t size);
    void (*line_new)(Terminal *self);
    void (*line_clear)(Terminal *self);
    void (*paint)(Terminal *self, struct renderer *renderer, int32_t x, int32_t y);
    TerminalCell *cells;
    uint32_t cols;
    uint32_t rows;
    uint32_t stride;
    uint32_t capacity_cols;
    uint32_t capacity_rows;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t foreground;
    uint32_t background;
    uint32_t default_foreground;
    uint32_t default_background;
    uint32_t csi_value;
    uint8_t parser_state;
    uint8_t csi_private;
    uint8_t cursor_visible;
};

void Terminal_init(Terminal *terminal);

#endif /* MONARCH_BASE_WIN_TERMINAL_H */
