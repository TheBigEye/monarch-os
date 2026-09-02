/**
 * @file ipctest.c
 * @brief Userspace smoke tests for pipes and local stream sockets.
 */
#include "base/usr/sys.h"
#include "base/gfx/allocator.h"

static int check(int condition, const char *name) {
    if (!condition) {
        eputs("ipctest: FAIL: ");
        eputs(name);
        eputs("\n");
        return 0;
    }
    puts("ipctest: ok: ");
    puts(name);
    puts("\n");
    return 1;
}

int main(int argc, char **argv) {
    int pair[2];
    int pty[2];
    struct winsize pty_size;
    struct termios pty_termios;
    char buffer[32];
    struct pollfd item;
    fd_set readset;
    struct timeval zero;
    int fifo_read;
    int fifo_write;
    int server;
    int client;
    int accepted;
    struct sockaddr_un address;
    int passed = 1;
    void *memory_a;
    void *memory_b;
    void *memory_c;
    unused(argc);
    unused(argv);

    memory_a = gfx_alloc(128);
    memory_b = gfx_alloc(256);
    gfx_free(memory_a);
    memory_c = gfx_alloc(64);
    passed &= check(memory_a && memory_b && memory_c && memory_c == memory_a,
                    "dynamic allocator reuse");
    gfx_free(memory_b);
    gfx_free(memory_c);

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0) return 1;
    passed &= check(write(pair[0], "hello", 5) == 5, "socketpair write");
    item.fd = pair[1]; item.events = POLLIN; item.revents = 0;
    passed &= check(poll(&item, 1, 0) == 1 && (item.revents & POLLIN), "poll readable");
    FD_ZERO(&readset); FD_SET(pair[1], &readset);
    zero.seconds = 0; zero.microseconds = 0;
    passed &= check(select(pair[1] + 1, &readset, nil, nil, &zero) == 1 &&
                    FD_ISSET(pair[1], &readset), "select readable");
    passed &= check(read(pair[1], buffer, 5) == 5, "socketpair read");
    close(pair[0]); close(pair[1]);

    passed &= check(pty_pair(pty) == 0, "pty pair");
    fcntl(pty[1], F_SETFL, O_NONBLOCK);
    passed &= check(write(pty[0], "a", 1) == 1, "pty canonical input");
    passed &= check(read(pty[1], buffer, 1) < 0 && errno == EAGAIN,
                    "pty waits for newline");
    passed &= check(write(pty[0], "\n", 1) == 1 && read(pty[1], buffer, 2) == 2,
                    "pty canonical line");
    passed &= check(write(pty[1], "tty", 3) == 3, "pty write");
    passed &= check(read(pty[0], buffer, 3) == 3, "pty read");
    passed &= check(write(pty[0], "\004", 1) == 1 && read(pty[1], buffer, 1) == 0,
                    "pty ctrl-d eof");
    passed &= check(ioctl(pty[0], TIOCGWINSZ, &pty_size) == 0 &&
                    pty_size.rows == 25 && pty_size.cols == 80,
                    "pty default size");
    pty_size.rows = 40; pty_size.cols = 120;
    passed &= check(ioctl(pty[1], TIOCSWINSZ, &pty_size) == 0 &&
                    ioctl(pty[0], TIOCGWINSZ, &pty_size) == 0 &&
                    pty_size.rows == 40 && pty_size.cols == 120,
                    "pty resize");
    passed &= check(ioctl(pty[0], TCGETS, &pty_termios) == 0 &&
                    (pty_termios.lflag & TERMIOS_ECHO), "pty termios");
    pty_termios.lflag &= ~TERMIOS_ECHO;
    passed &= check(ioctl(pty[1], TCSETS, &pty_termios) == 0 &&
                    ioctl(pty[0], TCGETS, &pty_termios) == 0 &&
                    !(pty_termios.lflag & TERMIOS_ECHO), "pty termios update");
    close(pty[0]); close(pty[1]);

    passed &= check(mkfifo("/tmp/ipctest.fifo") == 0, "mkfifo");
    fifo_read = (int)open("/tmp/ipctest.fifo", OREAD | O_NONBLOCK);
    fifo_write = (int)open("/tmp/ipctest.fifo", OWRITE | O_NONBLOCK);
    passed &= check(fifo_read >= 0 && fifo_write >= 0, "open named fifo");
    passed &= check(read(fifo_read, buffer, 1) < 0 && errno == EAGAIN,
                    "fifo nonblock empty");
    passed &= check(write(fifo_write, "fifo", 4) == 4, "fifo write");
    passed &= check(read(fifo_read, buffer, 4) == 4, "fifo read");
    close(fifo_read); close(fifo_write); unlink("/tmp/ipctest.fifo");

    memset(&address, 0, sizeof(address));
    address.family = AF_UNIX;
    strcpy(address.path, "/tmp/ipctest.sock");
    server = (int)socket(AF_UNIX, SOCK_STREAM, 0);
    passed &= check(server >= 0 && bind(server, &address) == 0, "socket bind");
    passed &= check(listen(server, 2) == 0, "socket listen");
    fcntl(server, F_SETFL, O_NONBLOCK);
    passed &= check(accept(server) < 0 && errno == EAGAIN,
                    "accept nonblock empty");
    client = (int)socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(client, F_SETFL, O_NONBLOCK);
    passed &= check(client >= 0 && connect(client, &address) == 0, "socket connect");
    fcntl(server, F_SETFL, O_NONBLOCK);
    accepted = (int)accept(server);
    passed &= check(accepted >= 0, "socket accept");
    if (accepted >= 0) {
        passed &= check(write(client, "unix", 4) == 4, "unix write");
        passed &= check(read(accepted, buffer, 4) == 4, "unix read");
        close(accepted);
    }
    close(client);
    passed &= check(unlink("/tmp/ipctest.sock") == 0, "socket unlink");
    close(server);
    puts(passed ? "ipctest: PASS\n" : "ipctest: FAIL\n");
    return passed ? 0 : 1;
}
