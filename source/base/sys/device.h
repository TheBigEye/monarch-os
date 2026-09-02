#ifndef MONARCH_BASE_SYS_DEVICE_H
#define MONARCH_BASE_SYS_DEVICE_H 1

#ifndef MONARCH_KERNEL_BUILD
#error "base/sys is kernel-only; userspace must use base/usr or base/api instead."
#endif

/**
 * @file device.h
 * @brief Minimal device metadata object.
 *
 * `struct device` is deliberately tiny.  It does not implement I/O by itself;
 * it simply describes a driver-owned object with a human-readable name and a
 * category string.  This is useful when building registries or debug listings
 * without forcing every caller to know the concrete driver type.
 */

#include "base/api/monarch.h"

/** Generic descriptor for a hardware or virtual device. */
struct device {
    /** Human-readable device name, e.g. "console0" or "ram0". */
    const char *name;
    /** Broad class/kind, e.g. "char", "block", "video". */
    const char *kind;
    /** Opaque pointer to the actual driver object. */
    void *driver;
};

/** Initialise a generic device descriptor. */
void device(struct device *dev, const char *name, const char *kind, void *driver);

#endif /* MONARCH_BASE_SYS_DEVICE_H */
