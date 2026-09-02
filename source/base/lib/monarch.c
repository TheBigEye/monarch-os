/**
 * @file monarch.c
 * @brief Freestanding runtime helpers shared by kernel and userspace.
 *
 * This file provides the small libc-like subset Monarch actually uses today:
 * memory operations, strings, ASCII helpers and a compact printf core.
 */

#include "base/api/monarch.h"

/** Set a byte range to a repeated value.  Used constantly for clearing structs, buffers, and video memory. */
void *memset(void *dst, int value, size_t count) {
    unsigned char *d = dst;
    while (count--) {
        *d++ = (unsigned char)value;
    }
    return dst;
}

/** Copy a non-overlapping byte range.  This is the fast/simple copy primitive; use memmove() when ranges can overlap. */
void *memcpy(void *dst, const void *src, size_t count) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (count--) {
        *d++ = *s++;
    }
    return dst;
}

/** Copy bytes safely even when source and destination overlap.  Direction is chosen to avoid overwriting unread input. */
void *memmove(void *dst, const void *src, size_t count) {
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d == s || count == 0) {
        return dst;
    }

    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;
        while (count--) {
            *--d = *--s;
        }
    }
    return dst;
}

/** Compare two memory ranges byte-by-byte.  The exact non-zero value is less important than its sign. */
int memcmp(const void *a, const void *b, size_t count) {
    const unsigned char *x = a;
    const unsigned char *y = b;
    while (count--) {
        if (*x != *y) {
            return *x < *y ? -1 : 1;
        }
        x++;
        y++;
    }
    return 0;
}

/** Measure a NUL-terminated string.  A nil pointer is treated as an empty string for kernel robustness. */
size_t strlen(const char *s) {
    const char *p = s;
    if (!s) {
        return 0;
    }
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}

/** Measure a string but never inspect more than maxlen bytes.  Useful for untrusted or fixed-size buffers. */
size_t strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    if (!s) {
        return 0;
    }
    while (n < maxlen && s[n]) {
        n++;
    }
    return n;
}

/** Copy a string including its NUL terminator.  The caller must guarantee that dst is large enough. */
char *strcpy(char *dst, const char *src) {
    char *out = dst;
    while ((*dst++ = *src++) != '\0') {
    }
    return out;
}

