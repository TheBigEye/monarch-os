/**
 * @file debug.c
 * @brief Conditional serial debug logger used by DEBUG=1 builds.
 *
 */

#include "kernel/core/debug.h"

static struct serial *debug_serial;

void debuginit(struct serial *serial) {
    debug_serial = serial;
#if MONARCH_DEBUG
    if (debug_serial) {
        debug_serial->write(debug_serial, "[debug] serial debug log enabled\n");
    }
#endif
}

void debuglog(const char *area, const char *fmt, ...) {
#if MONARCH_DEBUG
    va_list ap;

    if (!debug_serial) {
        return;
    }

    debug_serial->write(debug_serial, "[debug]");
    if (area && *area) {
        debug_serial->write(debug_serial, "[");
        debug_serial->write(debug_serial, area);
        debug_serial->write(debug_serial, "]");
    }
    debug_serial->write(debug_serial, " ");

    va_start(ap, fmt);
    kvformat((void (*)(void *, char))debug_serial->put, debug_serial, fmt, ap);
    va_end(ap);

    debug_serial->write(debug_serial, "\n");
#else
    unused(area);
    unused(fmt);
#endif
}
