/**
 * @file spark.c
 * @brief Userspace coordinator for one explicit graphical desktop session.
 *
 * SPARK is launched by the `wing` shell command. It owns the session lifetime:
 * when the session's USH exits, SPARK returns to its caller and the invoking
 * Bush or USH shell resumes.
 */
#include "base/usr/sys.h"
#include "base/win/terminal.h"

int main(int argc, char **argv) {
    long wing_pid;
    long desk_pid;
    long wush_pid;
    long shell_pid;
    int pty[2];
    int status = 0;
    unused(argc);
    unused(argv);

    puts("SPARK: preparing graphical userspace session\n");
    if (pty_pair(pty) == 0) {
        struct termios termios;
        if (ioctl(pty[1], TCGETS, &termios) == 0) {
            /* USH owns line editing and echo. The PTY delivers each key
               immediately instead of waiting for Enter or echoing twice. */
            termios.lflag &= ~(TERMIOS_ECHO | TERMIOS_ICANON);
            ioctl(pty[1], TCSETS, &termios);
        }
        dup2(pty[0], 3);
        dup2(pty[1], 0);
        dup2(pty[1], 1);
        dup2(pty[1], 2);
        if (pty[0] != 3) close(pty[0]);
        if (pty[1] != 0 && pty[1] != 1 && pty[1] != 2) close(pty[1]);
    } else {
        eputs("SPARK: cannot create desktop PTY\n");
        return 1;
    }

    wing_pid = spawn("/initrd/sys/bin/wing.elf");
    if (wing_pid < 0) { eputs("SPARK: unable to start Wing\n"); return 1; }
    desk_pid = spawn("/initrd/sys/bin/desk.elf");
    if (desk_pid < 0) { eputs("SPARK: unable to start Desk\n"); return 1; }
    wush_pid = spawn("/initrd/win/bin/wush.elf");
    if (wush_pid < 0) { eputs("SPARK: unable to start WUSH\n"); return 1; }
    shell_pid = spawn("/initrd/bin/ush.elf");
    if (shell_pid < 0) { eputs("SPARK: unable to start USH\n"); return 1; }

    /* Only WUSH should retain the master. SPARK's duplicate would keep the
       PTY alive after USH exits and prevent WUSH from seeing EOF. */
    close(3);
    /* SPARK also inherited the slave on stdio while preparing the session.
       Release those copies after all children have been spawned; otherwise
       USH can exit but the PTY never observes that its last slave is gone. */
    close(0);
    close(1);
    close(2);
    /* The graphical session ends when its interactive shell exits. WUSH sees
       PTY EOF and Wing/Desk close as part of the same session teardown. */
    if (waitpid((int)shell_pid, &status) < 0) return 1;
    /* Reap every service before SPARK exits. Otherwise the next `wing`
       invocation can inherit the previous session's zombie address spaces
       and exhaust the physical-page budget. The shutdown order follows the
       dependency graph: WUSH closes the PTY, Wing notices its client, and
       Desk notices Wing. */
    {
        int ignored;
        if (waitpid((int)wush_pid, &ignored) < 0) return 1;
        if (waitpid((int)wing_pid, &ignored) < 0) return 1;
        if (waitpid((int)desk_pid, &ignored) < 0) return 1;
    }
    return status;
}
