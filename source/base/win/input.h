#ifndef MONARCH_APPS_GUI_INPUT_H
#define MONARCH_APPS_GUI_INPUT_H 1

#include "base/api/monarch.h"

struct Event;

typedef struct Input Input;

struct Input {
    int (*open)(Input *self);
    void (*close)(Input *self);
    int (*poll)(Input *self, struct Event *event);
    int32_t mouse_x;
    int32_t mouse_y;
    uint32_t mouse_buttons;
    int keyboard;
    int mouse;
    int mouse_seen;
};

#define EVENT_NONE        0u
#define EVENT_MOUSE_MOVE  1u
#define EVENT_MOUSE_DOWN  2u
#define EVENT_MOUSE_UP    3u
#define EVENT_KEY_DOWN    4u
#define EVENT_MOUSE_ENTER 5u
#define EVENT_MOUSE_LEAVE 6u
#define EVENT_FOCUS       7u
#define EVENT_BLUR        8u
#define EVENT_CLOSE       9u
#define EVENT_MINIMIZE   10u
#define EVENT_MAXIMIZE   11u
#define EVENT_RESIZE     12u
#define EVENT_MOVE       13u

typedef struct MouseEvent MouseEvent;
typedef struct KeyEvent KeyEvent;
typedef struct WindowEvent WindowEvent;

struct MouseEvent {
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    uint32_t buttons;
};

struct KeyEvent {
    uint32_t key;
    uint32_t pressed;
};

struct WindowEvent {
    uint32_t type;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

struct Event {
    uint32_t type;
    uint32_t action;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    uint32_t buttons;
    uint32_t key;
    MouseEvent mouse;
    KeyEvent keyboard;
    WindowEvent window;
};

#define WING_EVENT_NONE  0u
#define WING_EVENT_KEY   1u
#define WING_EVENT_MOUSE 2u

void Input_init(Input *input);

#endif /* MONARCH_APPS_GUI_INPUT_H */