/** Bounded string copy.  Monarch's variant always NUL-terminates when count is non-zero. */
char *strncpy(char *dst, const char *src, size_t count) {
    size_t i = 0;
    if (!count) {
        return dst;
    }
    for (; i + 1 < count && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return dst;
}

/** Append src to the end of dst.  This is simple and assumes dst has enough free capacity. */
char *strcat(char *dst, const char *src) {
    strcpy(dst + strlen(dst), src);
    return dst;
}

/** Append at most count bytes from src to dst and write a final NUL byte. */
char *strncat(char *dst, const char *src, size_t count) {
    char *out = dst;
    dst += strlen(dst);
    while (count && *src) {
        *dst++ = *src++;
        count--;
    }
    *dst = '\0';
    return out;
}

/** Lexicographic string comparison with nil-pointer protection for defensive kernel code. */
int strcmp(const char *a, const char *b) {
    if (!a || !b) {
        return a ? 1 : b ? -1 : 0;
    }
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/** Bounded lexicographic string comparison. */
int strncmp(const char *a, const char *b, size_t count) {
    if (count == 0) {
        return 0;
    }
    while (count-- && *a && *a == *b) {
        if (count == 0) {
            return 0;
        }
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/** Find the first occurrence of a character in a string. */
char *strchr(const char *s, int c) {
    if (!s) {
        return nil;
    }
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return c == 0 ? (char *)s : nil;
}

/** Find the last occurrence of a character in a string. */
char *strrchr(const char *s, int c) {
    const char *last = nil;
    if (!s) {
        return nil;
    }
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return c == 0 ? (char *)s : (char *)last;
}

/** Return non-zero if c is one of the small ASCII whitespace characters Monarch recognises. */
int space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/** Remove leading/trailing whitespace in-place.  The returned pointer may point inside the original buffer. */
char *trim(char *s) {
    char *end;
    size_t len;

    if (!s) {
        return s;
    }

    while (space(*s)) {
        s++;
    }

    len = strlen(s);
    if (!len) {
        return s;
    }

    end = s + len - 1;
    while (end >= s && space(*end)) {
        *end-- = '\0';
    }
    return s;
}

/** Test whether s begins with prefix. */
int starts(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

/** Test for ASCII decimal digit. */
int digit(char c) {
    return c >= '0' && c <= '9';
}

/** Convert one ASCII uppercase character to lowercase; other bytes are returned unchanged. */
char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}

/** Convert one ASCII lowercase character to uppercase; other bytes are returned unchanged. */
char upper(char c) {
    return c >= 'a' && c <= 'z' ? (char)(c - ('a' - 'A')) : c;
}

/** Parse a signed decimal integer.  This intentionally small parser ignores overflow for now. */
int atoi(const char *s) {
    int sign = 1;
    int value = 0;

    while (space(*s)) {
        s++;
    }
    if (*s == '-' || *s == '+') {
        sign = *s == '-' ? -1 : 1;
        s++;
    }
    while (digit(*s)) {
        value = value * 10 + (*s - '0');
        s++;
    }
    return value * sign;
}

/** Internal helper: emit one formatted character and update the output count. */
static void emit(void (*put)(void *, char), void *ctx, char ch, int *count) {
    put(ctx, ch);
    (*count)++;
}

/** Internal helper: emit a whole string, using "(null)" for nil strings. */
static void emits(void (*put)(void *, char), void *ctx, const char *s, int *count) {
    if (!s) {
        s = "(null)";
    }
    while (*s) {
        emit(put, ctx, *s++, count);
    }
}

/** Internal helper: convert an unsigned number to decimal/hex text and emit it. */
static void number(void (*put)(void *, char), void *ctx, unsigned value, unsigned base, int width, char pad, int upperhex, int *count) {
    char digits[] = "0123456789abcdef";
    char stack[32];
    int i = 0;

    if (upperhex) {
        digits[10] = 'A'; digits[11] = 'B'; digits[12] = 'C';
        digits[13] = 'D'; digits[14] = 'E'; digits[15] = 'F';
    }

    if (value == 0) {
        stack[i++] = '0';
    } else {
        while (value && i < (int)sizeof(stack)) {
            stack[i++] = digits[value % base];
            value /= base;
        }
    }

    while (i < width) {
        emit(put, ctx, pad, count);
        width--;
    }

    while (i--) {
        emit(put, ctx, stack[i], count);
    }
}

/** Core printf-style formatter.  Supported specifiers are intentionally small: c, s, d/i, u, x/X, p, and %. */
int kvformat(void (*put)(void *ctx, char ch), void *ctx, const char *fmt, va_list ap) {
    int count = 0;

    while (*fmt) {
        int width = 0;
        int left = 0;
        char pad = ' ';
        int longarg = 0;

        if (*fmt != '%') {
            emit(put, ctx, *fmt++, &count);
            continue;
        }

        fmt++;
        if (*fmt == '-') {
            left = 1;
            fmt++;
        }
        if (*fmt == '0' && !left) {
            pad = '0';
            fmt++;
        }
        while (digit(*fmt)) {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (*fmt == 'l') {
            longarg = 1;
            fmt++;
            if (*fmt == 'l') {
                fmt++;
            }
        }

        switch (*fmt) {
            case 'c': {
                char value = (char)va_arg(ap, int);
                if (!left) {
                    for (int i = 1; i < width; i++) {
                        emit(put, ctx, pad, &count);
                    }
                }
                emit(put, ctx, value, &count);
                if (left) {
                    for (int i = 1; i < width; i++) {
                        emit(put, ctx, ' ', &count);
                    }
                }
                break;
            }
            case 's': {
                const char *value = va_arg(ap, const char *);
                int len;
                if (!value) {
                    value = "(null)";
                }
                len = (int)strlen(value);
                if (!left) {
                    for (int i = len; i < width; i++) {
                        emit(put, ctx, pad, &count);
                    }
                }
                emits(put, ctx, value, &count);
                if (left) {
                    for (int i = len; i < width; i++) {
                        emit(put, ctx, ' ', &count);
                    }
                }
                break;
            }
            case 'd':
            case 'i': {
                int value = longarg ? (int)va_arg(ap, long) : va_arg(ap, int);
                if (value < 0) {
                    emit(put, ctx, '-', &count);
                    value = -value;
                }
                number(put, ctx, (unsigned)value, 10, left ? 0 : width, pad, 0, &count);
                break;
            }
            case 'u': {
                unsigned value = longarg ? (unsigned)va_arg(ap, unsigned long) : va_arg(ap, unsigned);
                number(put, ctx, value, 10, left ? 0 : width, pad, 0, &count);
                break;
            }
            case 'x': {
                unsigned value = longarg ? (unsigned)va_arg(ap, unsigned long) : va_arg(ap, unsigned);
                number(put, ctx, value, 16, left ? 0 : width, pad, 0, &count);
                break;
            }
            case 'X': {
                unsigned value = longarg ? (unsigned)va_arg(ap, unsigned long) : va_arg(ap, unsigned);
                number(put, ctx, value, 16, left ? 0 : width, pad, 1, &count);
                break;
            }
            case 'p': {
                uintptr_t value = (uintptr_t)va_arg(ap, void *);
                emits(put, ctx, "0x", &count);
                number(put, ctx, (unsigned)value, 16, 8, '0', 0, &count);
                break;
            }
            case '%':
                emit(put, ctx, '%', &count);
                break;
            default:
                emit(put, ctx, '%', &count);
                if (*fmt) {
                    emit(put, ctx, *fmt, &count);
                }
                break;
        }
        if (*fmt) {
            fmt++;
        }
    }
    return count;
}


/** Variadic wrapper around kvformat(). */
int kformat(void (*put)(void *ctx, char ch), void *ctx, const char *fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = kvformat(put, ctx, fmt, ap);
    va_end(ap);
    return result;
}

struct buffer {
    char *data;
    size_t size;
    size_t used;
};

/** Internal snprintf sink: count every character but only store while space remains. */
static void bufferput(void *ctx, char ch) {
    struct buffer *b = ctx;
    if (b->used + 1 < b->size) {
        b->data[b->used] = ch;
    }
    b->used++;
}

/** Format into a fixed-size buffer, truncating safely and always writing a terminator when size > 0. */
int vsnprintf(char *buffer, size_t size, const char *fmt, va_list ap) {
    struct buffer b;
    int result;

    if (!buffer || !size) {
        return 0;
    }

    b.data = buffer;
    b.size = size;
    b.used = 0;
    result = kvformat(bufferput, &b, fmt, ap);
    buffer[min(b.used, size - 1)] = '\0';
    return result;
}

/** Variadic wrapper around vsnprintf(). */
int snprintf(char *buffer, size_t size, const char *fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = vsnprintf(buffer, size, fmt, ap);
    va_end(ap);
    return result;
}
