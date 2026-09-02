#ifndef MONARCH_APPS_GUI_BUTTON_H
#define MONARCH_APPS_GUI_BUTTON_H 1

#include "base/gfx/render.h"

typedef struct Button Button;

struct Button {
    int (*hit)(Button *self, int32_t x, int32_t y);
    void (*paint)(Button *self, struct renderer *renderer);
    const char *label;
    const uint8_t *glyph;
    struct rect frame;
    int pressed;
    int hover;
    int disabled;
};

void Button_init(Button *self, const uint8_t *glyph);

#endif /* MONARCH_APPS_GUI_BUTTON_H */
