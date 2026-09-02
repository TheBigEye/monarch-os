/**
 * @file sound.c
 * @brief Tiny userspace audio decoders for Monarch's fixed /dev/audio sink.
 */

#include "base/sfx/sound.h"

#define INBUF_SIZE 4096u
#define OUT_FRAMES 1024u
#define WAV_FMT_PCM 1u
#define AU_UNKNOWN_SIZE 0xFFFFFFFFu
#define AU_LINEAR8 2u
#define AU_LINEAR16 3u

static char error_text[80];
static uint8_t inbuf[INBUF_SIZE];
static int16_t outbuf[OUT_FRAMES * 2u];
static size_t out_frames;
static int audio_fd;

static void set_error(const char *text) {
    strncpy(error_text, text ? text : "sfx error", sizeof(error_text));
}

const char *sfx_error(void) {
    return error_text[0] ? error_text : "ok";
}

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static int read_full(int fd, void *buffer, size_t size) {
    size_t done = 0;

    while (done < size) {
        long count = read(fd, (uint8_t *)buffer + done, size - done);
        if (count <= 0) {
            return 0;
        }
        done += (size_t)count;
    }
    return 1;
}

static int skip_bytes(int fd, uint32_t size) {
    return lseek(fd, (long)size, SEEK_CUR) >= 0;
}

static int flush_audio(void) {
    size_t bytes = out_frames * SFX_CHANNELS * sizeof(int16_t);
    long wrote;

    if (!out_frames) {
        return 1;
    }

    wrote = write(audio_fd, outbuf, bytes);
    out_frames = 0;
    return wrote == (long)bytes;
}

static int emit_frame(int16_t left, int16_t right) {
    outbuf[out_frames * 2u + 0u] = left;
    outbuf[out_frames * 2u + 1u] = right;
    out_frames++;
    if (out_frames >= OUT_FRAMES) {
        return flush_audio();
    }
    return 1;
}

struct sfx_resampler {
    uint32_t source_rate;
    uint32_t accumulator;
};

static int validate_pcm_format(uint32_t rate, uint32_t channels, uint32_t bits) {
    if (rate < 4000u || rate > 192000u) {
        set_error("unsupported sample rate");
        return 0;
    }
    if (channels != 1u && channels != 2u) {
        set_error("unsupported channel count");
        return 0;
    }
    if (bits != 8u && bits != 16u) {
        set_error("unsupported sample size");
        return 0;
    }
    return 1;
}

static int emit_resampled(struct sfx_resampler *resampler, int16_t left, int16_t right) {
    if (!resampler || resampler->source_rate == SFX_RATE) {
        return emit_frame(left, right);
    }

    /*
     * Tiny nearest-neighbour style resampler.
     *
     * For each source frame, accumulate the output rate.  Emit the current
     * source sample every time that accumulator crosses the source rate.  Low
     * source rates repeat samples; high source rates naturally drop some.  This
     * is intentionally crude but very small and good enough for early OS audio
     * smoke tests.
     */
    resampler->accumulator += SFX_RATE;
    while (resampler->accumulator >= resampler->source_rate) {
        if (!emit_frame(left, right)) {
            return 0;
        }
        resampler->accumulator -= resampler->source_rate;
    }
    return 1;
}

static int decode_pcm_le(int fd, uint32_t data_bytes, uint32_t sample_rate, uint32_t channels, uint32_t bits) {
    uint32_t frame_bytes = channels * (bits / 8u);
    uint32_t remaining = data_bytes;
    struct sfx_resampler resampler;

    resampler.source_rate = sample_rate;
    resampler.accumulator = 0;

    if (!frame_bytes) {
        set_error("invalid pcm frame size");
        return 0;
    }

    while (remaining >= frame_bytes) {
        size_t want = min((size_t)remaining, sizeof(inbuf));
        long got;

        want -= want % frame_bytes;
        if (!want) {
            break;
        }

        got = read(fd, inbuf, want);
        if (got <= 0) {
            set_error("truncated pcm data");
            return 0;
        }
        got -= got % (long)frame_bytes;

        for (size_t pos = 0; pos < (size_t)got; pos += frame_bytes) {
            int16_t left;
            int16_t right;

            if (bits == 8u) {
                left = (int16_t)(((int)inbuf[pos] - 128) << 8);
                right = channels == 2u ? (int16_t)(((int)inbuf[pos + 1u] - 128) << 8) : left;
            } else {
                left = (int16_t)le16(inbuf + pos);
                right = channels == 2u ? (int16_t)le16(inbuf + pos + 2u) : left;
            }

            if (!emit_resampled(&resampler, left, right)) {
                set_error("audio write failed");
                return 0;
            }
        }

        remaining -= (uint32_t)got;
    }

    return flush_audio();
}

static int decode_pcm_be(int fd, uint32_t data_bytes, uint32_t sample_rate, uint32_t channels, uint32_t bits) {
    uint32_t frame_bytes = channels * (bits / 8u);
    uint32_t remaining = data_bytes;
    struct sfx_resampler resampler;

    resampler.source_rate = sample_rate;
    resampler.accumulator = 0;

    if (!frame_bytes) {
        set_error("invalid pcm frame size");
        return 0;
    }

    while (remaining >= frame_bytes) {
        size_t want = min((size_t)remaining, sizeof(inbuf));
        long got;

        want -= want % frame_bytes;
        if (!want) {
            break;
        }

        got = read(fd, inbuf, want);
        if (got <= 0) {
            set_error("truncated pcm data");
            return 0;
        }
        got -= got % (long)frame_bytes;

        for (size_t pos = 0; pos < (size_t)got; pos += frame_bytes) {
            int16_t left;
            int16_t right;

            if (bits == 8u) {
                left = (int16_t)((int8_t)inbuf[pos] << 8);
                right = channels == 2u ? (int16_t)((int8_t)inbuf[pos + 1u] << 8) : left;
            } else {
                left = (int16_t)((inbuf[pos] << 8) | inbuf[pos + 1u]);
                right = channels == 2u ? (int16_t)((inbuf[pos + 2u] << 8) | inbuf[pos + 3u]) : left;
            }

            if (!emit_resampled(&resampler, left, right)) {
                set_error("audio write failed");
                return 0;
            }
        }

        remaining -= (uint32_t)got;
    }

    return flush_audio();
}

