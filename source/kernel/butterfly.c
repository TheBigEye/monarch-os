#include "butterfly.h"

extern uint32_t kernel_head; /* The kernel start, 'kernel_head' section */
extern uint32_t kernel_tail; /* The kernel end, 'kernel_tail' section */

static multiboot_info_t butterfly_info;

extern void main(void);

/* Kernel entry point */
void butterfly(multiboot_info_t* multiboot_header, uint32_t boot_magic) {

    // Check if the bootloader is multiboot compliant
    if (boot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        THROW("Bootloader is not multiboot compliant!");

        /**
         * For now, no driver or kernel section actually uses
         * the mutiboot specification, so we're not going to
         * put a DEAD-END here
        */

        // FOREVER NOTHING // <- dead end
    }

    // Save the multiboot information for later :D
    butterfly_info = *multiboot_header;

    /* Get modules */
    // uint32_t initrd_pos = *((uint32_t*) multiboot_header->mods_addr);
    // uint32_t initrd_end = *((uint32_t*)(multiboot_header->mods_addr + 4));

    // Initialize debug serial port
    initializeCOM();

    // We init the screen in text mode
    initializeVGA(text_mode);

    // Clear the screen's trash
    tty_clear(NULL);

    initializeGDT(); // Install the General Descriptor Table
    initializeISR(); // Intall exception handlers
    initializeIRQ(); // Install drivers

    // Clear debug messages ...
    tty_clear(NULL);

    /* Initialize heap and paging */
    initializeMemory(&kernel_tail);
    initializePaging();

    /* ........ */

    tty_clear(NULL);

    /** NOTE: We need initialize the Heap before the File System */
    mountFileSystem();

    // We show a nice welcome screen fisrt ...
    initializeVGA(video_mode);
    gfx_clear(PX_BLACK);

    bglPlayWork();

    timerSleep(1500);
    gfx_clear(PX_BLACK);

    initializeVGA(text_mode);

    tty_clear(NULL);
    tty_cursor(0x3F, -1, -1);

    tty_plots(butterfly_logo, 0, 1, (BG_BLACK | FG_CYAN));
    startupSound();

    tty_plots(" Monarch OS - Under development ", 28, 20, (BG_BLACK | FG_DKGRAY));
    tty_plots(" (C) 2022-2025 ", 36, 21, (BG_BLACK | FG_DKGRAY));

    tty_prints("\n\n");

    memoryGetStatus();
    //dumpMultiboot();

    // We use the PIT and CPU ticks like the RNG seed
    rng_seed(timerGetTicks() + processorGetTicks());

    main();
}


void dumpMultiboot(void) {
    printl(INFO, "Multiboot Headers:\n");
    printf(" * Mutiboot flags: %#X\n", butterfly_info.flags);
    printf(" * Lower memory: %d KB\n", butterfly_info.mem_lower);
    printf(" * Upper memory: %d KB\n\n", butterfly_info.mem_upper);

    printf(" * VGA type: %#X\n\n", getVideoType());

    printf(" * Boot device: %#X\n", butterfly_info.boot_device);
    printf(" * Command line: %s\n", (char *) butterfly_info.cmdline);
    printf(" * Modules count: %#X\n", butterfly_info.mods_count);
    printf(" * Modules address: %#X\n\n", butterfly_info.mods_addr);

    printf(" * Memory map length: %d\n", butterfly_info.mmap_length);
    printf(" * Memory map address: %#X\n", butterfly_info.mmap_addr);
    printf(" * Drives length: %d\n", butterfly_info.drives_length);
    printf(" * Drives address: %#X\n\n", butterfly_info.drives_addr);

    printf(" * Config table: %#X\n", butterfly_info.config_table);
    printf(" * Booted from: %s\n", (char *) butterfly_info.boot_loader_name);
    printf(" * APM table: %#X\n\n", butterfly_info.apm_table);
}
