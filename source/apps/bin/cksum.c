/**
 * @file cksum.c
 * @brief Compute a simple FNV-1a checksum for files or stdin.
 */

#include "base/usr/sys.h"
#include "base/lib/hash.h"


static void puthex(uint32_t value) {
    const char *digits = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4) {
        putch(digits[(value >> shift) & 0xFu]);
    }
}

static int checksum(int fd, const char *name) {
    char buffer[256];
    uint32_t hash = FNV1A32_OFFSET;
    uint32_t total = 0;

    for (;;) {
        long count = read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            perror("cksum");
            return 1;
        }
        if (count == 0) {
            break;
        }
        hash = fnv1a32_update(hash, buffer, (size_t)count);
        total += (uint32_t)count;
    }

    puts(name);
    puts(" bytes=");
    putu(total);
    puts(" fnv32=");
    puthex(hash);
    putch('\n');
    return 0;
}

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        if (isatty(0)) {
            puts("usage: cksum [FILE...] or pipe input to cksum\n");
            return 1;
        }
        return checksum(0, "-");
    }

    for (int i = 1; i < argc; i++) {
        long fd = open(argv[i], OREAD);
        if (fd < 0) {
            perror("cksum");
            status = 1;
            continue;
        }
        if (checksum((int)fd, argv[i]) != 0) {
            status = 1;
        }
        close((int)fd);
    }

    return status;
}
