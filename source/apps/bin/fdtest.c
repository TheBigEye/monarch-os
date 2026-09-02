/**
 * @file fdtest.c
 * @brief Exercise dup, fcntl descriptor flags, shared file offsets and reads.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/initrd/etc/motd";
    long fd;
    long copy;
    char buffer[80];
    long count;

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("fdtest: open");
        return 1;
    }

    copy = dup((int)fd);
    if (copy < 0) {
        perror("fdtest: dup");
        close((int)fd);
        return 1;
    }

    puts("fdtest: fd=");
    puti((int32_t)fd);
    puts(" dup=");
    puti((int32_t)copy);
    putch('\n');

    if (fcntl((int)copy, F_SETFD, FD_CLOEXEC) < 0) {
        perror("fdtest: fcntl setfd");
    } else {
        puts("fdtest: dup fd flags=");
        puti((int32_t)fcntl((int)copy, F_GETFD, 0));
        putch('\n');
    }

    count = read((int)copy, buffer, sizeof(buffer) - 1u);
    if (count < 0) {
        perror("fdtest: read");
        close((int)copy);
        close((int)fd);
        return 1;
    }
    buffer[count] = '\0';
    puts("fdtest: first read bytes=");
    puti((int32_t)count);
    puts(" sample=");
    puts(buffer);
    if (count > 0 && buffer[count - 1] != '\n') {
        putch('\n');
    }

    close((int)copy);
    close((int)fd);
    return 0;
}
