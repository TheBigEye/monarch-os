#ifndef MONARCH_BASE_SYS_STREAM_H
#define MONARCH_BASE_SYS_STREAM_H 1

#ifndef MONARCH_KERNEL_BUILD
#error "base/sys is kernel-only; userspace must use base/usr or base/api instead."
#endif

/**
 * @file stream.h
 * @brief Generic character output stream adapter.
 *
 * A stream is a tiny object that knows how to emit characters to an arbitrary
 * target.  The target could be a console, a serial port, a memory buffer, a log
 * sink, or anything else that can accept one character at a time.
 *
 * The design mirrors a very small subset of the idea behind Unix file streams:
 * callers do not need to know exactly what device is behind the stream; they
 * just call `put`, `write`, or `printf`.
 */

#include "base/api/monarch.h"

/**
 * Generic output stream.
 *
 * The first fields are public methods.  The last fields are private state and
 * should normally be treated as implementation details.
 */
struct stream {
    /** Write one character to the stream. */
    void (*put)(struct stream *self, char ch);

    /** Write a NUL-terminated string to the stream. */
    void (*write)(struct stream *self, const char *text);

    /** Format text and write it to the stream. */
    int (*printf)(struct stream *self, const char *fmt, ...);

    /** Private: object that receives the characters. */
    void *_target;

    /** Private: low-level callback used by put(). */
    void (*_put)(void *target, char ch);
};

/**
 * Initialise a stream object.
 *
 * @param self   Stream object to initialise.
 * @param target Opaque pointer passed back to `put`.
 * @param put    Function that writes a single character to `target`.
 */
void stream(struct stream *self, void *target, void (*put)(void *target, char ch));

#endif /* MONARCH_BASE_SYS_STREAM_H */
