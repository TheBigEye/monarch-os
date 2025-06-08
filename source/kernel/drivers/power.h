#ifndef _KERNEL_POWER_H
#define _KERNEL_POWER_H 1

#include "../../common/common.h"

enum POWER_STATUS {
    POWER_HALT      = 0x00,
    POWER_REBOOT    = 0x01,
    POWER_SHUTDOWN  = 0x02
};

/**
 * Controls the power state of the system.
 *
 * @param operation The desired power operation (halt, reboot, shutdown).
 */
void powerControl(int operation);

#endif /* KERNEL_POWER_H */
