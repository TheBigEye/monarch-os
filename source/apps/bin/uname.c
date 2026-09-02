/**
 * @file uname.c
 * @brief Print kernel/system identity returned by the uname syscall.
 */

#include "base/usr/sys.h"

static void field(const char *text, int *first) {
    if (!*first) {
        putch(' ');
    }
    puts(text);
    *first = 0;
}

int main(int argc, char **argv) {
    struct utsname name;
    int all = 0;
    int show_sys = 0;
    int show_node = 0;
    int show_release = 0;
    int show_version = 0;
    int show_machine = 0;
    int first = 1;

    if (uname(&name) < 0) {
        perror("uname");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            all = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            show_sys = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            show_node = 1;
        } else if (strcmp(argv[i], "-r") == 0) {
            show_release = 1;
        } else if (strcmp(argv[i], "-v") == 0) {
            show_version = 1;
        } else if (strcmp(argv[i], "-m") == 0) {
            show_machine = 1;
        } else {
            eputs("usage: uname [-a|-s|-n|-r|-v|-m]\n");
            return 1;
        }
    }

    if (argc == 1) {
        show_sys = 1;
    }
    if (all) {
        show_sys = show_node = show_release = show_version = show_machine = 1;
    }

    if (show_sys) field(name.sysname, &first);
    if (show_node) field(name.nodename, &first);
    if (show_release) field(name.release, &first);
    if (show_version) field(name.version, &first);
    if (show_machine) field(name.machine, &first);
    putch('\n');
    return 0;
}
