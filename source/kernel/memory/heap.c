#include "heap.h"
#include "memory.h"

#include "../modules/terminal.h"
#include "../bugfault.h"

// Magic sentinel value to detect heap corruption
#define HEAP_MAGIC        0xC0DEFACE
#define BLOCK_FREE        0
#define BLOCK_ALLOCATED   1
#define ALIGNMENT_PADDING 4


// Memory layout statistics
static uint32_t last_alloc = 0;         // Points to the next allocation position
static uint32_t heap_end = 0;           // End of heap area
static uint32_t heap_begin = 0;         // Beginning of heap area
static uint32_t pheap_begin = 0;        // Start of page-aligned heap
static uint32_t pheap_end = 0;          // End of page-aligned heap
static uint8_t *pheap_desc = 0;         // Page allocation bitmap
static uint32_t memory_used = 0;        // Total memory in use
static uint32_t total_allocations = 0;  // Number of successful allocations
static uint32_t total_frees = 0;        // Number of successful frees


// Internal validation routine to check heap block integrity
static int validateBlock(alloc_t *block) {
    if (!block) return 0;

    // Check if block is within heap bounds
    if ((uint32_t)block < heap_begin || (uint32_t)block >= heap_end) {
        return 0;
    }

    // Check magic number for corruption
    if (block->magic != HEAP_MAGIC) {
        return 0;
    }

    return 1;
}


// Internal defragmentation to coalesce adjacent free blocks
static void defragmentHeap(void) {
    uint8_t *current = (uint8_t *)heap_begin;

    while ((uint32_t)current < last_alloc) {
        alloc_t *block = (alloc_t *)current;

        // Skip if not a valid block or is in use
        if (!validateBlock(block) || block->status == BLOCK_ALLOCATED) {
            current += block->size + sizeof(alloc_t) + ALIGNMENT_PADDING;
            continue;
        }

        // Try to merge with next block if it's free
        uint8_t *next_ptr = current + block->size + sizeof(alloc_t) + ALIGNMENT_PADDING;
        if ((uint32_t)next_ptr < last_alloc) {
            alloc_t *next_block = (alloc_t *) next_ptr;

            if (validateBlock(next_block) && next_block->status == BLOCK_FREE) {
                // Merge the blocks by increasing size and skipping the next header
                block->size += next_block->size + sizeof(alloc_t) + ALIGNMENT_PADDING;

                // Mark the merged block as invalid
                next_block->magic = 0;

                // Stay at current position to potentially merge more blocks
                continue;
            }
        }

        // Move to the next block
        current += block->size + sizeof(alloc_t) + ALIGNMENT_PADDING;
    }
}


void initializeMemory(uint32_t kernel_end) {
    // Validate kernel_end
    if (kernel_end == 0) {
        fprintf(serial, "[ERROR] Invalid 'kernel_end' value 0!\n");
        THROW("Invalid 'kernel_end' value!");
    }

    // Initialize heap area - aligned to 4KB after the kernel
    last_alloc = kernel_end + 0x1000;
    heap_begin = last_alloc;

    // Set page-aligned heap at 16MB mark
    pheap_end = 0x1000000;  // 16MB
    pheap_begin = pheap_end - (MAX_PAGE_ALIGNED_ALLOCS * 4096);
    heap_end = pheap_begin;

    // Zero out heap memory
    memorySet((char *) heap_begin, 0, heap_end - heap_begin);

    // Allocate a block for page allocation bitmap
    pheap_desc = (uint8_t *) memoryAllocateBlock(MAX_PAGE_ALIGNED_ALLOCS);
    if (!pheap_desc) {
        fprintf(serial, "[ERROR] Failed to allocate page descriptor array!\n");
        THROW("Failed to initialize page heap descriptor!");
    }

    // Zero out page allocation bitmap
    memorySet(pheap_desc, 0, MAX_PAGE_ALIGNED_ALLOCS);

    // Initialize statistics
    memory_used = 0;
    total_allocations = 0;
    total_frees = 0;

    fprintf(serial, "[i] Kernel heap initialized at %#X (%d bytes available)\n\n", last_alloc, heap_end - heap_begin);
}


