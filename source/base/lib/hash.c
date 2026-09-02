/**
 * @file hash.c
 * @brief FNV-1a helpers for checksums and small debug identifiers.
 */

#include "base/lib/hash.h"

uint32_t fnv1a32_update(uint32_t hash, const void *data, size_t size) {
    const uint8_t *bytes = data;

    if (!bytes && size) {
        return hash;
    }

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV1A32_PRIME;
    }
    return hash;
}

uint32_t fnv1a32(const void *data, size_t size) {
    return fnv1a32_update(FNV1A32_OFFSET, data, size);
}
