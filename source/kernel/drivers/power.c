#include "power.h"

#include "../CPU/HAL.h"
#include "../bugfault.h"

/**
 * Controls the power state of the system.
 *
 * @param status The desired power operation (halt, reboot, shutdown).
 */
void powerControl(int status) {
    OPERATION_WAIT

    switch (status) {
        case POWER_HALT:
            FOREVER ASM VOLATILE ("hlt"); // Halt the CPU
            break;

        case POWER_REBOOT:
            // Wait until the input buffer is empty
            while (readByteFromPort(0x64) & 0x02);

            writeByteToPort(0x64, 0xFE); // Send the reboot command
            FOREVER ASM VOLATILE ("hlt"); // Halt the CPU
            break;

        case POWER_SHUTDOWN:
            writeWordToPort(0xB004, 0x2000); // Bochs (BIOS)
            writeWordToPort(0x604, 0x2000); // QEMU (BIOS)
            writeWordToPort(0x4004, 0x3400); // VirtualBox (BIOS)

            FOREVER ASM VOLATILE ("hlt"); // Halt the CPU
            break;

        default:
            // Invalid operation, we return;
            BUG_CHECK(status > 2 || status < 0, "Invalid power operation!");
            break;
    }
}
