#include "memory.h"

/**
 * Copy a block of memory from the source to the destination (similar to memcpy).
 *
 * @param destination   Pointer to the destination memory block
 * @param source        Pointer to the source memory block
 * @param nbytes        Number of bytes to copy
 * @return              Pointer to the destination memory block
 *
 * @note This implementation is safe to use for simple operations, but
 *       it is very slow if used intensively
 */
inline void *memoryCopy(void *destination, const void *source, uint32_t nbytes) {
    if (!destination || !source)
        return NULL;

    if (nbytes == 0)
        return destination;

    uint8_t *dst = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;

    if (dst > src && dst < src + nbytes) {
        dst += nbytes;
        src += nbytes;
        do {
            *--dst = *--src;
        } while (--nbytes);
    } else {
        do {
            *dst++ = *src++;
        } while (--nbytes);
    }
    return destination;
}


/**
 * @brief Fast assembly version of memcpy.
 *
 * @param destination   Pointer to the destination memory block
 * @param source        Pointer to the source memory block
 * @param nbytes        Number of bytes to copy
 * @return              Pointer to the destination memory block
 */
inline void *fastMemoryCopy(void * restrict destination, const void * restrict source, uint32_t nbytes) {
    register int d0, d1, d2;
    ASM VOLATILE (
        "rep\n\t"
        "movsb"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        :"0" (nbytes), "1" (destination), "2" (source)
        :"memory"
    );
    return destination;
}

/**
 * @brief Fast assembly version of memcpy for 16-bit words (memcpyw).
 *
 * @param destination   Pointer to the destination memory block
 * @param source        Pointer to the source memory block
 * @param nbytes        Number of bytes to copy
 * @return              Pointer to the destination memory block
 */
inline void *fastWideMemoryCopy(void * restrict destination, const void * restrict source, uint32_t nbytes) {
    register int d0, d1, d2;
    ASM VOLATILE (
        "rep\n\t"
        "movsw"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (nbytes), "1" (destination), "2" (source)
        : "memory"
    );
    return destination;
}

/**
 * @brief Super fast assembly version of memcpy!
 * @warning This may broke something if the destination or source are not aligned
 */
inline void *fastFastMemoryCopy(void * restrict destination, const void * restrict source, uint32_t nbytes) {
    register int d0, d1, d2;
    ASM VOLATILE (
        "movl %%ecx, %%edx\n\t" // Save nbytes in edx for later
        "shrl $2, %%ecx\n\t"    // Divide nbytes by 4 to copy dwords
        "rep movsl\n\t"         // Copy dwords
        "movl %%edx, %%ecx\n\t" // Restore nbytes
        "andl $3, %%ecx\n\t"    // Calculate remaining bytes after copying dwords
        "rep movsb"             // Copy remaining bytes
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (nbytes), "1" ((long)destination), "2" ((long)source)
        : "memory", "edx"
    );
    return destination;
}



/**
 * Set a block of memory with a specified value (similar to memset).
 *
 * @param destination   Pointer to the memory block to be set
 * @param value         Value to set each byte in the memory block to
 * @param length        Length of the memory block in bytes
 * @return              Pointer to the destination memory block
 *
 * @note This implementation is safe to use for simple operations, but
 *       it is very slow if used intensively
 */
inline void *memorySet(void *destination, uint8_t value, uint32_t length) {
    uint8_t *pointer = (uint8_t *) destination;
    while (length--) *pointer++ = value;
    return destination;
}

/**
 * @brief Fast assembly version of memset.
 *
 * @param destination   Pointer to the memory block to be set
 * @param value         Value to set each byte in the memory block to
 * @param length        Length of the memory block in bytes
 * @return              Pointer to the destination memory block
 */
inline void *fastMemorySet(void *destination, uint8_t value, uint32_t length) {
    register int d0, d1;
    ASM VOLATILE (
        "rep\n\t"
        "stosb"
        : "=&c" (d0), "=&D" (d1)
        :"a" (value),"1" (destination),"0" (length)
        :"memory"
    );
    return destination;
}

/**
 * @brief Fast assembly version of memset for 16-bit words (memsetw).
 *
 * @param destination   Pointer to the memory block to be set
 * @param value         16-bit word value to set each word in the memory block to
 * @param length        Length of the memory block in bytes
 * @return              Pointer to the destination memory block
 */
inline void *fastWideMemorySet(void *destination, uint16_t value, uint32_t length) {
    register int d0, d1;
    ASM VOLATILE (
        "rep\n\t"
        "stosw"
        : "=&c" (d0), "=&D" (d1)
        : "a" (value), "1" (destination), "0" (length)
        : "memory"
    );
    return destination;
}

/**
 * @brief Super fast assembly version of memset!
 * @warning Use this with caution, may break something ...
 */
inline void *fastFastMemorySet(void *destination, uint8_t value, uint32_t length) {
    register int d0, d1, d2;
    uint32_t val4 = value | (value << 8) | (value << 16) | (value << 24); // Prepare the 4-byte value
    ASM VOLATILE (
        "movl %%ecx, %%edx\n\t" // Save length in edx for later
        "shrl $2, %%ecx\n\t"    // Divide length by 4 to set dwords
        "rep stosl\n\t"         // Set dwords
        "movl %%edx, %%ecx\n\t" // Restore length
        "andl $3, %%ecx\n\t"    // Calculate the remaining bytes after setting dwords
        "rep stosb"             // Set the remaining bytes
        : "=&c" (d0), "=&D" (d1), "=&a" (d2)
        : "1" ((long)destination), "0" (length), "2" (val4)
        : "memory", "edx"
    );
    return destination;
}



/**
 * Move a block of memory from the source to the destination. (memmove)
 *
 * @param destination   Pointer to the destination memory block
 * @param source    Pointer to the source memory block
 * @param length    Number of bytes to move
 * @return          Pointer to the destination memory block
 */
void *memoryMove(void *destination, const void *source, uint32_t length) {
    uint8_t *dst = (uint8_t *) destination;
    uint8_t *src = (uint8_t *) source;

    if (dst < src) {
        while (length--) *dst++ = *src++;
    } else {
        uint8_t *ldst = dst + (length - 1);
        uint8_t *lsrc = src + (length - 1);
        while (length--) *ldst-- = *lsrc--;
    }
    return destination;
}


/**
 * Compare two blocks of memory. (memcmp)
 *
 * @param a       Pointer to the first memory block
 * @param b       Pointer to the second memory block
 * @param count   Number of bytes to compare
 * @return        0 if the two blocks are identical, -1 if the
 *                first differing byte in a is less than the
 *                corresponding byte in b, 1 if the first differing
 *                byte in a is greater than the corresponding byte in b
 */
int memoryCompare(const void *a, const void *b, uint32_t count) {
    const uint8_t *sa = (const uint8_t *) a;
    const uint8_t *sb = (const uint8_t *) b;

    while (count--) {
        if (*sa++ != *sb++) {
            return sa[-1] < sb[-1] ? -1 : 1;
        }
    }
    return 0;
}
