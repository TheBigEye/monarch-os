#ifndef MONARCH_BASE_API_MONARCH_H
#define MONARCH_BASE_API_MONARCH_H 1

/**
 * @file monarch.h
 * @brief Freestanding C foundation shared by the Monarch kernel and userspace apps.
 *
 * A normal hosted C program can include libc headers and call functions such as
 * `memcpy`, `strlen`, or `printf`.  A kernel cannot assume that libc exists: it
 * runs before any userspace runtime has been loaded and it must not depend on an
 * operating system that it is itself trying to become.  This header therefore
 * declares the small set of libc-like helpers that Monarch provides internally.
 *
 * The goal is educational clarity.  The functions here intentionally cover only
 * what the rest of the toy kernel needs today.  They are not full ISO C library
 * replacements, but their names match common C routines where the behaviour is
 * close enough to make code easy to read.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "base/api/abi.h"

/** Null pointer spelling used throughout Monarch's code. */
#define nil ((void *)0)

/** Mark an intentionally unused variable or parameter to silence warnings. */
#define unused(x) ((void)(x))

/** Number of elements in a compile-time array. Do not use this on pointers. */
#define countof(a) (sizeof(a) / sizeof((a)[0]))

#define private static

/**
 * Round an integer address/value upward to the next alignment boundary.
 * `align` must be a power of two, for example 4, 16, or PAGE_SIZE.
 */
#define alignup(value, align) (((uintptr_t)(value) + ((uintptr_t)(align) - 1u)) & ~((uintptr_t)(align) - 1u))

/** Readable infinite-loop macro. */
#define forever for (;;)

/** Simple expression helpers. Arguments may be evaluated more than once. */
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define abs(a) ((a) < 0 ? -(a) : (a))

/*
 * Small Linux-like errno subset.
 *
 * Monarch kernel syscalls return negative errno values (for example -ENOENT).
 * The tiny userspace runtime converts those into -1 and stores the positive
 * value in its global `errno`, similar to a normal C library.
 */
#define EPERM        1
#define ENOENT       2
#define ESRCH        3
#define EINTR        4
#define EIO          5
#define ENOEXEC      8
#define EBADF        9
#define ECHILD      10
#define EAGAIN      11
#define ENOMEM      12
#define EACCES      13
#define EFAULT      14
#define EBUSY       16
#define EEXIST      17
#define ENODEV      19
#define ENOTDIR     20
#define EISDIR      21
#define EINVAL      22
#define EMFILE      24
#define ENOTTY      25
#define ESPIPE      29
#define ENOSYS      38
#define ENOTEMPTY   39

/** Wall-clock date/time as read from the machine RTC. */
struct datetime {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

#define UTSNAME_LENGTH 65u

struct utsname {
    char sysname[UTSNAME_LENGTH];
    char nodename[UTSNAME_LENGTH];
    char release[UTSNAME_LENGTH];
    char version[UTSNAME_LENGTH];
    char machine[UTSNAME_LENGTH];
};

/** Fill `count` bytes at `dst` with the low byte of `value`. */
void *memset(void *dst, int value, size_t count);

/** Copy `count` bytes from `src` to `dst`. Regions must not overlap. */
void *memcpy(void *dst, const void *src, size_t count);

/** Copy `count` bytes from `src` to `dst`. Overlapping regions are supported. */
void *memmove(void *dst, const void *src, size_t count);

/** Compare two byte buffers. Returns 0 when equal, otherwise negative/positive. */
int memcmp(const void *a, const void *b, size_t count);

/** Return the number of bytes before the first NUL character. */
size_t strlen(const char *s);

/** Like strlen(), but stops after `maxlen` bytes to avoid scanning forever. */
size_t strnlen(const char *s, size_t maxlen);

/** Copy a NUL-terminated string. The destination must be large enough. */
char *strcpy(char *dst, const char *src);

/** Copy at most `count - 1` bytes and always NUL-terminate when count > 0. */
char *strncpy(char *dst, const char *src, size_t count);

/** Append a NUL-terminated string. The destination must have enough space. */
char *strcat(char *dst, const char *src);

/** Append at most `count` bytes from `src` and then NUL-terminate. */
char *strncat(char *dst, const char *src, size_t count);

/** Lexicographically compare two strings. */
int strcmp(const char *a, const char *b);

/** Lexicographically compare up to `count` characters. */
int strncmp(const char *a, const char *b, size_t count);

/** Find the first occurrence of character `c` in `s`. */
char *strchr(const char *s, int c);

/** Find the last occurrence of character `c` in `s`. */
char *strrchr(const char *s, int c);

/** Trim leading and trailing ASCII whitespace in-place and return the new start. */
char *trim(char *s);

/** Return non-zero when `s` begins with `prefix`. */
int starts(const char *s, const char *prefix);

/** Convert a decimal ASCII string to an int. Stops at the first non-digit. */
int atoi(const char *s);

/** ASCII case and character classification helpers. */
char lower(char c);
char upper(char c);
int digit(char c);
int space(char c);

/**
 * Format text and emit it one character at a time through `put`.
 *
 * This is the core of Monarch's small printf implementation.  Instead of
 * knowing about consoles, serial ports, or memory buffers, it calls the supplied
 * callback for every output character.  That makes it reusable by the console,
 * serial driver, generic streams, and snprintf.
 */
int kvformat(void (*put)(void *ctx, char ch), void *ctx, const char *fmt, va_list ap);

/** Convenience wrapper around kvformat() for normal variadic calls. */
int kformat(void (*put)(void *ctx, char ch), void *ctx, const char *fmt, ...);

/** Format text into a bounded memory buffer, always NUL-terminating if size > 0. */
int snprintf(char *buffer, size_t size, const char *fmt, ...);

/** va_list version of snprintf(). */
int vsnprintf(char *buffer, size_t size, const char *fmt, va_list ap);

#endif /* MONARCH_BASE_API_MONARCH_H */
