/**
 * @file console.c
 * @brief Framebuffer/VGA console with ANSI subset, cursor color and text rendering.
 *
 */

#include "drivers/char/console.h"
#include "arch/x86/cpu.h"
#include "arch/x86/pit.h"

#define FB_CONSOLE_MAX_COLUMNS 160u
#define FB_CONSOLE_MAX_ROWS 120u

static const uint8_t ansi16[16][3] = {
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0xaa}, {0x00, 0xaa, 0x00}, {0x00, 0xaa, 0xaa},
    {0xaa, 0x00, 0x00}, {0xaa, 0x00, 0xaa}, {0xaa, 0x55, 0x00}, {0xaa, 0xaa, 0xaa},
    {0x55, 0x55, 0x55}, {0x55, 0x55, 0xff}, {0x55, 0xff, 0x55}, {0x55, 0xff, 0xff},
    {0xff, 0x55, 0x55}, {0xff, 0x55, 0xff}, {0xff, 0xff, 0x55}, {0xff, 0xff, 0xff},
};

static uint32_t fb_shadow[FB_CONSOLE_MAX_COLUMNS * FB_CONSOLE_MAX_ROWS];
static struct console *active;

#define CELL_BOLD      0x00010000u
#define CELL_UNDERLINE 0x00020000u

static uint32_t cell(char ch, uint8_t color, int bold, int underline) {
    uint32_t value = ((uint32_t)color << 8) | (uint8_t)ch;
    if (bold) {
        value |= CELL_BOLD;
    }
    if (underline) {
        value |= CELL_UNDERLINE;
    }
    return value;
}

static uint32_t currentcell(struct console *self, char ch) {
    return cell(ch, self->_color, self->_bold, self->_underline);
}

static uint32_t rgb(struct console *self, uint8_t index) {
    index &= 0x0F;
    if (self->_fb) {
        return self->_fb->rgb(self->_fb, ansi16[index][0], ansi16[index][1], ansi16[index][2]);
    }
    return ((uint32_t)ansi16[index][0] << 16) | ((uint32_t)ansi16[index][1] << 8) | ansi16[index][2];
}


static uint8_t ansicolor(unsigned code, int bright) {
    static const uint8_t normal[8] = {
        VGA_BLACK, VGA_RED, VGA_GREEN, VGA_BROWN,
        VGA_BLUE, VGA_MAGENTA, VGA_CYAN, VGA_LIGHT_GREY
    };
    static const uint8_t intense[8] = {
        VGA_DARK_GREY, VGA_LIGHT_RED, VGA_LIGHT_GREEN, VGA_YELLOW,
        VGA_LIGHT_BLUE, VGA_LIGHT_MAGENTA, VGA_LIGHT_CYAN, VGA_WHITE
    };

    code &= 7u;
    return bright ? intense[code] : normal[code];
}

static uint8_t namecolor(const char *name) {
    if (strcmp(name, "black") == 0) return VGA_BLACK;
    if (strcmp(name, "red") == 0) return VGA_LIGHT_RED;
    if (strcmp(name, "green") == 0) return VGA_LIGHT_GREEN;
    if (strcmp(name, "yellow") == 0) return VGA_YELLOW;
    if (strcmp(name, "blue") == 0) return VGA_LIGHT_BLUE;
    if (strcmp(name, "magenta") == 0) return VGA_LIGHT_MAGENTA;
    if (strcmp(name, "cyan") == 0) return VGA_LIGHT_CYAN;
    if (strcmp(name, "white") == 0) return VGA_WHITE;
    return VGA_LIGHT_GREY;
}

static void setfg(struct console *self, uint8_t color) {
    self->_color = (uint8_t)((self->_color & 0xF0u) | (color & 0x0Fu));
    self->_fg = rgb(self, color);
}

static void setbg(struct console *self, uint8_t color) {
    self->_color = (uint8_t)(((color & 0x0Fu) << 4) | (self->_color & 0x0Fu));
    self->_bg = rgb(self, color);
}

static void ansireset(struct console *self) {
    setfg(self, self->_default_foreground);
    setbg(self, self->_default_background);
}

