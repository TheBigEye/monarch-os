/**
 * @file input.c
 * @brief Userspace Wing input-device boundary.
 */

#include "input.h"
#include "base/usr/sys.h"

static int input_open(struct Input *);
static void input_close(struct Input *);
static int input_poll(struct Input *, struct Event *);

void Input_init(struct Input *input) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    input->open = input_open;
    input->close = input_close;
    input->poll = input_poll;
    input->keyboard = -1;
    input->mouse = -1;
}
static int input_open(struct Input *input) {
    if (!input) return 0;
    input->keyboard = (int)open("/dev/keyboard", OREAD);
    input->mouse = (int)open("/dev/mouse", OREAD);
    return input->keyboard >= 0 || input->mouse >= 0;
}
static void input_close(struct Input *input) {
    if (!input) return;
    if (input->keyboard >= 0) close(input->keyboard);
    if (input->mouse >= 0) close(input->mouse);
    input->keyboard = -1;
    input->mouse = -1;
}
static int input_poll(struct Input *input, struct Event *event) {
    struct kbd_event key;
    struct mouse_event mouse;

    if (!input || !event) return 0;
    memset(event, 0, sizeof(*event));
    /* Mouse packets are time-sensitive. Read them before keyboard traffic so
       typing cannot make pointer motion wait behind a key stream. */
    if (input->mouse >= 0 && ioctl(input->mouse, MOUSE_GETEVENT, &mouse) == 0) {
        input->mouse_seen = 1;
        input->mouse_x = mouse.x;
        input->mouse_y = mouse.y;
        input->mouse_buttons = mouse.buttons;
        event->type = WING_EVENT_MOUSE;
        event->x = mouse.x;
        event->y = mouse.y;
        event->dx = mouse.dx;
        event->dy = mouse.dy;
        event->buttons = mouse.buttons;
        return 1;
    }
    if (input->keyboard >= 0 && ioctl(input->keyboard, KBD_GETEVENT, &key) == 0) {
        event->type = WING_EVENT_KEY;
        event->key = key.ascii ? key.ascii : key.code;
        return 1;
    }
    return 0;
}
