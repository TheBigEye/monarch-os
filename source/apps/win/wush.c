/**
 * @file wush.c
 * @brief Windowed userspace shell client for Wing.
 */
#include "base/usr/sys.h"
#include "base/win/protocol.h"
#include "base/win/window.h"
#include "wush.h"

static int send_message(WinChannel *channel, WinMessage *message) {
    if (!channel || !channel->send(channel, message)) {
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

static int receive_message(WinChannel *channel, WinMessage *message) {
    struct pollfd item;
    item.fd = channel->fd; item.events = POLLIN; item.revents = 0;
    if (poll(&item, 1, 1000) != 1 || !(item.revents & POLLIN)) return 0;
    return channel->recv(channel, message);
}

static int key_to_pty(int fd, uint32_t key) {
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

int wush_run(void) {
    struct sockaddr_un address;
    WinMessage message;
    WinChannel channel;
    int wing;
    uint32_t handle;
    if (fcntl(3, F_GETFL, 0) < 0) return 1;
    wing = -1;
    memset(&address, 0, sizeof(address));
    address.family = AF_UNIX;
    strcpy(address.path, "/tmp/wing.sock");
    for (unsigned attempt = 0; attempt < 1000u; attempt++) {
        wing = (int)socket(AF_UNIX, SOCK_STREAM, 0);
        if (wing >= 0) {
            fcntl(wing, F_SETFL, O_NONBLOCK);
            if (connect(wing, &address) == 0) break;
            close(wing);
            wing = -1;
        }
        sleepms(10);
    }
    if (wing < 0) return 1;
    WinChannel_init(&channel, wing);

    channel.clear(&channel, &message, WIN_HELLO);
    message.request = WIN_REQUEST_HELLO;
    message.flags = WIN_CLIENT_TERMINAL;
    if (!send_message(&channel, &message) || !receive_message(&channel, &message)) return 1;
    fcntl(wing, F_SETFL, 0);

    channel.clear(&channel, &message, WIN_CREATE);
    message.request = WIN_REQUEST_FIRST;
    message.x = 100; message.y = 70; message.width = 600; message.height = 440;
    message.color = 0x164A82u;
    message.flags = WINDOW_NO_CLOSE | WINDOW_NO_MINIMIZE |
                   WINDOW_NO_MAXIMIZE | WINDOW_TERMINAL;
    strcpy(message.text, "User Space Shell");
    message.value = 1;
    if (!send_message(&channel, &message) || !receive_message(&channel, &message) ||
        message.opcode != WIN_ACK) return 1;
    handle = message.handle;

    fcntl(3, F_SETFL, O_NONBLOCK);
    fcntl(wing, F_SETFL, O_NONBLOCK);
    for (;;) {
        struct pollfd inputs[2];
        char data[WIN_PROTOCOL_TEXT];
        int pty_closed = 0;
        inputs[0].fd = 3; inputs[0].events = POLLIN; inputs[0].revents = 0;
        inputs[1].fd = wing; inputs[1].events = POLLIN; inputs[1].revents = 0;
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
                channel.clear(&channel, &message, WIN_TERMINAL_DATA);
                message.handle = handle;
                message.value = (uint32_t)used;
                memcpy(message.text, data, used);
                if (!send_message(&channel, &message)) goto done;
            }
        }
        /* The shell may legitimately terminate after `exit`. Do not keep
           forwarding later keyboard events to a PTY with no slave; that
           produces EIO and used to make WUSH appear to crash. */
        if (pty_closed) goto done;
        if (inputs[1].revents & POLLIN) {
            if (!channel.recv(&channel, &message)) break;
            if (message.opcode == WIN_EVENT && message.handle == handle) {
                if (message.event == WIN_EVENT_KEY) {
                    key_to_pty(3, message.value);
                } else if (message.event == WIN_EVENT_RESIZE) {
                    struct winsize size;
                    size.cols = (uint16_t)((message.width > 8u ? message.width - 8u : 8u) / 8u);
                    size.rows = (uint16_t)((message.height > 28u ? message.height - 28u : 28u) / 16u);
                    size.xpixel = 0;
                    size.ypixel = 0;
                    ioctl(3, TIOCSWINSZ, &size);
                }
            }
        }
    }
done:
    close(wing);
    return 0;
}

int main(int argc, char **argv) {
    unused(argc);
    unused(argv);
    return wush_run();
}
