#ifndef _KERNEL_HEAP_H
#define _KERNEL_HEAP_H 1

#include "../../common/common.h"

#define MAX_PAGE_ALIGNED_ALLOCS 32

typedef struct {
    uint32_t magic;   // Magic number to detect corruption
    uint8_t status;   // 0 = free, 1 = allocated
    uint32_t size;    // Size of the data portion (not including this header)
} alloc_t;


/**
 * Initialize the memory subsystem
 *
 * @param kernel_end Physical address where the kernel ends
 */
void initializeMemory(uint32_t kernel_end);


/**
 * Display memory allocation statistics
 */
void memoryGetStatus(void);


/**
 * Allocate page-aligned memory
 *
 * @param size Number of pages to allocate (each page is 4096 bytes)
 * @return Pointer to the allocated memory, or NULL on failure
 */
char* memoryAllocatePages(uint32_t size);


/**
 * Allocate a block of memory
 *
 * @param size Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL on failure
 */
char* memoryAllocateBlock(uint32_t size);


/**
 * Free a block of memory previously allocated with memoryAllocateBlock
 *
 * @param mem Pointer to the memory block to free
 */
void memoryFreeBlock(void *mem);


/**
 * Free page-aligned memory previously allocated with memoryAllocatePages
 *
 * @param mem Pointer to the page-aligned memory to free
 */
void memoryFreePages(void *mem);


#endif /* _KERNEL_HEAP_H */
