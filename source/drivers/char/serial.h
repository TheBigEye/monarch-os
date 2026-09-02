#ifndef MONARCH_DRIVERS_CHAR_SERIAL_H
#define MONARCH_DRIVERS_CHAR_SERIAL_H 1

#include "base/api/monarch.h"

struct serial {
    void (*put)(struct serial *self, char ch);
    void (*write)(struct serial *self, const char *text);
    int (*printf)(struct serial *self, const char *fmt, ...);
    uint16_t _port;
};

void serial(struct serial *self, uint16_t port);

#endif /* MONARCH_DRIVERS_CHAR_SERIAL_H */
