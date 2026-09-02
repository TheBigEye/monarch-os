/**
 * @file tree.c
 * @brief Recursively print a small directory tree using getdents.
 */

#include "base/usr/sys.h"

#define MAX_ENTRIES 64u
#define MAX_PATH 256u
#define MAX_DEPTH 8u

static void indent(unsigned depth) {
    for (unsigned i = 0; i < depth; i++) {
        puts("  ");
    }
}

static void join(char *out, size_t size, const char *base, const char *name) {
    if (strcmp(base, "/") == 0) {
        snprintf(out, size, "/%s", name);
    } else {
        snprintf(out, size, "%s/%s", base, name);
    }
}

static int tree(const char *path, unsigned depth) {
    struct dirent entries[MAX_ENTRIES];
    long fd;
    long count;

    if (depth > MAX_DEPTH) {
        indent(depth);
        puts("...\n");
        return 0;
    }

    fd = open(path, OREAD);
    if (fd < 0) {
        indent(depth);
        puts(path);
        puts(": ");
        puts(strerror(errno));
        putch('\n');
        return 1;
    }

    count = getdents((int)fd, entries, MAX_ENTRIES);
    close((int)fd);
    if (count < 0) {
        indent(depth);
        puts(path);
        puts(": ");
        puts(strerror(errno));
        putch('\n');
        return 1;
    }

    for (long i = 0; i < count; i++) {
        char child[MAX_PATH];
        indent(depth);
        puts(entries[i].type == VFS_DIR ? "+ " : "- ");
        puts(entries[i].name);
        putch('\n');

        if (entries[i].type == VFS_DIR) {
            join(child, sizeof(child), path, entries[i].name);
            tree(child, depth + 1u);
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/";
    puts(path);
    putch('\n');
    return tree(path, 1u);
}
