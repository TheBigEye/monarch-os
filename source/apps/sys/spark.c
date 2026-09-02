/**
 * @file spark.c
 * @brief Initial userspace service coordinator for Monarch.
 *
 * SPARK is intentionally small while the service and window-server IPC
 * protocol is being introduced. It currently records startup stages and starts
 * Wing as a child process; device preparation and SHIFT fallback remain in
 * the kernel handoff layer.
 */
#include "base/usr/sys.h"
#include "base/win/terminal.h"

int main(int argc, char **argv) {
    long pid;
    int pty[2];
    unused(argc);
    unused(argv);
    puts("SPARK: System Process And Root Kernel-task\n");
    puts("SPARK: preparing userspace services\n");
    puts("SPARK: creating User Space Shell PTY\n");
    if (pty_pair(pty) == 0) {
        struct termios termios;
        if (ioctl(pty[1], TCGETS, &termios) == 0) {
            /* USH owns line editing and echo. The PTY must deliver each
               keystroke immediately instead of waiting for Enter. */
            termios.lflag &= ~(TERMIOS_ECHO | TERMIOS_ICANON);
            ioctl(pty[1], TCSETS, &termios);
        }
        dup2(pty[0], 3);
        dup2(pty[1], 0);
        dup2(pty[1], 1);
        dup2(pty[1], 2);
        if (pty[0] != 3) close(pty[0]);
        if (pty[1] != 0 && pty[1] != 1 && pty[1] != 2) close(pty[1]);
    }
    puts("SPARK: starting Wing window manager\n");
    pid = spawn("/initrd/sys/bin/wing.elf");
    if (pid < 0) { eputs("SPARK: unable to start Wing\n"); return 1; }
    pid = spawn("/initrd/sys/bin/desk.elf");
    if (pid < 0) { eputs("SPARK: unable to start Desk\n"); return 1; }
    pid = spawn("/initrd/win/bin/wush.elf");
    if (pid < 0) { eputs("SPARK: unable to start WUSH\n"); return 1; }
    pid = spawn("/initrd/bin/ush.elf");
    if (pid < 0) { eputs("SPARK: unable to start USH\n"); return 1; }
    pid = spawn("/initrd/win/bin/wasp.elf");
    if (pid < 0) { eputs("SPARK: unable to start WASP\n"); return 1; }
    for (;;) sleepms(1000);
}
