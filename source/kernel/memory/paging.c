#include "paging.h"

#include "../modules/terminal.h"

static uint32_t* page_directory = 0;
static uint32_t page_dir_loc = 0;
static uint32_t* last_page = 0;

/*
 * Paging now will be really simple
 * we reserve 0-16MB for kernel stuff
 * heap will be from approx 1mb to 16mb
 * and paging stuff will be from 16mb
 */

void initializePaging(void) {
    fprintf(serial, "[i] Setting up paging ...\n");
    page_directory = (uint32_t *) 0x800000;
    page_dir_loc = (uint32_t) page_directory;
    last_page = (uint32_t *) 0x808000;

    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0 | 2;
    }

    // Map first 16MB of memory
    memoryPagingMap(0x000000, 0x000000);  // 0-4MB
    memoryPagingMap(0x400000, 0x400000);  // 4-8MB
    memoryPagingMap(0x800000, 0x800000);  // 8-12MB
    memoryPagingMap(0xC00000, 0xC00000);  // 12-16MB

    // Enable paging
    ASM VOLATILE ("mov %%eax, %%cr3": :"a"(page_dir_loc));
    ASM VOLATILE ("mov %cr0, %eax");
    ASM VOLATILE ("orl $0x80000000, %eax");
    ASM VOLATILE ("mov %eax, %cr0");

    fprintf(serial, "[i] Paging was successfully enabled!\n\n");
}


void memoryPagingMap(uint32_t virtual, uint32_t physical) {
    uint16_t id = virtual >> 22;

    for (int i = 0; i < 1024; i++) {
        last_page[i] = physical | 3;
        physical += PAGE_SIZE;
    }

    page_directory[id] = ((uint32_t) last_page) | 3;
    last_page = (uint32_t *)(((uint32_t) last_page) + PAGE_SIZE);
    fprintf(serial, "[DEBUG] Mapping %#X (%d) to %#X\n", virtual, id, physical);
}
