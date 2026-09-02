#ifndef MONARCH_APPS_GUI_LABEL_H
#define MONARCH_APPS_GUI_LABEL_H 1

#include "base/gfx/render.h"

typedef struct Label Label;

struct Label {
    void (*paint)(struct Label *self, struct renderer *renderer,
                  uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
    const char *text;
};

void Label_init(struct Label *self, const char *text);

#endif /* MONARCH_APPS_GUI_LABEL_H */
