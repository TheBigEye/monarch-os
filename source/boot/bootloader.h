#ifndef MONARCH_BOOT_BOOTLOADER_H
#define MONARCH_BOOT_BOOTLOADER_H 1

#include "base/api/monarch.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289u

#define MB2_TAG_END 0u
#define MB2_TAG_CMDLINE 1u
#define MB2_TAG_BOOTLOADER 2u
#define MB2_TAG_MODULE 3u
#define MB2_TAG_BASIC_MEMORY 4u
#define MB2_TAG_BOOT_DEVICE 5u
#define MB2_TAG_MMAP 6u
#define MB2_TAG_FRAMEBUFFER 8u

#define MB2_MEMORY_AVAILABLE 1u
#define MB2_MEMORY_RESERVED 2u
#define MB2_MEMORY_ACPI_RECLAIMABLE 3u
#define MB2_MEMORY_NVS 4u
#define MB2_MEMORY_BADRAM 5u

struct bootinfo {
    uint32_t magic;
    uintptr_t address;
};

struct mb2tag {
    uint32_t type;
    uint32_t size;
};

struct mb2mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct mb2mmapentry {
    uint64_t address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};

struct mb2module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[];
};

struct mb2framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
    uint8_t red_field_position;
    uint8_t red_mask_size;
    uint8_t green_field_position;
    uint8_t green_mask_size;
    uint8_t blue_field_position;
    uint8_t blue_mask_size;
};

const struct mb2tag *mb2find(uintptr_t info, uint32_t type);

#endif /* MONARCH_BOOT_BOOTLOADER_H */
