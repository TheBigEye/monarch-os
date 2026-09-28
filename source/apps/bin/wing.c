/**
 * @file wing.c
 * @brief Launch the graphical desktop session from a shell.
 *
 * This is deliberately a launcher, not the Window Server. The server remains
 * /initrd/sys/bin/wing.elf; this command starts SPARK, waits for the desktop
 * session to finish, and then returns to the shell that invoked it.
 */
#include "base/usr/sys.h"

int main(int argc, char **argv) {
    long pid;
    int status = 0;
    unused(argc);
    unused(argv);

    pid = spawn("/initrd/sys/bin/spark.elf");
    if (pid < 0) {
        eputs("wing: cannot start desktop session\n");
        return 1;
    }
    if (waitpid((int)pid, &status) < 0) {
        eputs("wing: desktop session wait failed\n");
        return 1;
    }
    /* Wing owns the visible framebuffer during the session. Restore a clean
       console before handing control back to Bush or the invoking USH. */
    puts("\033[2J\033[H\033[0m");
    return status;
}
