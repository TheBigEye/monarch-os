/**
 * @file wush.c
 * @brief Windowed userspace shell client for Wing.
 */
#include "wush.h"
#include "base/win/window.h"

private int send_message(Wush *self) {
    if (!self->channel.send(&self->channel, &self->message)) {
        int serial = (int)open("/dev/serial", OWRITE);
        if (serial >= 0) {
            const char text[] = "WUSH: Wing socket write failed\n";
            write(serial, text, sizeof(text) - 1u);
            close(serial);
        }
        return 0;
    }
    return 1;
}

private int receive_message(Wush *self) {
    struct pollfd item;
    item.fd = self->channel.fd;
    item.events = POLLIN;
    item.revents = 0;
    if (poll(&item, 1, 1000) != 1 || !(item.revents & POLLIN)) return 0;
    return self->channel.recv(&self->channel, &self->message);
}

private int key_to_pty(int fd, uint32_t key) {
    static const char left[] = "\033[D";
    static const char right[] = "\033[C";
    static const char up[] = "\033[A";
    static const char down[] = "\033[B";
    const char *sequence = nil;
    uint8_t ascii;
    long result;
    if (key == 0x81u) sequence = left;
    else if (key == 0x82u) sequence = right;
    else if (key == 0x83u) sequence = up;
    else if (key == 0x84u) sequence = down;
    if (sequence) {
        result = write(fd, sequence, strlen(sequence));
        return result >= 0;
    }
    if (key <= 0xFFu) {
        ascii = (uint8_t)key;
        result = write(fd, &ascii, 1);
        return result >= 0;
    }
    return 1;
}

private int connect_wing(Wush *self) {
    memset(&self->address, 0, sizeof(self->address));
    self->address.family = AF_UNIX;
    strcpy(self->address.path, "/tmp/wing.sock");
    for (unsigned attempt = 0; attempt < 1000u; attempt++) {
        self->wing = (int)socket(AF_UNIX, SOCK_STREAM, 0);
        if (self->wing >= 0) {
            fcntl(self->wing, F_SETFL, O_NONBLOCK);
            if (connect(self->wing, &self->address) == 0) break;
            close(self->wing);
            self->wing = -1;
        }
        sleepms(10);
    }
    if (self->wing < 0) return 0;
    WinChannel_init(&self->channel, self->wing);
    self->channel.clear(&self->channel, &self->message, WIN_HELLO);
    self->message.request = WIN_REQUEST_HELLO;
    self->message.flags = WIN_CLIENT_TERMINAL;
    if (!send_message(self) || !receive_message(self)) return 0;
    fcntl(self->wing, F_SETFL, 0);
    return 1;
}

private int create_terminal(Wush *self) {
    self->channel.clear(&self->channel, &self->message, WIN_CREATE);
    self->message.request = WIN_REQUEST_FIRST;
    self->message.x = 100;
    self->message.y = 70;
    self->message.width = 600;
    self->message.height = 440;
    self->message.color = 0x164A82u;
    self->message.flags = WINDOW_NO_CLOSE | WINDOW_NO_MINIMIZE |
                          WINDOW_NO_MAXIMIZE | WINDOW_TERMINAL;
    strcpy(self->message.text, "User Space Shell");
    self->message.value = 1;
    if (!send_message(self) || !receive_message(self) ||
        self->message.opcode != WIN_ACK) return 0;
    self->handle = self->message.handle;
    return 1;
}

private int run(Wush *self) {
    if (fcntl(3, F_GETFL, 0) < 0) return 1;
    if (!self->connect(self) || !self->create(self)) return 1;
    close(0);
    close(1);
    close(2);
    fcntl(3, F_SETFL, O_NONBLOCK);
    fcntl(self->wing, F_SETFL, O_NONBLOCK);
    for (;;) {
        struct pollfd inputs[2];
        char data[WIN_PROTOCOL_TEXT];
        int pty_closed = 0;
        inputs[0].fd = 3; inputs[0].events = POLLIN; inputs[0].revents = 0;
        inputs[1].fd = self->wing; inputs[1].events = POLLIN; inputs[1].revents = 0;
        if (poll(inputs, 2, 20) < 0) break;
        if (inputs[0].revents & (POLLIN | POLLHUP)) {
            size_t used = 0;
            for (unsigned burst = 0; burst < 8u && used < sizeof(data); burst++) {
                struct pollfd ready;
                long count;
                ready.fd = 3; ready.events = POLLIN; ready.revents = 0;
                if (poll(&ready, 1, 0) <= 0 || !(ready.revents & POLLIN)) break;
                count = read(3, data + used, sizeof(data) - used);
                if (count == 0 || (count < 0 && errno != EAGAIN)) {
                    pty_closed = 1;
                    break;
                }
                if (count < 0) break;
                used += (size_t)count;
            }
            if (used) {
                self->channel.clear(&self->channel, &self->message, WIN_TERMINAL_DATA);
                self->message.handle = self->handle;
                self->message.value = (uint32_t)used;
                memcpy(self->message.text, data, used);
                if (!send_message(self)) break;
            }
        }
        if (pty_closed) {
            int serial = (int)open("/dev/serial", OWRITE);
            if (serial >= 0) { const char text[] = "WUSH: PTY EOF, closing session\n"; write(serial, text, sizeof(text) - 1u); close(serial); }
            break;
        }
        if (inputs[1].revents & POLLIN) {
            if (!self->channel.recv(&self->channel, &self->message)) break;
            if (self->message.opcode == WIN_EVENT && self->message.handle == self->handle) {
                if (self->message.event == WIN_EVENT_KEY)
                    key_to_pty(3, self->message.value);
                else if (self->message.event == WIN_EVENT_RESIZE) {
                    struct winsize size;
                    size.cols = (uint16_t)((self->message.width > 8u ? self->message.width - 8u : 8u) / 8u);
                    size.rows = (uint16_t)((self->message.height > 28u ? self->message.height - 28u : 28u) / 16u);
                    size.xpixel = 0; size.ypixel = 0;
                    ioctl(3, TIOCSWINSZ, &size);
                }
            }
        }
    }
    close(self->wing);
    return 0;
}

void Wush_init(Wush *self) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->wing = -1;
    self->connect = connect_wing;
    self->create = create_terminal;
    self->run = run;
}
