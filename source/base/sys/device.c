/**
 * @file device.c
 * @brief Generic kernel-side device metadata helper.
 */

#include "base/sys/device.h"

void device(struct device *dev, const char *name, const char *kind, void *driver) {
    dev->name = name;
    dev->kind = kind;
    dev->driver = driver;
}
