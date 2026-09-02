/**
 * @file terminal.c
 * @brief Small terminal cell grid for the userspace shell window.
 */
#include "terminal.h"
#include "base/gfx/allocator.h"
#include "base/gfx/font.h"

static int init(Terminal *, uint32_t, uint32_t);
static void destroy(Terminal *);
static int resize(Terminal *, uint32_t, uint32_t);
static void feed(Terminal *, const char *, size_t);
static void line_new(Terminal *);
static void line_clear(Terminal *);
static void paint(Terminal *, struct renderer *, int32_t, int32_t);

void Terminal_init(Terminal *terminal) {
    if (!terminal) return;
    memset(terminal, 0, sizeof(*terminal));
    terminal->init = init;
    terminal->destroy = destroy;
    terminal->resize = resize;
    terminal->feed = feed;
    terminal->line_new = line_new;
    terminal->line_clear = line_clear;
    terminal->paint = paint;
    terminal->foreground = 0xFFFFFFu;
    terminal->background = 0x101820u;
    terminal->default_foreground = terminal->foreground;
    terminal->default_background = terminal->background;
    terminal->cursor_visible = 1;
}

static int init(Terminal *self, uint32_t cols, uint32_t rows) {
    const uint32_t max_cols = 160u;
    const uint32_t max_rows = 60u;
    if (!self || !cols || !rows || cols > max_cols || rows > max_rows) return 0;
    self->cols = cols;
    self->rows = rows;
    self->stride = max_cols;
    self->capacity_cols = max_cols;
    self->capacity_rows = max_rows;
    self->cursor_x = 0;
    self->cursor_y = 0;
    self->cells = gfx_zero((size_t)max_cols * max_rows, sizeof(*self->cells));
    if (!self->cells) return 0;
    for (uint32_t i = 0; i < max_cols * max_rows; i++) {
        self->cells[i].value = ' ';
        self->cells[i].foreground = self->foreground;
        self->cells[i].background = self->background;
    }
    return 1;
}

static void destroy(Terminal *self) {
    if (!self) return;
    if (self->cells) gfx_free(self->cells);
    self->cells = nil;
    self->cols = 0;
    self->rows = 0;
    self->stride = 0;
    self->capacity_cols = 0;
    self->capacity_rows = 0;
}

static int resize(Terminal *self, uint32_t cols, uint32_t rows) {
    uint32_t old_cols;
    uint32_t old_rows;
    uint32_t old_x;
    uint32_t old_y;
    if (!self || !self->cells || !cols || !rows ||
        cols > self->capacity_cols || rows > self->capacity_rows) return 0;
    old_cols = self->cols;
    old_rows = self->rows;
    old_x = self->cursor_x;
    old_y = self->cursor_y;
    self->cols = cols;
    self->rows = rows;
    for (uint32_t y = 0; y < rows; y++) {
        for (uint32_t x = 0; x < cols; x++) {
            if (y >= old_rows || x >= old_cols) {
                TerminalCell *cell = &self->cells[y * self->stride + x];
                cell->value = ' ';
                cell->foreground = self->foreground;
                cell->background = self->background;
            }
        }
    }
    self->cursor_x = old_x < cols ? old_x : cols - 1u;
    self->cursor_y = old_y < rows ? old_y : rows - 1u;
    return 1;
}

static void line_new(Terminal *self) {
    self->cursor_x = 0;
    if (++self->cursor_y >= self->rows) {
        memmove(self->cells, self->cells + self->stride,
                (size_t)(self->rows - 1u) * self->stride * sizeof(*self->cells));
        for (uint32_t x = 0; x < self->cols; x++) {
            TerminalCell *cell = &self->cells[(self->rows - 1u) * self->stride + x];
            cell->value = ' '; cell->foreground = self->foreground; cell->background = self->background;
        }
        self->cursor_y = self->rows - 1u;
    }
}

static void line_clear(Terminal *self) {
    if (!self || !self->cells) return;
    for (uint32_t x = 0; x < self->cols; x++) {
        TerminalCell *cell = &self->cells[self->cursor_y * self->stride + x];
        cell->value = ' ';
        cell->foreground = self->foreground;
        cell->background = self->background;
    }
    self->cursor_x = 0;
}

static uint32_t ansi_color(uint32_t code, int bright) {
    static const uint32_t colors[8] = {
        0x000000u, 0xAA0000u, 0x00AA00u, 0xAA5500u,
        0x0000AAu, 0xAA00AAu, 0x00AAAAu, 0xAAAAAAu
    };
    static const uint32_t bright_colors[8] = {
        0x555555u, 0xFF5555u, 0x55FF55u, 0xFFFF55u,
        0x5555FFu, 0xFF55FFu, 0x55FFFFu, 0xFFFFFFu
    };
    return code < 8u ? (bright ? bright_colors[code] : colors[code]) : 0xFFFFFFu;
}

