#ifndef _KERNEL_MEMORY_H
#define _KERNEL_MEMORY_H 1

#include "../../common/common.h"

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
void *memoryCopy(void *destination, const void *source, uint32_t nbytes);

/**
 * @brief Fast assembly version of memcpy.
 *
 * @param destination   Pointer to the destination memory block
 * @param source        Pointer to the source memory block
 * @param nbytes        Number of bytes to copy
 * @return              Pointer to the destination memory block
 */
void *fastMemoryCopy(void * restrict destination, const void * restrict source, uint32_t nbytes);

/**
 * @brief Fast assembly version of memcpy for 16-bit words (memcpyw).
 *
 * @param destination   Pointer to the destination memory block
 * @param source        Pointer to the source memory block
 * @param nbytes        Number of bytes to copy
 * @return              Pointer to the destination memory block
 */
void *fastWideMemoryCopy(void * restrict destination, const void * restrict source, uint32_t nbytes);

/**
 * @brief Super fast assembly version of memcpy!
 * @warning This may broke something if the destination or source are not aligned
 */
void *fastFastMemoryCopy(void * restrict destination, const void * restrict source, uint32_t nbytes);



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
void *memorySet(void *destination, uint8_t value, uint32_t length);

/**
 * @brief Fast assembly version of memset.
 *
 * @param destination   Pointer to the memory block to be set
 * @param value         Value to set each byte in the memory block to
 * @param length        Length of the memory block in bytes
 * @return              Pointer to the destination memory block
 */
void *fastMemorySet(void *destination, uint8_t value, uint32_t length);

/**
 * @brief Fast assembly version of memset for 16-bit words (memsetw).
 *
 * @param destination   Pointer to the memory block to be set
 * @param value         16-bit word value to set each word in the memory block to
 * @param length        Length of the memory block in bytes
 * @return              Pointer to the destination memory block
 */
void *fastWideMemorySet(void *destination, uint16_t value, uint32_t length);

/**
 * @brief Super fast assembly version of memset!
 * @warning Use this with caution, may broke something ...
 */
void *fastFastMemorySet(void *destination, uint8_t value, uint32_t length);



/**
 * Move a block of memory from the source to the destination. (memmove)
 *
 * @param destination   Pointer to the destination memory block
 * @param source    Pointer to the source memory block
 * @param length    Number of bytes to move
 * @return          Pointer to the destination memory block
 */
void *memoryMove(void *destination, const void *source, uint32_t length);


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
int memoryCompare(const void *a, const void *b, uint32_t count);

#endif /* KERNEL_MEMORY_H */
