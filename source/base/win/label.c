/**
 * @file label.c
 * @brief Wing text-label widget.
 */

#include "label.h"

static void paint(struct Label *self, struct renderer *renderer,
                       uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    if (self && renderer && renderer->text && self->text)
        renderer->text(renderer, self->text, x, y, fg, bg);
}

void Label_init(struct Label *self, const char *text) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->paint = paint;
    self->text = text;
}
