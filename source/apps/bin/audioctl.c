/**
 * @file audioctl.c
 * @brief Query /dev/audio format and ring-buffer state via ioctl.
 */

#include "base/usr/sys.h"

static const char *format_name(uint32_t format) {
    return format == AUDIO_FMT_S16LE ? "s16le" : "unknown";
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/dev/audio";
    struct audioinfo info;
    long fd;

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("audioctl: open");
        return 1;
    }

    if (ioctl((int)fd, AUDIO_GETINFO, &info) < 0) {
        perror("audioctl: ioctl");
        close((int)fd);
        return 1;
    }

    close((int)fd);

    puts(path);
    puts(" ready="); putu(info.ready);
    puts(" format="); puts(format_name(info.format));
    puts(" rate="); putu(info.sample_rate);
    puts(" channels="); putu(info.channels);
    puts(" bits="); putu(info.bits_per_sample);
    putch('\n');

    puts("buffer_bytes="); putu(info.buffer_bytes);
    puts(" queued="); putu(info.queued);
    puts(" running="); putu(info.running);
    puts(" completed="); putu(info.completed);
    puts(" underruns="); putu(info.underruns);
    puts(" errors="); putu(info.errors);
    putch('\n');

    return info.ready ? 0 : 1;
}
