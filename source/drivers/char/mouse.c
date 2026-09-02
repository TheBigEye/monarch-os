#include "drivers/char/mouse.h"
#include "arch/x86/cpu.h"
#include "arch/x86/interrupt.h"

#define PS2_STATUS 0x64
#define PS2_DATA   0x60
#define PS2_ACK    0xFA
#define PS2_RESEND 0xFE

static struct mouse *active;

static int wait_input(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x02) == 0) {
            return 1;
        }
    }
    return 0;
}

static int wait_output(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) {
            return 1;
        }
    }
    return 0;
}

static int read_data(uint8_t *out) {
    if (!wait_output()) {
        return 0;
    }
    *out = inb(PS2_DATA);
    return 1;
}

static int command(uint8_t value) {
    if (!wait_input()) {
        return 0;
    }
    outb(PS2_STATUS, value);
    return 1;
}

static int data(uint8_t value) {
    if (!wait_input()) {
        return 0;
    }
    outb(PS2_DATA, value);
    return 1;
}

static void flush(void) {
    for (uint32_t i = 0; i < 32 && (inb(PS2_STATUS) & 1); i++) {
        (void)inb(PS2_DATA);
    }
}

static int writemouse(uint8_t value) {
    uint8_t response = 0;

    for (int retry = 0; retry < 3; retry++) {
        if (!command(0xD4) || !data(value) || !read_data(&response)) {
            continue;
        }
        if (response == PS2_ACK) {
            return 1;
        }
        if (response != PS2_RESEND) {
            break;
        }
    }

    if (active) {
        active->_errors++;
    }
    return 0;
}

static int writemouse_arg(uint8_t command_byte, uint8_t argument) {
    return writemouse(command_byte) && writemouse(argument);
}

static uint8_t mouseid(void) {
    uint8_t id = 0;
    if (!writemouse(0xF2)) {
        return 0;
    }
    if (!read_data(&id)) {
        if (active) {
            active->_errors++;
        }
        return 0;
    }
    return id;
}

static void clamp(struct mouse *self) {
    if (self->_x < 0) self->_x = 0;
    if (self->_y < 0) self->_y = 0;
    if (self->_x >= self->_width) self->_x = self->_width - 1;
    if (self->_y >= self->_height) self->_y = self->_height - 1;
}

static int wheel_delta(uint8_t byte) {
    int z = byte & 0x0F;
    if (z & 0x08) {
        z |= ~0x0F;
    }
    return z;
}

static void packet(struct mouse *self) {
    int dx;
    int dy;
    int dz = 0;

    if ((self->_packet[0] & 0x08) == 0 || (self->_packet[0] & 0xC0) != 0) {
        self->_packet_index = 0;
        self->_errors++;
        return;
    }

    dx = (int8_t)self->_packet[1];
    dy = (int8_t)self->_packet[2];
    if (self->_packet_size == 4) {
        dz = wheel_delta(self->_packet[3]);
    }

    self->_dx = dx;
    self->_dy = -dy;
    self->_dz = dz;
    self->_x += dx;
    self->_y -= dy;
    self->_z += dz;
    self->_buttons = self->_packet[0] & 0x07;
    if (self->_packet_size == 4) {
        self->_buttons |= self->_packet[3] & 0x30;
    }
    clamp(self);
    self->_packets++;
    self->_changed = 1;

    /* IRQ12 may produce several packets between userspace polls. Keep them in
       a bounded queue so cursor motion is not reduced to occasional jumps. */
    {
        uint8_t next = (uint8_t)((self->_qhead + 1u) % MOUSE_QUEUE_SIZE);
        struct mouseevent *queued = &self->_queue[self->_qhead];
        queued->x = self->_x;
        queued->y = self->_y;
        queued->dx = dx;
        queued->dy = -dy;
        queued->dz = dz;
        queued->buttons = self->_buttons;
        self->_qhead = next;
        if (next == self->_qtail) {
            self->_qtail = (uint8_t)((self->_qtail + 1u) % MOUSE_QUEUE_SIZE);
        }
    }
}

