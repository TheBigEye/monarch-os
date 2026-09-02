#ifndef MONARCH_BASE_SFX_SOUND_H
#define MONARCH_BASE_SFX_SOUND_H 1

/**
 * @file sound.h
 * @brief Tiny userspace sound decoding/playback helpers.
 *
 * The SFX layer sits above `/dev/audio`.  The device currently accepts one fixed
 * PCM format: signed 16-bit little-endian stereo at 48000 Hz.  These helpers
 * decode a few simple uncompressed file containers into that device format and
 * include a tiny nearest-neighbour resampler for common source rates.
 *
 * This is deliberately not a full codec framework.  It is the audio equivalent
 * of Monarch's early framebuffer FBD layer: small, direct and educational.
 */

#include "base/usr/sys.h"

#ifndef MONARCH_USER_BUILD
#error "base/sfx is userspace-only for now; kernel code should use drivers/sound directly."
#endif

#define SFX_RATE 48000u
#define SFX_CHANNELS 2u
#define SFX_BITS 16u

enum sfx_format {
    SFX_UNKNOWN = 0,
    SFX_WAV = 1,
    SFX_AU = 2,
    SFX_MSFX = 3
};

struct sfx_info {
    enum sfx_format format;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint32_t data_bytes;
};

/** Return the last SFX error as a static string. */
const char *sfx_error(void);

/** Decode and play a supported file to `/dev/audio`. */
int sfx_play(const char *path);

#endif /* MONARCH_BASE_SFX_SOUND_H */
