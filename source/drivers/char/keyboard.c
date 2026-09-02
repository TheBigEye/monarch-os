/**
 * @file keyboard.c
 * @brief PS/2 keyboard IRQ driver with printable and navigation key events.
 *
 */

#include "drivers/char/keyboard.h"
#include "arch/x86/cpu.h"
#include "arch/x86/interrupt.h"
#include "drivers/char/console.h"
#include "kernel/scheduler/thread.h"

#define QUEUE 128u

static const char normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'', '`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0
};

static const char shifted[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"', '~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0
};

static struct keyboard *active;
static struct keyevent queue[QUEUE];
static char pending[4];
static unsigned pending_len;
static unsigned pending_pos;
static volatile unsigned head;
static volatile unsigned tail;

static void push(struct keyevent event) {
    unsigned next = (head + 1u) % QUEUE;
    if (next == tail) {
        return;
    }
    queue[head] = event;
    head = next;
}

static char translate(struct keyboard *self, uint8_t scancode) {
    char ch;

    if (scancode >= countof(normal)) {
        return 0;
    }

    ch = self->_shift ? shifted[scancode] : normal[scancode];
    if ((normal[scancode] >= 'a' && normal[scancode] <= 'z') ||
        (normal[scancode] >= 'A' && normal[scancode] <= 'Z')) {
        ch = (self->_shift ^ self->_caps) ? upper(normal[scancode]) : lower(normal[scancode]);
    }
    return ch;
}

static void interruptkey(struct registers *state) {
    uint8_t scancode;
    uint8_t code;
    int released;
    struct keyevent event;

    unused(state);

    if (!active) {
        (void)inb(0x60);
        return;
    }

    scancode = inb(0x60);
    if (scancode == 0xE0u) {
        active->_extended = 1;
        return;
    }

    released = (scancode & 0x80u) != 0;
    code = (uint8_t)(scancode & 0x7Fu);

    if (active->_extended) {
        active->_extended = 0;
        if (released) {
            return;
        }
        memset(&event, 0, sizeof(event));
        event.pressed = 1;
        switch (code) {
            case 0x4Bu: event.code = KEY_LEFT; break;
            case 0x4Du: event.code = KEY_RIGHT; break;
            case 0x47u: event.code = KEY_HOME; break;
            case 0x4Fu: event.code = KEY_END; break;
            case 0x53u: event.code = KEY_DELETE; break;
            case 0x48u: event.code = KEY_UP; break;
            case 0x50u: event.code = KEY_DOWN; break;
            default: return;
        }
        push(event);
        return;
    }

    if (code == 0x2A || code == 0x36) {
        active->_shift = !released;
        return;
    }

    if (!released && code == 0x3A) {
        active->_caps = !active->_caps;
        return;
    }

    if (released) {
        return;
    }

    memset(&event, 0, sizeof(event));
    event.code = code;
    event.ascii = translate(active, code);
    event.pressed = 1;

    if (event.ascii) {
        push(event);
    }
}

static int poll_impl(struct keyboard *self, struct keyevent *out) {
    unused(self);

    if (!out || head == tail) {
        return 0;
    }

    *out = queue[tail];
    tail = (tail + 1u) % QUEUE;
    return 1;
}

static struct keyevent event(struct keyboard *self) {
    struct keyevent e;

    while (!poll_impl(self, &e)) {
        console_tick();
        schedpoll();
        __asm__ volatile ("hlt");
    }

    return e;
}

static void pend(const char *text) {
    pending_len = 0;
    pending_pos = 0;
    while (text[pending_len] && pending_len < sizeof(pending)) {
        pending[pending_len] = text[pending_len];
        pending_len++;
    }
}

static char read(struct keyboard *self) {
    for (;;) {
        struct keyevent e;

        if (pending_pos < pending_len) {
            return pending[pending_pos++];
        }
        pending_pos = 0;
        pending_len = 0;

        e = self->event(self);
        if (e.pressed && e.ascii) {
            return e.ascii;
        }
        if (e.pressed) {
            switch (e.code) {
                case KEY_LEFT: pend("\033[D"); break;
                case KEY_RIGHT: pend("\033[C"); break;
                case KEY_HOME: pend("\033[H"); break;
                case KEY_END: pend("\033[F"); break;
                case KEY_DELETE: pend("\033[3~"); break;
                case KEY_UP: pend("\033[A"); break;
                case KEY_DOWN: pend("\033[B"); break;
                default: break;
            }
        }
    }
}

void keyboard(struct keyboard *self) {
    memset(self, 0, sizeof(*self));
    self->event = event;
    self->poll = poll_impl;
    self->read = read;
    active = self;
    head = 0;
    tail = 0;
    pending_len = 0;
    pending_pos = 0;
    irq_register(IRQ_KEYBOARD, interruptkey);
}
