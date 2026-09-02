/**
 * @file debug.h
 * @brief Public interface for Monarch serial debug logging.
 */

#ifndef MONARCH_KERNEL_CORE_DEBUG_H
#define MONARCH_KERNEL_CORE_DEBUG_H 1

#include "base/api/monarch.h"
#include "drivers/char/serial.h"

#ifndef MONARCH_DEBUG
#define MONARCH_DEBUG 0
#endif

#ifndef MONARCH_TRACE_SCHED
#define MONARCH_TRACE_SCHED 0
#endif

void debuginit(struct serial *serial);
void debuglog(const char *area, const char *fmt, ...);

#if MONARCH_DEBUG
#define KLOG(area, fmt, ...) debuglog((area), (fmt), ##__VA_ARGS__)
#else
#define KLOG(area, fmt, ...) do { } while (0)
#endif

#if MONARCH_DEBUG && MONARCH_TRACE_SCHED
#define KTRACE(area, fmt, ...) debuglog((area), (fmt), ##__VA_ARGS__)
#define KSCHED(fmt, ...) debuglog("sched", (fmt), ##__VA_ARGS__)
#else
#define KTRACE(area, fmt, ...) do { } while (0)
#define KSCHED(fmt, ...) do { } while (0)
#endif

#endif /* MONARCH_KERNEL_CORE_DEBUG_H */
