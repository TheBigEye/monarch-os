/**
 * @file fbrestore.c
 * @brief Restore the kernel framebuffer console after raw /dev/fb0 demos.
 *
 * Userspace framebuffer demos draw directly into the same linear framebuffer used
 * by the kernel console.  Until Monarch has virtual terminals or framebuffer
 * ownership, the simplest restore path is to ask the console device to clear
 * itself and move the cursor home using the tiny ANSI subset it already
 * understands.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    long fd;
    const char restore[] = "\x1b[2J\x1b[H";

    unused(argc);
    unused(argv);

    fd = open("/dev/console", OWRITE);
    if (fd < 0) {
        perror("fbrestore: open console");
        return 1;
    }

    if (write((int)fd, restore, sizeof(restore) - 1u) < 0) {
        perror("fbrestore: write console");
        close((int)fd);
        return 1;
    }

    close((int)fd);
    return 0;
}