static void csi_final(Terminal *self, unsigned char ch) {
    uint32_t amount;
    if (!self) return;
    amount = self->csi_value ? self->csi_value : 1u;
    if (ch == 'm') {
        if (self->csi_value == 0u) {
            self->foreground = self->default_foreground;
            self->background = self->default_background;
        } else if (self->csi_value >= 30u && self->csi_value <= 37u) {
            self->foreground = ansi_color(self->csi_value - 30u, 0);
        } else if (self->csi_value >= 90u && self->csi_value <= 97u) {
            self->foreground = ansi_color(self->csi_value - 90u, 1);
        } else if (self->csi_value >= 40u && self->csi_value <= 47u) {
            self->background = ansi_color(self->csi_value - 40u, 0);
        }
    } else if (ch == 'h' && self->csi_private && self->csi_value == 25u) {
        self->cursor_visible = 1;
    } else if (ch == 'l' && self->csi_private && self->csi_value == 25u) {
        self->cursor_visible = 0;
    } else if (ch == 'A') self->cursor_y = self->cursor_y > amount ? self->cursor_y - amount : 0;
    else if (ch == 'B') self->cursor_y = self->cursor_y + amount < self->rows ? self->cursor_y + amount : self->rows - 1u;
    else if (ch == 'C') self->cursor_x = self->cursor_x + amount < self->cols ? self->cursor_x + amount : self->cols - 1u;
    else if (ch == 'D') self->cursor_x = self->cursor_x > amount ? self->cursor_x - amount : 0;
    else if (ch == 'H' || ch == 'f') { self->cursor_x = 0; self->cursor_y = 0; }
    else if (ch == 'J' && self->csi_value == 2u) {
        for (uint32_t i = 0; i < self->stride * self->rows; i++) {
            self->cells[i].value = ' ';
            self->cells[i].foreground = self->foreground;
            self->cells[i].background = self->background;
        }
        self->cursor_x = 0; self->cursor_y = 0;
    } else if (ch == 'K') self->line_clear(self);
    /* SGR colors are intentionally kept at the default palette for now. */
    self->csi_value = 0;
    self->csi_private = 0;
}

static void feed(Terminal *self, const char *data, size_t size) {
    if (!self || !self->cells || !data) return;
    for (size_t i = 0; i < size; i++) {
        unsigned char ch = (unsigned char)data[i];
        if (self->parser_state == 1) {
            if (ch == '[') { self->parser_state = 2; self->csi_value = 0; }
            else if (ch == ']') self->parser_state = 3;
            else self->parser_state = 0;
            continue;
        }
        if (self->parser_state == 2) {
            if (ch == '?' && self->csi_value == 0u) {
                self->csi_private = 1;
            } else if (ch >= '0' && ch <= '9') {
                self->csi_value = self->csi_value * 10u + (ch - '0');
            } else if (ch >= 0x40u && ch <= 0x7Eu) {
                csi_final(self, ch); self->parser_state = 0;
            }
            continue;
        }
        if (self->parser_state == 3) {
            if (ch == 0x07u) self->parser_state = 0;
            continue;
        }
        if (ch == 0x1Bu) { self->parser_state = 1; continue; }
        if (ch == '\r') { self->cursor_x = 0; continue; }
        if (ch == '\n') { self->line_new(self); continue; }
        if (ch == '\b') {
            if (self->cursor_x) {
                self->cursor_x--;
                self->cells[self->cursor_y * self->stride + self->cursor_x].value = ' ';
                self->cells[self->cursor_y * self->stride + self->cursor_x].foreground = self->foreground;
                self->cells[self->cursor_y * self->stride + self->cursor_x].background = self->background;
            }
            continue;
        }
        if (ch < 0x20u) continue;
        self->cells[self->cursor_y * self->stride + self->cursor_x].value = (char)ch;
        self->cells[self->cursor_y * self->stride + self->cursor_x].foreground = self->foreground;
        self->cells[self->cursor_y * self->stride + self->cursor_x].background = self->background;
        if (++self->cursor_x >= self->cols) self->line_new(self);
    }
}

static void paint(Terminal *self, struct renderer *renderer, int32_t x, int32_t y) {
    if (!self || !self->cells || !renderer) return;
    for (uint32_t row = 0; row < self->rows; row++) {
        for (uint32_t col = 0; col < self->cols; col++) {
            TerminalCell *cell = &self->cells[row * self->stride + col];
            int32_t px = x + (int32_t)col * 8;
            int32_t py = y + (int32_t)row * 16;
            const uint8_t *glyph = font8x16 + (uint32_t)(uint8_t)cell->value * 16u;
            if (cell->value == ' ' && cell->background == self->default_background)
                continue;
            renderer->rect(renderer, (uint32_t)px, (uint32_t)py, 8, 16, cell->background);
            for (uint32_t glyph_row = 0; glyph_row < 16u; glyph_row++) {
                for (uint32_t glyph_col = 0; glyph_col < 8u; glyph_col++) {
                    if (glyph[glyph_row] & (0x80u >> glyph_col))
                        renderer->pixel(renderer, (uint32_t)px + glyph_col,
                                        (uint32_t)py + glyph_row, cell->foreground);
                }
            }
        }
    }
    if (self->cursor_visible && self->cursor_x < self->cols && self->cursor_y < self->rows)
        renderer->rect(renderer, (uint32_t)(x + (int32_t)self->cursor_x * 8),
                       (uint32_t)(y + (int32_t)self->cursor_y * 16 + 14),
                       8, 2, self->foreground);
}