void memoryGetStatus(void) {
    printl(INFO, "Memory Heap Status:\n");

    printf(" * Total alloc: %d\n", total_allocations);
    printf(" * Total frees: %d\n\n", total_frees);

    printf(" * Memory used: %d bytes\n", memory_used);
    printf(" * Memory free: %d bytes\n\n", heap_end - heap_begin - memory_used);

    printf(" * Memory-Heap size: %d bytes\n", heap_end - heap_begin);
    printf(" * Memory-Heap head: %#X\n", heap_begin);
    printf(" * Memory-Heap tail: %#X\n\n", heap_end);

    printf(" * Paging-Heap size: %d bytes\n", pheap_end - pheap_begin);
    printf(" * Paging-Heap head: %#X\n", pheap_begin);
    printf(" * Paging-Heap tail: %#X\n\n", pheap_end);
}


void memoryFreeBlock(void *mem) {
    // Validate pointer
    if (mem == NULL) {
        fprintf(serial, "[ERROR] Attempting to free NULL pointer-block!\n");
        return;
    }

    // Calculate header location
    alloc_t *alloc = ((alloc_t *)((uint8_t *) mem - sizeof(alloc_t)));

    // Validate the block
    if (!validateBlock(alloc)) {
        fprintf(serial, "[ERROR] Attempting to free invalid or corrupted block at %#X!\n", (uint32_t) mem);
        return;
    }

    // Check if block is already freed
    if (alloc->status == BLOCK_FREE) {
        fprintf(serial, "[ERROR] Double-free detected at %#X!\n", (uint32_t) mem);
        return;
    }

    // Bounds check the size
    if (alloc->size + sizeof(alloc_t) > memory_used) {
        fprintf(serial, "[ERROR] Attempting to free more memory [%d bytes] than allocated [%d bytes]\n", alloc->size + sizeof(alloc_t), memory_used);
        return;
    }

    // Mark block as free and update statistics
    memory_used -= (alloc->size + sizeof(alloc_t) + ALIGNMENT_PADDING);
    alloc->status = BLOCK_FREE;
    total_frees++;

    // Zero out memory to prevent information leaks and aid debugging
    memorySet((uint8_t*) mem, 0, alloc->size);

    // Periodically defragment the heap (every 10 frees)
    if (total_frees % 10 == 0) {
        defragmentHeap();
    }

    // fprintf(serial, "[DEBUG] Freed %d bytes at %#X\n", alloc->size, (uint32_t)mem);
}


void memoryFreePages(void *mem) {
    // Validate pointer
    if (mem == NULL) {
        fprintf(serial, "[ERROR] Attempting to free NULL pointer-page!\n");
        return;
    }

    // Validate address bounds
    if ((uint32_t) mem < pheap_begin || (uint32_t) mem >= pheap_end) {
        fprintf(serial, "[ERROR] Memory address %#X out of paging heap range! (%#X-%#X)\n", (uint32_t) mem, pheap_begin, pheap_end);
        return;
    }

    // Determine which page is it
    uint32_t pageIndex = ((uint32_t)mem - pheap_begin) / 4096;

    // Validate page index
    if (pageIndex >= MAX_PAGE_ALIGNED_ALLOCS) {
        fprintf(serial, "[ERROR] Page index %d out of range (0-%d)!\n", pageIndex, MAX_PAGE_ALIGNED_ALLOCS - 1);
        return;
    }

    // Check if page is already freed
    if (pheap_desc[pageIndex] == 0) {
        fprintf(serial, "[ERROR] Double-free of page at %#X (index %d)!\n", (uint32_t) mem, pageIndex);
        return;
    }

    // Free the page
    pheap_desc[pageIndex] = 0;
    // fprintf(serial, "[DEBUG] Page %d freed at %#X\n", pageIndex, (uint32_t)mem);

    // Zero out the page memory
    memorySet(mem, 0, 4096);
}


