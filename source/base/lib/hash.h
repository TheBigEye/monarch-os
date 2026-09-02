#ifndef MONARCH_BASE_LIB_HASH_H
#define MONARCH_BASE_LIB_HASH_H 1

/**
 * @file hash.h
 * @brief Tiny reusable non-cryptographic hash helpers.
 *
 * These helpers live in base/lib because they are pure freestanding C: the
 * kernel, userspace tools, and tests can all use the same implementation.  The
 * hashes here are for checksums, debug identifiers, and quick comparisons; they
 * are not security primitives.
 */

#include "base/api/monarch.h"

#define FNV1A32_OFFSET 2166136261u
#define FNV1A32_PRIME  16777619u

/** Continue a 32-bit FNV-1a hash with `size` bytes from `data`. */
uint32_t fnv1a32_update(uint32_t hash, const void *data, size_t size);

/** Compute a complete 32-bit FNV-1a hash using the standard offset basis. */
uint32_t fnv1a32(const void *data, size_t size);

#endif /* MONARCH_BASE_LIB_HASH_H */
