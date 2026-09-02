#ifndef MONARCH_APPS_GUI_ASSETS_H
#define MONARCH_APPS_GUI_ASSETS_H 1

#include "base/api/monarch.h"

/* Small indexed masks keep decorative widgets independent from the renderer. */
#define WING_ASSET_W 8u
#define WING_ASSET_H 8u

static const uint8_t wing_corner_mask[WING_ASSET_W * WING_ASSET_H] = {
    0,0,0,1,1,1,1,1,
    0,0,1,1,1,1,1,1,
    0,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1
};

/* Win95-like bevel palette: highlight, face, shadow, dark shadow. */
#define WING_HILITE 0xFFFFFFu
#define WING_FACE   0xC0C0C0u
#define WING_SHADOW 0x808080u
#define WING_DARK   0x000000u
#define WING_BLUE   0x000080u

static const uint8_t wing_close_glyph[WING_ASSET_W * WING_ASSET_H] = {
    1,0,0,0,0,0,0,1,
    0,1,0,0,0,0,1,0,
    0,0,1,0,0,1,0,0,
    0,0,0,1,1,0,0,0,
    0,0,0,1,1,0,0,0,
    0,0,1,0,0,1,0,0,
    0,1,0,0,0,0,1,0,
    1,0,0,0,0,0,0,1
};

static const uint8_t wing_max_glyph[WING_ASSET_W * WING_ASSET_H] = {
    1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1
};

static const uint8_t wing_min_glyph[WING_ASSET_W * WING_ASSET_H] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,1,1,1,1,1,1,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

#endif /* MONARCH_APPS_GUI_ASSETS_H */
