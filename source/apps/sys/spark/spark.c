/**
 * @file spark.c
 * @brief SPARK session coordinator implementation.
 */
#include "spark.h"

static int prepare(Spark *self) {
    struct termios termios;
    if (!self || pty_pair(self->pty) != 0) {
        if (self) eputs("SPARK: cannot create desktop PTY\n");
        return 0;
    }
    if (ioctl(self->pty[1], TCGETS, &termios) == 0) {
        termios.lflag &= ~(TERMIOS_ECHO | TERMIOS_ICANON);
        ioctl(self->pty[1], TCSETS, &termios);
    }
    dup2(self->pty[0], SPARK_PTY_MASTER);
    dup2(self->pty[1], 0);
    dup2(self->pty[1], 1);
    dup2(self->pty[1], 2);
    if (self->pty[0] != SPARK_PTY_MASTER) close(self->pty[0]);
    if (self->pty[1] != 0 && self->pty[1] != 1 && self->pty[1] != 2)
        close(self->pty[1]);
    return 1;
}

static int start(Spark *self) {
    if (!self) return 0;
    self->wing_pid = spawn(SPARK_WING_PATH);
    if (self->wing_pid < 0) return 0;
    self->desk_pid = spawn(SPARK_DESK_PATH);
    if (self->desk_pid < 0) return 0;
    self->wush_pid = spawn(SPARK_WUSH_PATH);
    if (self->wush_pid < 0) return 0;
    self->shell_pid = spawn(SPARK_USH_PATH);
    return self->shell_pid >= 0;
}

static int wait_session(Spark *self) {
    int ignored;
    if (!self || waitpid((int)self->shell_pid, &self->status) < 0) return 0;
    if (waitpid((int)self->wush_pid, &ignored) < 0) return 0;
    if (waitpid((int)self->wing_pid, &ignored) < 0) return 0;
    if (waitpid((int)self->desk_pid, &ignored) < 0) return 0;
    return 1;
}

static int run(Spark *self) {
    if (!self) return 1;
    puts("SPARK: preparing graphical userspace session\n");
    if (!self->prepare(self) || !self->start(self)) return 1;
    close(SPARK_PTY_MASTER);
    close(0);
    close(1);
    close(2);
    return self->wait(self) ? self->status : 1;
}

void Spark_init(Spark *self) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->prepare = prepare;
    self->start = start;
    self->wait = wait_session;
    self->run = run;
}
