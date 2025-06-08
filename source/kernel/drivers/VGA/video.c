#include "video.h"

#include "../../CPU/BIOS.h"
#include "../../CPU/HAL.h"
#include "../../memory/memory.h"


static uint32_t getSegment(void) {
    uint32_t segment;

    writeByteToPort(GRAPHICS_INDEX, 0x06);
    segment = readByteFromPort(GRAPHICS_DATA);

    segment >>= 2;
    segment &= 3;

    switch (segment) {
        case 0:
        case 1: segment = 0xA0000; break;
        case 2: segment = 0xB0000; break;
        case 3: segment = 0xB8000; break;
    }
    return segment;
}

static inline void writeRegister(uint16_t port, uint8_t reg, uint8_t val) {
    writeByteToPort(port, reg); /* Select register */
    writeByteToPort(port + 1, val); /* Do writing through data port */
}

static inline uint8_t readRegister(uint16_t port, uint8_t reg) {
    writeByteToPort(port, reg); /* Select register */
    return readByteFromPort(port + 1); /* Do reading through data port */
}

static inline void writeVideoMemory(uint32_t destOffset, uint8_t *source, uint32_t count) {
    fastMemoryCopy((void *)(getSegment() + destOffset), (void *) source, count);
}

static void setPlane(uint8_t plane) {
    uint8_t mask;

    // Ensures that 'plane' is in the range 0 to 3 ...
    // since there are 4 memory planes in VGA ...
    plane &= 3;
    mask = 1 << plane;

    /* Set read plane */
    writeByteToPort(GRAPHICS_INDEX, REG_GRAPHICS_MAP_READ);
    writeByteToPort(GRAPHICS_DATA, plane);

    /* Set write plane */
    writeByteToPort(SEQUENCER_INDEX, REG_SEQUENCER_MASK);
    writeByteToPort(SEQUENCER_DATA, mask);
}

static void writeColorPallete(uint8_t index, uint32_t RGB) {
    // Write the color index to the digital/analog controller port
    writeByteToPort(DIGANALOG_WRITE_INDEX, index);

    // For each color component (R, G, B)
    for (uint16_t i = 0; i < 3; i++) {
        // Shift the color bits and take only the last 6 bits
        uint8_t colorComponent = (RGB >> (i * 6)) & 0x3F;

        // Write the color component to port 0x3c9
        writeByteToPort(DIGANALOG_DATA, colorComponent);
    }
}


/// THESE ARE UNSUED FOR NOW ...
/*
static void vpokeb(unsigned off, char val) {
   char *pokebyte=(char *)(getSegment()+off);
   *pokebyte=val;
}

static char vpeekb(unsigned off) {
   char *pokebyte=(char *)(getSegment()+off);
    return (*pokebyte);
}
*/

static void writeFont(uint8_t *buffer, uint8_t fontHeight) {
    uint8_t seq2, seq4, gc4, gc5, gc6;

    // Save current state of SEQ and GC registers that will be modified
    writeByteToPort(SEQUENCER_INDEX, 2);
    seq2 = readByteFromPort(SEQUENCER_DATA);
    writeByteToPort(SEQUENCER_INDEX, 4);
    seq4 = readByteFromPort(SEQUENCER_DATA);

    // Turn off even-odd addressing to enable flat addressing mode
    // Assuming chain-4 addressing is already off
    writeByteToPort(SEQUENCER_DATA, seq4 | 0x04);

    writeByteToPort(GRAPHICS_INDEX, 4);
    gc4 = readByteFromPort(GRAPHICS_DATA);
    writeByteToPort(GRAPHICS_INDEX, 5);
    gc5 = readByteFromPort(GRAPHICS_DATA);

    // Disable even-odd addressing for graphics controller
    writeByteToPort(GRAPHICS_DATA, gc5 & ~0x10);
    writeByteToPort(GRAPHICS_INDEX, 6);
    gc6 = readByteFromPort(GRAPHICS_DATA);

    // Turn off even-odd addressing for memory mode register
    writeByteToPort(GRAPHICS_DATA, gc6 & ~0x02);

    // Set the plane to 2 for font writing
    setPlane(2);

    /* https://forum.osdev.org/viewtopic.php?p=160494 ¯\_(ツ)_/¯ */

    // Write the font data to video memory, one character at a time
    for (uint16_t i = 0; i < 256; i++) {

        /*
        * So.. what the FUCK is this!?
        *
        * - The value 16384 (or 0x4000 in hexadecimal) represents the size of the
        *   memory block in which VGA font bitmaps are stored. In VGA, each plane
        *   has a size of 64 KB (65536 bytes), and 16384 bytes is a quarter of that
        *   size, because fonts are stored in 16 KB segments.
        *
        * - In this context, 16384u * 0 is simply 0, but in the case of having more
        *   fonts, another multiplier could be used (such as 16384u * 1 for the next
        *   font).
        *
        * - The memory operation here directly overwrites the area where the BIOS
        *   stores a copy of its default font. By overwriting this memory region,
        *   we can change the font that is used by the system. Cool, right?
        *
        *  https://wiki.osdev.org/VGA_Hardware#Memory_Layout_in_text_modes
        */

        writeVideoMemory(16384U * 0 + i * 32, buffer, fontHeight);
        buffer += fontHeight;
    }

    // Restore the original state of SEQ and GC registers
    writeByteToPort(SEQUENCER_INDEX, 2);
    writeByteToPort(SEQUENCER_DATA, seq2);
    writeByteToPort(SEQUENCER_INDEX, 4);
    writeByteToPort(SEQUENCER_DATA, seq4);
    writeByteToPort(GRAPHICS_INDEX, 4);
    writeByteToPort(GRAPHICS_DATA, gc4);
    writeByteToPort(GRAPHICS_INDEX, 5);
    writeByteToPort(GRAPHICS_DATA, gc5);
    writeByteToPort(GRAPHICS_INDEX, 6);
    writeByteToPort(GRAPHICS_DATA, gc6);
}


