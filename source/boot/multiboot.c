#include "boot/bootloader.h"

const struct mb2tag *mb2find(uintptr_t info, uint32_t type) {
    uint32_t total;
    uintptr_t offset;

    if (!info) {
        return nil;
    }

    total = *(uint32_t *)info;
    offset = 8;

    while (offset + sizeof(struct mb2tag) <= total) {
        const struct mb2tag *tag = (const struct mb2tag *)(info + offset);

        if (tag->type == MB2_TAG_END) {
            break;
        }
        if (tag->type == type) {
            return tag;
        }
        if (tag->size < sizeof(struct mb2tag)) {
            break;
        }

        offset += alignup(tag->size, 8);
    }

    return nil;
}
