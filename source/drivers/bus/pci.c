#include "drivers/bus/pci.h"
#include "arch/x86/cpu.h"

#define PCI_CONFIG_ADDRESS 0xCF8u
#define PCI_CONFIG_DATA    0xCFCu
#define PCI_ENABLE         0x80000000u

static uint32_t address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    return PCI_ENABLE |
           ((uint32_t)bus << 16) |
           ((uint32_t)(slot & 0x1Fu) << 11) |
           ((uint32_t)(function & 0x07u) << 8) |
           (uint32_t)(offset & 0xFCu);
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, address(bus, slot, function, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read32(bus, slot, function, offset);
    return (uint16_t)(value >> ((offset & 2u) * 8u));
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read32(bus, slot, function, offset);
    return (uint8_t)(value >> ((offset & 3u) * 8u));
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, address(bus, slot, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

int pci_read_device(uint8_t bus, uint8_t slot, uint8_t function, struct pcidevice *out) {
    uint32_t id;

    if (!out) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    id = pci_read32(bus, slot, function, 0x00);
    if ((id & 0xFFFFu) == 0xFFFFu) {
        return 0;
    }

    out->bus = bus;
    out->slot = slot;
    out->function = function;
    out->vendor_id = (uint16_t)(id & 0xFFFFu);
    out->device_id = (uint16_t)(id >> 16);
    out->revision = pci_read8(bus, slot, function, 0x08);
    out->prog_if = pci_read8(bus, slot, function, 0x09);
    out->subclass = pci_read8(bus, slot, function, 0x0A);
    out->class_code = pci_read8(bus, slot, function, 0x0B);
    out->header_type = pci_read8(bus, slot, function, 0x0E);
    out->irq_line = pci_read8(bus, slot, function, 0x3C);

    for (unsigned i = 0; i < countof(out->bar); i++) {
        out->bar[i] = pci_read32(bus, slot, function, (uint8_t)(0x10u + i * 4u));
    }
    return 1;
}

int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, struct pcidevice *out) {
    for (uint16_t bus = 0; bus < 256u; bus++) {
        for (uint8_t slot = 0; slot < 32u; slot++) {
            uint8_t functions = 1;
            struct pcidevice device;

            if (!pci_read_device((uint8_t)bus, slot, 0, &device)) {
                continue;
            }
            if (device.header_type & 0x80u) {
                functions = 8;
            }

            for (uint8_t function = 0; function < functions; function++) {
                if (function != 0 && !pci_read_device((uint8_t)bus, slot, function, &device)) {
                    continue;
                }
                if (device.class_code == class_code &&
                    device.subclass == subclass &&
                    (prog_if == PCI_ANY || device.prog_if == prog_if)) {
                    if (out) {
                        *out = device;
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}

void pci_enable_io_busmaster(const struct pcidevice *device) {
    uint32_t command;

    if (!device) {
        return;
    }

    command = pci_read32(device->bus, device->slot, device->function, 0x04);
    command |= 0x00000005u; /* I/O space + bus master. */
    pci_write32(device->bus, device->slot, device->function, 0x04, command);
}
