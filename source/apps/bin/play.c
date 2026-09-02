/**
 * @file play.c
 * @brief Decode and play a simple audio file through /dev/audio.
 */

#include "base/sfx/sound.h"

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        eputs("usage: play FILE...\n");
        eputs("formats: WAV PCM, AU linear PCM, MSFX\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (!sfx_play(argv[i])) {
            eputs("play: ");
            eputs(argv[i]);
            eputs(": ");
            eputs(sfx_error());
            eputch('\n');
            status = 1;
        }
    }

    return status;
}
