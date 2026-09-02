#ifndef MONARCH_DRIVERS_BUS_PCI_H
#define MONARCH_DRIVERS_BUS_PCI_H 1

/**
 * @file pci.h
 * @brief Tiny PCI configuration-space scanner.
 *
 * This is intentionally small: enough to find simple legacy PCI devices such as
 * QEMU's AC'97 audio controller without pulling in a full PCI subsystem yet.
 */

#include "base/api/monarch.h"

#define PCI_ANY 0xFFu

struct pcidevice {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint8_t irq_line;
    uint32_t bar[6];
};

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);

int pci_read_device(uint8_t bus, uint8_t slot, uint8_t function, struct pcidevice *out);
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, struct pcidevice *out);
void pci_enable_io_busmaster(const struct pcidevice *device);

#endif /* MONARCH_DRIVERS_BUS_PCI_H */
