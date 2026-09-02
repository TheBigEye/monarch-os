/**
 * @file audiotone.c
 * @brief Write a fixed-format PCM square-wave tone to /dev/audio.
 *
 * /dev/audio currently accepts signed 16-bit little-endian stereo PCM at
 * 48000 Hz.  This tiny utility proves the userspace write path before the SFX
 * library grows WAV and other decoders.
 */

#include "base/usr/sys.h"

#define RATE 48000u
#define FRAMES 1024u

static int16_t samples[FRAMES * 2u];

static void fill(uint32_t start, uint32_t frames, uint32_t frequency) {
    uint32_t period = frequency ? RATE / frequency : RATE / 440u;

    if (period < 2u) {
        period = 2u;
    }

    for (uint32_t i = 0; i < frames; i++) {
        uint32_t frame = start + i;
        int16_t sample = ((frame % period) < (period / 2u)) ? 9000 : -9000;
        samples[i * 2u + 0u] = sample;
        samples[i * 2u + 1u] = sample;
    }
}

int main(int argc, char **argv) {
    uint32_t frequency = 440;
    uint32_t duration = 250;
    uint32_t total_frames;
    uint32_t frame = 0;
    long fd;

    if (argc > 1) {
        frequency = (uint32_t)atoi(argv[1]);
    }
    if (argc > 2) {
        duration = (uint32_t)atoi(argv[2]);
    }
    if (!frequency) {
        frequency = 440;
    }
    if (!duration) {
        duration = 250;
    }

    fd = open("/dev/audio", OWRITE);
    if (fd < 0) {
        perror("audiotone: open");
        return 1;
    }

    total_frames = (RATE * duration) / 1000u;
    while (frame < total_frames) {
        uint32_t count = min(total_frames - frame, FRAMES);
        size_t bytes = (size_t)count * 2u * sizeof(int16_t);
        long wrote;

        fill(frame, count, frequency);
        wrote = write((int)fd, samples, bytes);
        if (wrote < 0) {
            perror("audiotone: write");
            close((int)fd);
            return 1;
        }
        if (wrote == 0) {
            break;
        }
        frame += (uint32_t)wrote / (2u * sizeof(int16_t));
    }

    close((int)fd);
    puts("audiotone: ok\n");
    return 0;
}
