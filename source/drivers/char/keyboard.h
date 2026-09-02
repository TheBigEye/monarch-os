/**
 * @file keyboard.h
 * @brief Keyboard event API shared by kernel TTY and userspace input devices.
 */

#ifndef MONARCH_DRIVERS_CHAR_KEYBOARD_H
#define MONARCH_DRIVERS_CHAR_KEYBOARD_H 1

#include "base/api/monarch.h"

#define KEY_LEFT   0x81u
#define KEY_RIGHT  0x82u
#define KEY_HOME   0x83u
#define KEY_END    0x84u
#define KEY_DELETE 0x85u
#define KEY_UP     0x86u
#define KEY_DOWN   0x87u

struct keyevent {
    uint8_t code;
    char ascii;
    int pressed;
};

struct keyboard {
    struct keyevent (*event)(struct keyboard *self);
    int (*poll)(struct keyboard *self, struct keyevent *out);
    char (*read)(struct keyboard *self);
    int _shift;
    int _caps;
    int _extended;
};

void keyboard(struct keyboard *self);

#endif /* MONARCH_DRIVERS_CHAR_KEYBOARD_H */