char* memoryAllocatePages(uint32_t size) {
    // Validate size
    if (size == 0) {
        fprintf(serial, "[ERROR] Attempting to allocate 0 pages!\n");
        return NULL;
    }

    // Find consecutive free pages
    uint32_t consecutiveFree = 0;
    uint32_t startPage = 0;

    for (uint32_t i = 0; i < MAX_PAGE_ALIGNED_ALLOCS; i++) {
        if (pheap_desc[i] == 0) {
            if (consecutiveFree == 0) {
                startPage = i;
            }
            consecutiveFree++;

            if (consecutiveFree >= size) {
                // Found enough consecutive pages
                for (uint32_t j = startPage; j < startPage + size; j++) {
                    pheap_desc[j] = 1;
                }

                void *allocatedMemory = (void*)(pheap_begin + startPage * 4096);

                // Zero out the memory
                memorySet(allocatedMemory, 0, size * 4096);

                //fprintf(serial, "[DEBUG] Allocated %d pages from %#X to %#X\n", size, (uint32_t)allocatedMemory, (uint32_t)allocatedMemory + (size * 4096) - 1);

                return allocatedMemory;
            }
        } else {
            // Reset counter when we hit an allocated page
            consecutiveFree = 0;
        }
    }

    fprintf(serial, "[ERROR] Failed to allocate %d pages - not enough consecutive free pages!\n", size);
    return NULL;
}


char* memoryAllocateBlock(uint32_t size) {
    // Validate size
    if (size == 0) {
        fprintf(serial, "[ERROR] Attempting to allocate a block of size 0!\n");
        return NULL;
    }

    // First attempt: search for a suitable free block
    uint8_t *mem = (uint8_t *) heap_begin;

    while ((uint32_t) mem < last_alloc) {
        alloc_t *block = (alloc_t *)mem;

        // If the block has no size, we've reached the end of allocation
        if (!validateBlock(block) || block->size == 0) {
            break;
        }

        // If the block is allocated, skip it
        if (block->status == BLOCK_ALLOCATED) {
            mem += block->size + sizeof(alloc_t) + ALIGNMENT_PADDING;
            continue;
        }

        // Found a free block, check if it's large enough
        if (block->size >= size) {
            // Check if we should split this block
            if (block->size >= size + sizeof(alloc_t) + ALIGNMENT_PADDING + 64) {  // 64 byte minimum for split block
                // Calculate the size of the remaining free block
                uint32_t remaining_size = block->size - size - sizeof(alloc_t) - ALIGNMENT_PADDING;

                // Create a new header for the split block
                alloc_t *split_block = (alloc_t *)(mem + sizeof(alloc_t) + size + ALIGNMENT_PADDING);
                split_block->magic = HEAP_MAGIC;
                split_block->status = BLOCK_FREE;
                split_block->size = remaining_size;

                // Adjust the size of the original block
                block->size = size;
            }

            // Mark the block as allocated
            block->status = BLOCK_ALLOCATED;
            block->magic = HEAP_MAGIC;

            // Update statistics
            memory_used += (block->size + sizeof(alloc_t) + ALIGNMENT_PADDING);
            total_allocations++;

            // Zero out the block for security
            memorySet(mem + sizeof(alloc_t), 0, size);

            //fprintf(serial, "[DEBUG] Re-allocated %d bytes at %#X\n", size, (uint32_t)(mem + sizeof(alloc_t)));

            return (char *)(mem + sizeof(alloc_t));
        }

        // Move to the next block
        mem += block->size + sizeof(alloc_t) + ALIGNMENT_PADDING;
    }

    // Second attempt: allocate a new block at the end of the heap

    // Check if we have enough space
    if ((last_alloc + size + sizeof(alloc_t) + ALIGNMENT_PADDING) >= heap_end) {
        fprintf(serial, "[ERROR] Out of memory: Cannot allocate %d bytes! (heap_end: %#X, last_alloc: %#X)\n", size, heap_end, last_alloc);
        return NULL;
    }

    // Set up the new block header
    alloc_t *new_block = (alloc_t *) last_alloc;
    new_block->magic = HEAP_MAGIC;
    new_block->status = BLOCK_ALLOCATED;
    new_block->size = size;

    // Calculate new allocation position
    uint32_t new_last_alloc = last_alloc + size + sizeof(alloc_t) + ALIGNMENT_PADDING;

    // Update allocation statistics
    memory_used += (size + sizeof(alloc_t) + ALIGNMENT_PADDING);
    total_allocations++;

    // Zero out the memory for security
    memorySet((char *)(last_alloc + sizeof(alloc_t)), 0, size);

    // Return the pointer to the allocated memory
    void *result = (void *)(last_alloc + sizeof(alloc_t));

    // Update the last allocation pointer
    last_alloc = new_last_alloc;

    //fprintf(serial, "[DEBUG] Allocated new %d bytes at %#X (ends at %#X)\n", size, (uint32_t)result, last_alloc);

    return (char *)result;
}
