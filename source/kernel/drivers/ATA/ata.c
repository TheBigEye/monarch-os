#include "ata.h"

#include "../../CPU/HAL.h"
#include "../../modules/terminal.h"


bool ataDeviceDetect(uint8_t bus, uint8_t drive) {
    uint8_t status;

    uint16_t io = (bus == ATA_PRIMARY) ? ATA_PRIMARY_IO : ATA_SECUNDARY_IO;
    writeByteToPort(io + ATA_REG_HDDEVSEL, 0xA0 | (drive << 4));
    OPERATION_WAIT

    status = readByteFromPort(io + ATA_REG_STATUS);
    if (status == 0xFF) {
        return false;
    }

    return true;
}


bool ataDeviceIdentify(uint8_t bus, uint8_t drive, uint16_t *buffer) {
    uint16_t io = (bus == ATA_PRIMARY) ? ATA_PRIMARY_IO : ATA_SECUNDARY_IO;

    writeByteToPort(io + ATA_REG_HDDEVSEL, 0xA0 | (drive << 4));
    OPERATION_WAIT

    writeByteToPort(io + ATA_REG_STATUS, ATA_CMD_IDENTIFY);
    OPERATION_WAIT

    if (readByteFromPort(io + ATA_REG_STATUS) == 0) {
        return false;
    }

    for (uint16_t i = 0; i < 256; i++) {
        buffer[i] = readWordFromPort(io);
    }

    return true;
}


void ataSectorRead(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t *buffer) {
    if (!buffer) {
        return;
    }

    uint16_t io = (bus == ATA_PRIMARY) ? ATA_PRIMARY_IO : ATA_SECUNDARY_IO;
    writeByteToPort(io + ATA_REG_HDDEVSEL, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));

    writeByteToPort(io + 1, 0x00);
    writeByteToPort(io + ATA_REG_SEC_CNT, 1); // 1 sector
    writeByteToPort(io + ATA_REG_LBA_LOW, (uint8_t)(lba));       // Sector number or LBA Low, most likely LBA Low
    writeByteToPort(io + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));  // Cyl Low number or LBA Mid
    writeByteToPort(io + ATA_REG_LBA_UPR, (uint8_t)(lba >> 16)); // Cyl High number or LBA High

    // We tell to the device to read
    writeByteToPort(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    while (!(readByteFromPort(io + ATA_REG_STATUS) & 0x08)) NOTHING

    for (uint16_t i = 0; i < 256; i++) {
        uint16_t data = readWordFromPort(io);
        buffer[i * 2] = data & 0xFF;
        buffer[i * 2 + 1] = (data >> 8) & 0xFF;
    }
}


void ataSectorWrite(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t *buffer) {
    if (!buffer) {
        return;
    }

    if (lba < 0x20 || lba >= 0x1000) {
        fprintf(serial, "FAIL: Attempting to write to a reserved/system sector.\n");
        return;
    }

    uint16_t io = (bus == ATA_PRIMARY) ? ATA_PRIMARY_IO : ATA_SECUNDARY_IO;
    writeByteToPort(io + ATA_REG_HDDEVSEL, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));

    writeByteToPort(io + 1, 0x00);
    writeByteToPort(io + ATA_REG_SEC_CNT, 1);
    writeByteToPort(io + ATA_REG_LBA_LOW, (uint8_t)(lba));
    writeByteToPort(io + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    writeByteToPort(io + ATA_REG_LBA_UPR, (uint8_t)(lba >> 16));

    // We tell to the device to write
    writeByteToPort(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    uint32_t timeout = 10000;
    while (!(readByteFromPort(io + ATA_REG_STATUS) & 0x08)) {
        if (--timeout == 0) {
            fprintf(serial, "DEBUG: ATA write timeout!\n");
            return;
        }
    }

    uint8_t status = readByteFromPort(io + ATA_REG_STATUS);
    if (status & 0x01) {
        fprintf(serial, "FAIL: ATA write failed (status error)\n");
        return;
    }

    for (uint16_t i = 0; i < 256; i++) {
        uint16_t data = (buffer[i * 2 + 1] << 8) | buffer[i * 2];
        writeWordToPort(io, data);
    }

    timeout = 10000;
    while (readByteFromPort(io + ATA_REG_STATUS) & 0x80) {
        if (--timeout == 0) {
            status = readByteFromPort(io + ATA_REG_STATUS);

            if (status & 0x01) {
                fprintf(serial, "FAIL: ATA write failed (status error)\n");
            } else if (status & 0x02) {
                fprintf(serial, "FAIL: ATA write failed (drive is busy)\n");
            } else if (status & 0x20) {
                fprintf(serial, "FAIL: ATA write failed (write fault)\n");
            } else if (status & 0x40) {
                fprintf(serial, "FAIL: ATA write failed (seek error)\n");
            } else {
                fprintf(serial, "FAIL: ATA write timeout after writing data\n");
            }

            return;
        }
    }

    status = readByteFromPort(io + ATA_REG_STATUS);
    if (status & 0x01) {
        fprintf(serial, "FAIL: ATA write failed (post-write error)\n");
    }
}