static uint32_t *cellptr(struct console *self, unsigned row, unsigned column) {
    if (!self->_shadow || row >= self->_rows || column >= self->_columns) {
        return nil;
    }
    return &self->_shadow[row * self->_columns + column];
}

static void hardware(struct console *self) {
    uint16_t pos;

    if (self->_fb) {
        return;
    }

    pos = (uint16_t)(self->_row * self->_columns + self->_column);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void drawglyph(struct console *self, unsigned row, unsigned column, char ch, uint32_t fg, uint32_t bg, int bold, int underline) {
    uint32_t x = column * FONT_WIDTH;
    uint32_t y = row * FONT_HEIGHT;
    const uint8_t *glyph = &font8x16[(uint8_t)ch * FONT_HEIGHT];

    for (uint32_t gy = 0; gy < FONT_HEIGHT; gy++) {
        uint8_t bits = glyph[gy];
        if (underline && gy == FONT_HEIGHT - 2u) {
            bits = 0xFFu;
        }
        for (uint32_t gx = 0; gx < FONT_WIDTH; gx++) {
            int set = (bits & (0x80u >> gx)) != 0;
            if (bold && gx > 0 && (bits & (0x80u >> (gx - 1u)))) {
                set = 1;
            }
            self->_fb->pixel(self->_fb, x + gx, y + gy, set ? fg : bg);
        }
    }
}

static void drawcell(struct console *self, unsigned row, unsigned column, int inverted) {
    uint32_t *ptr = cellptr(self, row, column);
    uint32_t value;
    uint8_t attr;
    uint32_t fg;
    uint32_t bg;

    if (!self->_fb || !ptr) {
        return;
    }

    value = *ptr;
    attr = (uint8_t)((value >> 8) & 0xFFu);
    fg = rgb(self, attr & 0x0F);
    bg = rgb(self, (uint8_t)(attr >> 4));

    if (inverted) {
        if (self->_cursor_custom) {
            fg = self->_cursor_fg;
            bg = self->_cursor_bg;
        } else {
            uint32_t tmp = fg;
            fg = bg;
            bg = tmp;
        }
    }

    drawglyph(self, row, column, (char)(value & 0xFF), fg, bg, (value & CELL_BOLD) != 0, (value & CELL_UNDERLINE) != 0);
}

/*
 * Draw or erase the software cursor in the framebuffer console.
 *
 * Older code had separate cursorhide()/cursorshow() routines with mirrored
 * state checks.  Keeping the state transition in one helper makes cursor
 * behaviour easier to audit as we add color and ANSI movement support.
 */
static void cursor_draw(struct console *self, int visible) {
    if (!self || !self->_fb) {
        return;
    }

    if (!visible) {
        if (self->_cursor_drawn) {
            drawcell(self, self->_cursor_row, self->_cursor_column, 0);
            self->_cursor_drawn = 0;
        }
        return;
    }

    if (!self->_cursor_enabled || !self->_cursor_on) {
        return;
    }

    self->_cursor_row = min(self->_row, self->_rows - 1);
    self->_cursor_column = min(self->_column, self->_columns - 1);
    drawcell(self, self->_cursor_row, self->_cursor_column, 1);
    self->_cursor_drawn = 1;
}

/* Make the cursor visible immediately after user-visible cursor movement. */
static void cursor_touch(struct console *self) {
    if (!self || !self->_fb || !self->_cursor_enabled) {
        return;
    }

    cursor_draw(self, 0);
    self->_cursor_on = 1;
    self->_cursor_last = ticks();
    cursor_draw(self, 1);
}

static void clear_line(struct console *self, unsigned row) {
    if (self->_fb) {
        for (unsigned col = 0; col < self->_columns; col++) {
            uint32_t *ptr = cellptr(self, row, col);
            if (ptr) {
                *ptr = currentcell(self, ' ');
            }
        }
        self->_fb->rect(self->_fb, 0, row * FONT_HEIGHT, self->_fb->width, FONT_HEIGHT, self->_bg);
        return;
    }

    for (unsigned col = 0; col < self->_columns; col++) {
        self->_buffer[row * self->_columns + col] = (uint16_t)currentcell(self, ' ');
    }
}

static void scroll(struct console *self) {
    if (self->_row < self->_rows) {
        return;
    }

    cursor_draw(self, 0);

    if (self->_fb) {
        size_t rowbytes = (size_t)self->_fb->pitch * FONT_HEIGHT;
        size_t movebytes = (size_t)self->_fb->pitch * (self->_fb->height - FONT_HEIGHT);
        memmove(self->_fb->address, self->_fb->address + rowbytes, movebytes);
        memmove(self->_shadow, self->_shadow + self->_columns, (self->_rows - 1) * self->_columns * sizeof(uint32_t));
        self->_fb->rect(self->_fb, 0, self->_fb->height - FONT_HEIGHT, self->_fb->width, FONT_HEIGHT, self->_bg);
    } else {
        for (unsigned row = 1; row < self->_rows; row++) {
            for (unsigned col = 0; col < self->_columns; col++) {
                self->_buffer[(row - 1) * self->_columns + col] = self->_buffer[row * self->_columns + col];
            }
        }
    }

    clear_line(self, self->_rows - 1);
    self->_row = self->_rows - 1;
}

static void clear(struct console *self) {
    cursor_draw(self, 0);

    if (self->_fb) {
        for (unsigned i = 0; i < self->_rows * self->_columns; i++) {
            self->_shadow[i] = currentcell(self, ' ');
        }
        self->_fb->clear(self->_fb, self->_bg);
    } else {
        for (unsigned i = 0; i < self->_rows * self->_columns; i++) {
            self->_buffer[i] = (uint16_t)currentcell(self, ' ');
        }
    }

    self->_row = 0;
    self->_column = 0;
    hardware(self);
    cursor_touch(self);
}



static void eraseeol(struct console *self) {
    cursor_draw(self, 0);

    if (self->_fb) {
        for (unsigned col = self->_column; col < self->_columns; col++) {
            uint32_t *ptr = cellptr(self, self->_row, col);
            if (ptr) {
                *ptr = currentcell(self, ' ');
                drawcell(self, self->_row, col, 0);
            }
        }
    } else {
        for (unsigned col = self->_column; col < self->_columns; col++) {
            self->_buffer[self->_row * self->_columns + col] = (uint16_t)currentcell(self, ' ');
        }
    }
    cursor_touch(self);
}

static void ansiparam(struct console *self) {
    if (self->_ansi_count < countof(self->_ansi_params)) {
        self->_ansi_params[self->_ansi_count++] = self->_ansi_value;
    }
    self->_ansi_value = 0;
}

static void sgr(struct console *self) {
    if (self->_ansi_count == 0) {
        ansireset(self);
        return;
    }

    for (unsigned i = 0; i < self->_ansi_count; i++) {
        unsigned value = self->_ansi_params[i];
        if (value == 0) {
            self->_bold = 0;
            self->_underline = 0;
            ansireset(self);
        } else if (value == 1) {
            self->_bold = 1;
        } else if (value == 22) {
            self->_bold = 0;
        } else if (value == 4) {
            self->_underline = 1;
        } else if (value == 24) {
            self->_underline = 0;
        } else if (value >= 30 && value <= 37) {
            setfg(self, ansicolor(value - 30u, 0));
        } else if (value >= 90 && value <= 97) {
            setfg(self, ansicolor(value - 90u, 1));
        } else if (value == 39) {
            setfg(self, self->_default_foreground);
        } else if (value >= 40 && value <= 47) {
            setbg(self, ansicolor(value - 40u, 0));
        } else if (value >= 100 && value <= 107) {
            setbg(self, ansicolor(value - 100u, 1));
        } else if (value == 49) {
            setbg(self, self->_default_background);
        }
    }
}

static void osc(struct console *self) {
    if (starts(self->_osc, "12;")) {
        uint8_t color = namecolor(self->_osc + 3);
        self->cursorcolor(self, VGA_BLACK, color);
    }
}

/**
 * Consume one byte of the small ANSI/OSC terminal parser.
 *
 * Returns non-zero when the byte was part of an escape sequence.  The supported
 * subset intentionally mirrors only what Monarch needs for colored prompts,
 * basic line editing and simple text attributes.
 */
static int ansi(struct console *self, char ch) {
    if (self->_ansi_state == 0) {
        if ((uint8_t)ch == 0x1Bu) {
            self->_ansi_state = 1;
            return 1;
        }
        return 0;
    }

    if (self->_ansi_state == 1) {
        if (ch == '[') {
            memset(self->_ansi_params, 0, sizeof(self->_ansi_params));
            self->_ansi_count = 0;
            self->_ansi_value = 0;
            self->_ansi_state = 2;
            return 1;
        }
        if (ch == ']') {
            self->_osc_len = 0;
            self->_osc[0] = '\0';
            self->_ansi_state = 3;
            return 1;
        }
        self->_ansi_state = 0;
        return 1;
    }

    if (self->_ansi_state == 2) {
        if (digit(ch)) {
            self->_ansi_value = self->_ansi_value * 10u + (unsigned)(ch - '0');
            return 1;
        }
        if (ch == ';') {
            ansiparam(self);
            return 1;
        }
        ansiparam(self);
        if (ch == 'm') {
            sgr(self);
        } else if (ch == 'H' || ch == 'f') {
            unsigned row = self->_ansi_params[0] ? self->_ansi_params[0] - 1u : 0;
            unsigned col = self->_ansi_count > 1 && self->_ansi_params[1] ? self->_ansi_params[1] - 1u : 0;
            self->move(self, row, col);
        } else if (ch == 'J' && self->_ansi_params[0] == 2) {
            self->clear(self);
        } else if (ch == 'K') {
            eraseeol(self);
        } else if (ch == 'C') {
            unsigned n = self->_ansi_params[0] ? self->_ansi_params[0] : 1u;
            self->move(self, self->_row, min(self->_column + n, self->_columns - 1u));
        } else if (ch == 'D') {
            unsigned n = self->_ansi_params[0] ? self->_ansi_params[0] : 1u;
            self->move(self, self->_row, self->_column > n ? self->_column - n : 0);
        } else if (ch == 'A') {
            unsigned n = self->_ansi_params[0] ? self->_ansi_params[0] : 1u;
            self->move(self, self->_row > n ? self->_row - n : 0, self->_column);
        } else if (ch == 'B') {
            unsigned n = self->_ansi_params[0] ? self->_ansi_params[0] : 1u;
            self->move(self, min(self->_row + n, self->_rows - 1u), self->_column);
        }
        self->_ansi_state = 0;
        return 1;
    }

    if (self->_ansi_state == 3) {
        if (ch == '\a') {
            osc(self);
            self->_ansi_state = 0;
            return 1;
        }
        if (self->_osc_len + 1u < sizeof(self->_osc)) {
            self->_osc[self->_osc_len++] = ch;
            self->_osc[self->_osc_len] = '\0';
        }
        return 1;
    }

    self->_ansi_state = 0;
    return 1;
}

static void put(struct console *self, char ch) {
    if (ansi(self, ch)) {
        return;
    }

    cursor_draw(self, 0);

    switch (ch) {
        case '\n':
            self->_column = 0;
            self->_row++;
            break;
        case '\r':
            self->_column = 0;
            break;
        case '\b':
            if (self->_column > 0) {
                self->_column--;
            } else if (self->_row > 0) {
                self->_row--;
                self->_column = self->_columns - 1;
            }
            if (self->_fb) {
                uint32_t *ptr = cellptr(self, self->_row, self->_column);
                if (ptr) {
                    *ptr = currentcell(self, ' ');
                }
                drawcell(self, self->_row, self->_column, 0);
            } else {
                self->_buffer[self->_row * self->_columns + self->_column] = (uint16_t)currentcell(self, ' ');
            }
            break;
        case '\t':
            do {
                put(self, ' ');
            } while ((self->_column % 4) != 0);
            return;
        default:
            if (self->_fb) {
                uint32_t *ptr = cellptr(self, self->_row, self->_column);
                if (ptr) {
                    *ptr = currentcell(self, ch);
                }
                drawcell(self, self->_row, self->_column, 0);
            } else {
                self->_buffer[self->_row * self->_columns + self->_column] = (uint16_t)currentcell(self, ch);
            }
            self->_column++;
            if (self->_column >= self->_columns) {
                self->_column = 0;
                self->_row++;
            }
            break;
    }

    scroll(self);
    hardware(self);
    cursor_touch(self);
}

static void write(struct console *self, const char *text) {
    while (*text) {
        self->put(self, *text++);
    }
}

static void putctx(void *ctx, char ch) {
    struct console *self = ctx;
    self->put(self, ch);
}

static int print(struct console *self, const char *fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = kvformat(putctx, self, fmt, ap);
    va_end(ap);
    return result;
}

static void move(struct console *self, unsigned row, unsigned column) {
    cursor_draw(self, 0);
    self->_row = min(row, self->_rows - 1);
    self->_column = min(column, self->_columns - 1);
    hardware(self);
    cursor_touch(self);
}

static void color(struct console *self, uint8_t foreground, uint8_t background) {
    cursor_draw(self, 0);
    self->_color = (uint8_t)((background << 4) | (foreground & 0x0F));
    self->_fg = rgb(self, foreground);
    self->_bg = rgb(self, background);
    cursor_touch(self);
}

static void cursor(struct console *self, int visible) {
    cursor_draw(self, 0);
    self->_cursor_enabled = visible ? 1 : 0;
    self->_cursor_on = visible ? 1 : 0;

    if (self->_fb) {
        cursor_draw(self, 1);
        return;
    }

    outb(0x3D4, 0x0A);
    outb(0x3D5, visible ? 0x0E : 0x20);
}

static void cursorcolor(struct console *self, uint8_t foreground, uint8_t background) {
    cursor_draw(self, 0);
    self->_cursor_fg = rgb(self, foreground);
    self->_cursor_bg = rgb(self, background);
    self->_cursor_custom = 1;
    cursor_touch(self);
}

static unsigned rows(struct console *self) {
    return self->_rows;
}

static unsigned columns(struct console *self) {
    return self->_columns;
}

void console_tick(void) {
    uint32_t now;
    uint32_t interval;

    if (!active || !active->_fb || !active->_cursor_enabled) {
        return;
    }

    now = ticks();
    interval = tickhz() ? tickhz() / 2u : 25u;
    if (!interval) {
        interval = 1;
    }

    if ((uint32_t)(now - active->_cursor_last) < interval) {
        return;
    }

    active->_cursor_last = now;
    if (active->_cursor_on) {
        active->_cursor_on = 0;
        cursor_draw(active, 0);
    } else {
        active->_cursor_on = 1;
        cursor_draw(active, 1);
    }
}

void console(struct console *self) {
    memset(self, 0, sizeof(*self));
    self->clear = clear;
    self->put = put;
    self->write = write;
    self->printf = print;
    self->move = move;
    self->color = color;
    self->cursor = cursor;
    self->cursorcolor = cursorcolor;
    self->rows = rows;
    self->columns = columns;
    self->_buffer = (volatile uint16_t *)0xB8000;
    self->_fb = framebuffer_get();

    if (self->_fb) {
        self->_rows = min(self->_fb->height / FONT_HEIGHT, FB_CONSOLE_MAX_ROWS);
        self->_columns = min(self->_fb->width / FONT_WIDTH, FB_CONSOLE_MAX_COLUMNS);
        self->_shadow = fb_shadow;
    } else {
        self->_rows = CONSOLE_HEIGHT;
        self->_columns = CONSOLE_WIDTH;
    }

    self->_default_foreground = VGA_LIGHT_GREY;
    self->_default_background = VGA_BLACK;
    self->_color = (uint8_t)((VGA_BLACK << 4) | VGA_LIGHT_GREY);
    self->_fg = rgb(self, VGA_LIGHT_GREY);
    self->_bg = rgb(self, VGA_BLACK);
    self->_cursor_fg = rgb(self, VGA_BLACK);
    self->_cursor_bg = rgb(self, VGA_LIGHT_RED);
    self->_cursor_custom = 1;
    self->_cursor_enabled = 1;
    self->_cursor_on = 1;
    self->_cursor_last = ticks();
    active = self;
}
