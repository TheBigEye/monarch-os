#ifndef _KERNEL_ATA_DRIVER_H
#define _KERNEL_ATA_DRIVER_H 1

#include "../../../common/common.h"

#define ATA_MASTER        0x00
#define ATA_SLAVE         0x01

#define ATA_PRIMARY       0x00
#define ATA_SECONDARY     0x01

#define ATA_PRIMARY_IO    0x1F0
#define ATA_SECUNDARY_IO  0x170

#define ATA_REG_SEC_CNT   0x02
#define ATA_REG_LBA_LOW   0x03
#define ATA_REG_LBA_MID   0x04
#define ATA_REG_LBA_UPR   0x05
#define ATA_REG_HDDEVSEL  0x06

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0

#define ATA_REG_STATUS    0x07
#define ATA_REG_COMMAND   0x07
#define ATA_CMD_IDENTIFY  0xEC

bool ataDeviceDetect(uint8_t bus, uint8_t drive);
bool ataDeviceIdentify(uint8_t bus, uint8_t drive, uint16_t *buffer);
void ataSectorRead(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t *buffer);
void ataSectorWrite(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t *buffer);

#endif