static int play_wav(int fd) {
    uint8_t header[12];
    uint32_t channels = 0;
    uint32_t sample_rate = 0;
    uint32_t bits = 0;
    int have_fmt = 0;

    if (lseek(fd, 0, SEEK_SET) < 0 || !read_full(fd, header, sizeof(header)) ||
        memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        set_error("invalid wav header");
        return 0;
    }

    for (;;) {
        uint8_t chunk[8];
        uint32_t size;

        if (!read_full(fd, chunk, sizeof(chunk))) {
            set_error("wav data chunk not found");
            return 0;
        }
        size = le32(chunk + 4);

        if (memcmp(chunk, "fmt ", 4) == 0) {
            uint8_t fmt[16];
            if (size < sizeof(fmt) || !read_full(fd, fmt, sizeof(fmt))) {
                set_error("invalid wav fmt chunk");
                return 0;
            }
            if (le16(fmt) != WAV_FMT_PCM) {
                set_error("unsupported wav compression");
                return 0;
            }
            channels = le16(fmt + 2);
            sample_rate = le32(fmt + 4);
            bits = le16(fmt + 14);
            have_fmt = 1;
            if (size > sizeof(fmt) && !skip_bytes(fd, size - sizeof(fmt))) {
                set_error("cannot skip wav fmt extension");
                return 0;
            }
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!have_fmt || !validate_pcm_format(sample_rate, channels, bits)) {
                return 0;
            }
            return decode_pcm_le(fd, size, sample_rate, channels, bits);
        } else if (!skip_bytes(fd, size)) {
            set_error("cannot skip wav chunk");
            return 0;
        }

        if (size & 1u) {
            (void)skip_bytes(fd, 1);
        }
    }
}

static int play_au(int fd) {
    uint8_t header[24];
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t encoding;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits;

    if (lseek(fd, 0, SEEK_SET) < 0 || !read_full(fd, header, sizeof(header)) || memcmp(header, ".snd", 4) != 0) {
        set_error("invalid au header");
        return 0;
    }

    data_offset = be32(header + 4);
    data_size = be32(header + 8);
    encoding = be32(header + 12);
    sample_rate = be32(header + 16);
    channels = be32(header + 20);

    if (encoding == AU_LINEAR8) {
        bits = 8;
    } else if (encoding == AU_LINEAR16) {
        bits = 16;
    } else {
        set_error("unsupported au encoding");
        return 0;
    }

    if (!validate_pcm_format(sample_rate, channels, bits)) {
        return 0;
    }
    if (data_offset < sizeof(header) || lseek(fd, (long)data_offset, SEEK_SET) < 0) {
        set_error("invalid au data offset");
        return 0;
    }
    if (data_size == AU_UNKNOWN_SIZE) {
        long end = lseek(fd, 0, SEEK_END);
        if (end < (long)data_offset || lseek(fd, (long)data_offset, SEEK_SET) < 0) {
            set_error("cannot size au data");
            return 0;
        }
        data_size = (uint32_t)((uint32_t)end - data_offset);
    }

    return decode_pcm_be(fd, data_size, sample_rate, channels, bits);
}

static int play_msfx(int fd) {
    uint8_t header[20];
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits;
    uint32_t data_size;

    if (lseek(fd, 0, SEEK_SET) < 0 || !read_full(fd, header, sizeof(header)) || memcmp(header, "MSFX", 4) != 0) {
        set_error("invalid msfx header");
        return 0;
    }

    sample_rate = le32(header + 4);
    channels = le32(header + 8);
    bits = le32(header + 12);
    data_size = le32(header + 16);

    if (!validate_pcm_format(sample_rate, channels, bits)) {
        return 0;
    }
    return decode_pcm_le(fd, data_size, sample_rate, channels, bits);
}

int sfx_play(const char *path) {
    uint8_t magic[12];
    int fd;
    int ok;

    error_text[0] = '\0';
    out_frames = 0;

    if (!path) {
        set_error("no path");
        return 0;
    }

    audio_fd = (int)open("/dev/audio", OWRITE);
    if (audio_fd < 0) {
        set_error("cannot open /dev/audio");
        return 0;
    }

    fd = (int)open(path, OREAD);
    if (fd < 0) {
        close(audio_fd);
        set_error("cannot open input");
        return 0;
    }

    if (!read_full(fd, magic, sizeof(magic))) {
        close(fd);
        close(audio_fd);
        set_error("cannot read input header");
        return 0;
    }

    if (memcmp(magic, "RIFF", 4) == 0 && memcmp(magic + 8, "WAVE", 4) == 0) {
        ok = play_wav(fd);
    } else if (memcmp(magic, ".snd", 4) == 0) {
        ok = play_au(fd);
    } else if (memcmp(magic, "MSFX", 4) == 0) {
        ok = play_msfx(fd);
    } else {
        ok = 0;
        set_error("unknown sound format");
    }

    close(fd);
    close(audio_fd);
    return ok;
}
