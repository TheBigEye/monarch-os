#ifndef MONARCH_DRIVERS_CHAR_MOUSE_H
#define MONARCH_DRIVERS_CHAR_MOUSE_H 1

#include "base/api/monarch.h"

struct mouseevent {
    int x;
    int y;
    int dx;
    int dy;
    int dz;
    uint8_t buttons;
};

#define MOUSE_QUEUE_SIZE 32u

struct mouse {
    int (*poll)(struct mouse *self, struct mouseevent *event);
    void (*bounds)(struct mouse *self, int width, int height);
    int (*x)(struct mouse *self);
    int (*y)(struct mouse *self);
    int (*z)(struct mouse *self);
    uint8_t (*buttons)(struct mouse *self);
    int (*ready)(struct mouse *self);
    uint8_t (*id)(struct mouse *self);
    uint32_t (*packets)(struct mouse *self);
    uint32_t (*errors)(struct mouse *self);

    volatile int _ready;
    volatile int _packet_index;
    uint8_t _packet[4];
    uint8_t _packet_size;
    uint8_t _id;
    volatile int _changed;
    int _x;
    int _y;
    int _z;
    int _dx;
    int _dy;
    int _dz;
    int _width;
    int _height;
    uint8_t _buttons;
    uint32_t _packets;
    uint32_t _errors;
    /* IRQ12 produces packets independently of userspace polling. */
    struct mouseevent _queue[MOUSE_QUEUE_SIZE];
    volatile uint8_t _qhead;
    volatile uint8_t _qtail;
};

void mouse(struct mouse *self, int width, int height);
struct mouse *mouseget(void);

#endif /* MONARCH_DRIVERS_CHAR_MOUSE_H */
