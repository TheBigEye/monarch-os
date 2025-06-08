ENTRY(bootmain)
OUTPUT_FORMAT(elf32-i386)

phys = 0x00100000;

SECTIONS {

    /*
     * Start of kernel.
     */
    kernel_head = .;

    /*
     * Actual code
     */
    .text phys : AT(phys) {
        code = .;
        *(.text)
        *(.rodata)
        . = ALIGN(4096);
    }

    /*
     * Kernel data
     */
    .data : AT(phys + (data - code)) {
        data = .;
        *(.data)
        . = ALIGN(4096);
    }

    /*
     * Statically defined, uninitialized values
     */
    .bss : AT(phys + (bss - code)) {
        bss = .;
        *(.bss)
        . = ALIGN(4096);
    }

    /*
     * End of kernel.
     */
    kernel_tail = .;

    /*
     * End section
     */
    end = .;

    /*
     * Get rid of unnecessary GCC sections.
     */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