void initializeVGA(uint8_t *registers) {
    /* We need clear the buffers always! */
    fastFastMemorySet((uint16_t *) GRAPHMODE_BUFFER, 0, GRAPHMODE_SIZE);
    fastFastMemorySet((uint16_t *) TEXTMODE_BUFFER, 0, TEXTMODE_SIZE);

    /* write MISCELLANEOUS register */
    writeByteToPort(MISCELLANEOUS_WRITE, *registers);
    registers++;

    /* write SEQUENCER registers */
    for (uint8_t i = 0; i < NUM_SEQUENCER_REGS; i++) {
        writeByteToPort(SEQUENCER_INDEX, i);
        writeByteToPort(SEQUENCER_DATA, *registers);
        registers++;
    }

    /* Unlock CRTC registers */
    writeByteToPort(CATHODERAY_INDEX, 0x03);
    writeByteToPort(CATHODERAY_DATA, readByteFromPort(CATHODERAY_DATA) | 0x80);
    writeByteToPort(CATHODERAY_INDEX, 0x11);
    writeByteToPort(CATHODERAY_DATA, readByteFromPort(CATHODERAY_DATA) & ~0x80);

    /* Make sure they remain unlocked */
    registers[0x03] |= 0x80;
    registers[0x11] &= ~0x80;

    /* Write CRTC registers */
    for (uint8_t i = 0; i < NUM_CATHODERAY_REGS; i++) {
        writeByteToPort(CATHODERAY_INDEX, i);
        writeByteToPort(CATHODERAY_DATA, *registers);
        registers++;
    }

    /* Write GRAPHICS CONTROLLER registers */
    for (uint8_t i = 0; i < NUM_GRAPHICS_REGS; i++) {
        writeByteToPort(GRAPHICS_INDEX, i);
        writeByteToPort(GRAPHICS_DATA, *registers);
        registers++;
    }

    /* Write ATTRIBUTE CONTROLLER registers */
    for (uint8_t i = 0; i < NUM_ATTRIBUTE_REGS; i++) {
        (void) readByteFromPort(INPUT_STATE_READ);
        writeByteToPort(ATTRIBUTE_INDEX, i);
        writeByteToPort(ATTRIBUTE_WRITE, *registers);
        registers++;
    }

    /*if (!isVideoMode) {
        for (uint8_t i = 0; i < 16; i++) {
            writeColorPallete(i, color_palette[i]);
        }
    }*/

    /* Lock 16-color palette and unblank display */
    (void) readByteFromPort(INPUT_STATE_READ);
    writeByteToPort(ATTRIBUTE_INDEX, 0x20);

    // If inst an pixel based mode ...
    if (!registers[62]) {
        writeFont(small_font, 8);
    }
}


enum video_type getVideoType(void) {
    return (enum video_type) (getBDA() & 0x30);
}
