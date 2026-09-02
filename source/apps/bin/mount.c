/**
 * @file mount.c
 * @brief List mounts or mount a block device on a VFS path.
 */

#include "base/usr/sys.h"

#define MAX_MOUNTS 16u

static int list_mounts(void) {
    struct mountent entries[MAX_MOUNTS];
    long count = mounts(entries, MAX_MOUNTS);

    if (count < 0) {
        perror("mount");
        return 1;
    }

    for (long i = 0; i < count; i++) {
        puts(entries[i].type);
        puts(" on ");
        puts(entries[i].path);
        putch('\n');
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *type;
    const char *source;
    const char *target;

    if (argc == 1) {
        return list_mounts();
    }

    if (argc == 5 && strcmp(argv[1], "-t") == 0) {
        type = argv[2];
        source = argv[3];
        target = argv[4];
    } else if (argc == 4) {
        type = argv[1];
        source = argv[2];
        target = argv[3];
    } else {
        eputs("usage: mount\n");
        eputs("       mount -t TYPE SOURCE TARGET\n");
        eputs("       mount TYPE SOURCE TARGET\n");
        return 1;
    }

    if (mount(source, target, type) < 0) {
        perror("mount");
        return 1;
    }
    return 0;
}
