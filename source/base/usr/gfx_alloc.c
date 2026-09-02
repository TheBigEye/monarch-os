/**
 * @file gfx_alloc.c
 * @brief Userspace GFX allocator backed by the process heap.
 */

#include "base/gfx/allocator.h"
#include "base/usr/sys.h"

#define GFX_BLOCK_MAGIC 0x47584642u
#define GFX_SPLIT_MIN 16u

struct gfx_block {
    uint32_t magic;
    uint32_t free;
    size_t size;
    struct gfx_block *prev;
    struct gfx_block *next;
};

static struct gfx_block *blocks;
/* Cached last node of the block list. Kept in sync by split()/merge_next()/
   gfx_platform_alloc() so growing the heap never needs to walk the whole
   list just to find where to append. */
static struct gfx_block *list_tail;

static struct gfx_block *find_free(size_t size) {
    for (struct gfx_block *block = blocks; block; block = block->next)
        if (block->magic == GFX_BLOCK_MAGIC && block->free && block->size >= size)
            return block;
    return nil;
}

static void split(struct gfx_block *block, size_t size) {
    struct gfx_block *frag;
    if (!block || block->size < size + sizeof(*frag) + GFX_SPLIT_MIN) return;
    frag = (struct gfx_block *)((uint8_t *)(block + 1) + size);
    frag->magic = GFX_BLOCK_MAGIC;
    frag->free = 1;
    frag->size = block->size - size - sizeof(*frag);
    frag->prev = block;
    frag->next = block->next;
    if (frag->next) frag->next->prev = frag;
    else list_tail = frag; /* block used to be the last node; frag is now */
    block->next = frag;
    block->size = size;
}

static void merge_next(struct gfx_block *block) {
    struct gfx_block *next;
    if (!block || !(next = block->next) || !next->free) return;
    if ((uint8_t *)(block + 1) + block->size != (uint8_t *)next) return;
    block->size += sizeof(*next) + next->size;
    block->next = next->next;
    if (block->next) block->next->prev = block;
    else list_tail = block; /* next used to be the last node; block is now */
}

void *gfx_platform_alloc(size_t size) {
    struct gfx_block *block;
    size_t aligned;
    if (!size) return nil;
    aligned = (size + 15u) & ~(size_t)15u;
    block = find_free(aligned);
    if (block) {
        block->free = 0;
        split(block, aligned);
        return block + 1;
    }
    block = sbrk((long)(sizeof(*block) + aligned));
    if (block == (void *)-1) return nil;
    block->magic = GFX_BLOCK_MAGIC;
    block->free = 0;
    block->size = aligned;
    block->prev = list_tail;
    block->next = nil;
    if (list_tail) {
        list_tail->next = block;
    } else {
        blocks = block;
    }
    list_tail = block;
    return block + 1;
}

void *gfx_platform_zero(size_t count, size_t size) {
    size_t total = count * size;
    void *memory;

    /* Guard against a wraparound making `total` smaller than the caller
       actually asked for, which would hand back an undersized buffer. */
    if (size && total / size != count) {
        return nil;
    }

    memory = gfx_platform_alloc(total);
    if (memory) memset(memory, 0, total);
    return memory;
}

void gfx_platform_free(void *ptr) {
    struct gfx_block *block;
    if (!ptr) return;
    block = (struct gfx_block *)ptr - 1;
    if (block->magic != GFX_BLOCK_MAGIC || block->free) return;
    block->free = 1;
    if (block->prev && block->prev->free) {
        merge_next(block->prev);
        block = block->prev;
    }
    merge_next(block);
}