static void irq(struct registers *state) {
    uint8_t status;
    uint8_t byte;

    unused(state);

    if (!active || !active->_ready) {
        if (inb(PS2_STATUS) & 1) {
            (void)inb(PS2_DATA);
        }
        return;
    }

    status = inb(PS2_STATUS);
    if ((status & 0x20) == 0 || (status & 0x01) == 0) {
        return;
    }

    byte = inb(PS2_DATA);
    if (active->_packet_index == 0 && (byte & 0x08) == 0) {
        active->_errors++;
        return;
    }

    active->_packet[active->_packet_index++] = byte;
    if (active->_packet_index >= active->_packet_size) {
        active->_packet_index = 0;
        packet(active);
    }
}

static int poll_impl(struct mouse *self, struct mouseevent *event) {
    uint8_t tail;

    if (!self || !event || self->_qhead == self->_qtail) {
        return 0;
    }

    /* Consumers such as Boing run at a fixed frame rate. Return the newest
       position instead of replaying stale packets and making the pointer feel
       delayed or over-responsive after a quick movement. */
    tail = self->_qtail;
    do {
        *event = self->_queue[tail];
        tail = (uint8_t)((tail + 1u) % MOUSE_QUEUE_SIZE);
    } while (tail != self->_qhead);
    self->_qtail = self->_qhead;
    self->_changed = 0;
    return 1;
}

static void bounds_impl(struct mouse *self, int width, int height) {
    if (!self) {
        return;
    }
    self->_width = width > 0 ? width : 1;
    self->_height = height > 0 ? height : 1;
    clamp(self);
}

static int x_impl(struct mouse *self) { return self ? self->_x : 0; }
static int y_impl(struct mouse *self) { return self ? self->_y : 0; }
static int z_impl(struct mouse *self) { return self ? self->_z : 0; }
static uint8_t buttons_impl(struct mouse *self) { return self ? self->_buttons : 0; }
static int ready_impl(struct mouse *self) { return self && self->_ready; }
static uint8_t id_impl(struct mouse *self) { return self ? self->_id : 0; }
static uint32_t packets_impl(struct mouse *self) { return self ? self->_packets : 0; }
static uint32_t errors_impl(struct mouse *self) { return self ? self->_errors : 0; }

struct mouse *mouseget(void) {
    return active && active->_ready ? active : nil;
}

void mouse(struct mouse *self, int width, int height) {
    uint8_t config = 0;
    uint8_t value = 0;

    memset(self, 0, sizeof(*self));
    self->poll = poll_impl;
    self->bounds = bounds_impl;
    self->x = x_impl;
    self->y = y_impl;
    self->z = z_impl;
    self->buttons = buttons_impl;
    self->ready = ready_impl;
    self->id = id_impl;
    self->packets = packets_impl;
    self->errors = errors_impl;
    self->_width = width > 0 ? width : 1;
    self->_height = height > 0 ? height : 1;
    self->_x = self->_width / 2;
    self->_y = self->_height / 2;
    self->_packet_size = 3;
    active = self;

    flush();
    command(0xA8); /* enable auxiliary device */

    if (command(0x20) && read_data(&value)) {
        config = value;
    }
    config |= 0x02;           /* enable IRQ12 */
    config &= (uint8_t)~0x20; /* enable mouse clock */
    command(0x60);
    data(config);

    /* Reset is useful on real-ish controllers, but some emulators are lax.
       Treat failures as non-fatal and continue with a standard mouse. */
    if (writemouse(0xFF)) {
        uint8_t selftest = 0;
        uint8_t id = 0;
        (void)read_data(&selftest);
        (void)read_data(&id);
        self->_id = id;
    }

    writemouse(0xF6); /* defaults */

    /* IntelliMouse wheel detection sequence: 200, 100, 80, then get ID. */
    if (writemouse_arg(0xF3, 200) && writemouse_arg(0xF3, 100) && writemouse_arg(0xF3, 80)) {
        self->_id = mouseid();
        if (self->_id == 3) {
            self->_packet_size = 4;
        }
    }

    /* Use a moderate resolution. The PS/2 default is often four counts per
       millimetre, which feels excessively sensitive in a 800x600 desktop. */
    writemouse_arg(0xE8, 1); /* two counts per millimetre */
    writemouse_arg(0xF3, 100); /* stable packet rate */
    writemouse(0xF4); /* enable streaming */

    irq_register(IRQ_MOUSE, irq);
    irq_enable(IRQ_CASCADE);
    irq_enable(IRQ_MOUSE);
    self->_ready = 1;
}
