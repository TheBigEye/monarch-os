#include "arch/x86/cpu.h"

uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" :: "a"(value), "Nd"(port));
}

void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" :: "a"(value), "Nd"(port));
}

void wait(void) {
    outb(0x80, 0);
}

void pause(void) {
    __asm__ volatile ("pause");
}

void enable(void) {
    __asm__ volatile ("sti");
}

void disable(void) {
    __asm__ volatile ("cli");
}

void halt(void) {
    disable();
    forever {
        __asm__ volatile ("hlt");
    }
}

void reboot(void) {
    disable();
    while (inb(0x64) & 0x02) {
        pause();
    }
    outb(0x64, 0xFE);
    halt();
}

void shutdown(void) {
    disable();
    outw(0xB004, 0x2000); /* Bochs */
    outw(0x0604, 0x2000); /* QEMU */
    outw(0x4004, 0x3400); /* VirtualBox */
    halt();
}

uint32_t rdtsc(void) {
    uint32_t lo;
    __asm__ volatile ("rdtsc" : "=a"(lo) :: "edx");
    return lo;
}
