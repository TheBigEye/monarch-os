#include "terminal.h"

#include "../drivers/TTY/console.h"
#include "../drivers/COM/serial.h"
#include "../drivers/keyboard.h"


/* Output destinations */
typedef enum {
    OUTPUT_CONSOLE,  // Console output using tty_plotc
    OUTPUT_SERIAL,   // Serial output using comPrintChr
    OUTPUT_BUFFER    // String buffer output
} OutputDest;


/* Output handler structure */
typedef struct {
    OutputDest destination;
    void *context;         // Either NULL for console/serial or buffer pointer
    size_t max_size;       // Used for snprintf to limit output size
    size_t count;          // Characters written so far
} OutputHandler;


/* Write a single character to the appropriate destination */
static inline void write_char(OutputHandler *handler, char c) {
    // If we've hit the buffer limit, don't write any more chars (except null terminator)
    if (handler->max_size > 0 && handler->count >= handler->max_size - 1) {
        return;
    }

    switch (handler->destination) {
        case OUTPUT_CONSOLE:
            tty_plotc(c, -1, -1, (BG_BLACK | FG_DKGRAY));  // Use dark gray for values
            break;
        case OUTPUT_SERIAL:
            comPrintChr(c);
            break;
        case OUTPUT_BUFFER: {
            char *buffer = (char *)handler->context;
            if (buffer) {
                buffer[handler->count] = c;
            }
            break;
        }
    }

    handler->count++;
}


/* Write a string to the appropriate destination */
static inline void write_string(OutputHandler *handler, const char *str, int len) {
    if (len < 0) {
        len = strlen(str);
    }

    for (int i = 0; i < len; i++) {
        write_char(handler, str[i]);
    }
}


/* Write padding characters */
static inline void write_padding(OutputHandler *handler, char pad_char, int count) {
    for (int i = 0; i < count; i++) {
        write_char(handler, pad_char);
    }
}


/* Format output based on format specifiers */
static int format_output(OutputHandler *handler, const char *format, va_list args) {
    bool left_justify, zero_padding, alternate_form;
    bool force_sign, space_if_positive;
    int min_width, precision;
    bool precision_specified;
    char length_mod, pad_char;

    char temp_buffer[128];  // Buffer for number conversions
    handler->count = 0;     // Reset character count

    while (*format) {
        // Handle regular characters
        if (*format != '%') {
            if (handler->destination == OUTPUT_CONSOLE) {
                tty_plotc(*format++, -1, -1, (BG_BLACK | FG_LTGRAY));  // Regular text in light gray
            } else {
                write_char(handler, *format++);
            }
            continue;
        }

        // Handle % character
        format++;

        // Reset formatting flags
        left_justify = zero_padding = alternate_form = false;
        force_sign = space_if_positive = false;
        min_width = 0;
        precision = 0;
        precision_specified = false;
        length_mod = '\0';

        // Parse flags
        bool parsing_flags = true;
        while (parsing_flags) {
            switch (*format) {
                case '-': left_justify = true; format++; break;
                case '+': force_sign = true; format++; break;
                case ' ': space_if_positive = true; format++; break;
                case '0': zero_padding = true; format++; break;
                case '#': alternate_form = true; format++; break;
                default: parsing_flags = false; break;
            }
        }

        // Parse width
        if (*format == '*') {
            min_width = va_arg(args, int);
            if (min_width < 0) {
                min_width = -min_width;
                left_justify = true;
            }
            format++;
        } else {
            while (isDigit(*format)) {
                min_width = min_width * 10 + (*format - '0');
                format++;
            }
        }

        // Parse precision
        if (*format == '.') {
            precision_specified = true;
            format++;

            if (*format == '*') {
                precision = va_arg(args, int);
                // Negative precision is treated as no precision
                if (precision < 0) precision_specified = false;
                format++;
            } else {
                while (isDigit(*format)) {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }

        // Parse length modifier
        switch (*format) {
            case 'h': case 'l': case 'j': case 'z': case 't': case 'L':
                length_mod = *format++;
                // Handle 'hh' and 'll'
                if ((length_mod == 'h' || length_mod == 'l') && *format == length_mod) {
                    length_mod = (length_mod == 'h') ? 'H' : 'q';  // Uppercase 'H' for 'hh', 'q' for 'll'
                    format++;
                }
                break;
        }

        // Choose padding character (space or zero)
        pad_char = (zero_padding && !left_justify && !precision_specified) ? '0' : ' ';

        // Handle format specifier
        switch (*format) {
            case 'd': case 'i': {
                // Signed integer
                long long value;

                // Get argument based on length modifier
                if (length_mod == 'l') value = va_arg(args, long);
                else if (length_mod == 'q') value = va_arg(args, long long);
                else if (length_mod == 'h') value = (short)va_arg(args, int);
                else if (length_mod == 'H') value = (char)va_arg(args, int);
                else value = va_arg(args, int);

                // Convert to string and prepare prefix
                if (value < 0) {
                    strint(-value, temp_buffer, 10);
                    char prefix = '-';

                    // Calculate length and determine padding
                    int len = strlen(temp_buffer);
                    int num_padding = (precision_specified && precision > len) ?
                                     precision - len : 0;
                    int prefix_len = 1;  // Length of the sign character
                    int total_len = len + num_padding + prefix_len;
                    int width_padding = (min_width > total_len) ? min_width - total_len : 0;

                    // Output with proper padding
                    if (!left_justify && pad_char == ' ')
                        write_padding(handler, ' ', width_padding);

                    // Write prefix
                    write_char(handler, prefix);

                    // Write zero padding if needed
                    if (!left_justify && pad_char == '0')
                        write_padding(handler, '0', width_padding);

                    // Write precision zeros
                    write_padding(handler, '0', num_padding);

                    // Write the number
                    write_string(handler, temp_buffer, len);

                    // Write trailing padding for left justification
                    if (left_justify)
                        write_padding(handler, ' ', width_padding);
                } else {
                    strint(value, temp_buffer, 10);

                    // Determine sign prefix
                    char prefix = 0;
                    if (force_sign) prefix = '+';
                    else if (space_if_positive) prefix = ' ';

                    // Calculate length and determine padding
                    int len = strlen(temp_buffer);
                    int num_padding = (precision_specified && precision > len) ?
                                     precision - len : 0;
                    int prefix_len = (prefix) ? 1 : 0;
                    int total_len = len + num_padding + prefix_len;
                    int width_padding = (min_width > total_len) ? min_width - total_len : 0;

                    // Output with proper padding
                    if (!left_justify && pad_char == ' ')
                        write_padding(handler, ' ', width_padding);

                    // Write prefix if needed
                    if (prefix) write_char(handler, prefix);

                    // Write zero padding if needed
                    if (!left_justify && pad_char == '0')
                        write_padding(handler, '0', width_padding);

                    // Write precision zeros
                    write_padding(handler, '0', num_padding);

                    // Write the number
                    write_string(handler, temp_buffer, len);

                    // Write trailing padding for left justification
                    if (left_justify)
                        write_padding(handler, ' ', width_padding);
                }
                break;
            }

            case 'u': case 'o': case 'x': case 'X': {
                // Unsigned integer formats
                unsigned long long value;
                int base;

                // Determine base from format
                if (*format == 'o') base = 8;
                else if (*format == 'x' || *format == 'X') base = 16;
                else base = 10;

                // Get argument based on length modifier
                if (length_mod == 'l') value = va_arg(args, unsigned long);
                else if (length_mod == 'q') value = va_arg(args, unsigned long long);
                else if (length_mod == 'h') value = (unsigned short)va_arg(args, unsigned int);
                else if (length_mod == 'H') value = (unsigned char)va_arg(args, unsigned int);
                else value = va_arg(args, unsigned int);

                // Convert to string
                strint(value, temp_buffer, base);

                // Handle case for hex
                if (*format == 'X') strupr(temp_buffer);
                else if (*format == 'x') strlwr(temp_buffer);

                // Determine prefix
                char prefix[3] = {0};
                int prefix_len = 0;

                if (alternate_form && value != 0) {
                    if (*format == 'o') {
                        prefix[0] = '0';
                        prefix_len = 1;
                    } else if (*format == 'x') {
                        prefix[0] = '0';
                        prefix[1] = 'x';
                        prefix_len = 2;
                    } else if (*format == 'X') {
                        prefix[0] = '0';
                        prefix[1] = 'x';
                        prefix_len = 2;
                    }
                }

                // Calculate length and determine padding
                int len = strlen(temp_buffer);
                int num_padding = (precision_specified && precision > len) ? (precision - len) : 0;
                int total_len = len + num_padding + prefix_len;
                int width_padding = (min_width > total_len) ? (min_width - total_len) : 0;

                // Output with proper padding
                if (!left_justify && pad_char == ' ') {
                    write_padding(handler, ' ', width_padding);
                }

                // Write prefix if needed
                if (prefix_len > 0) {
                    write_string(handler, prefix, prefix_len);
                }

                // Write zero padding if needed
                if (!left_justify && pad_char == '0') {
                    write_padding(handler, '0', width_padding);
                }

                // Write precision zeros
                write_padding(handler, '0', num_padding);

                // Write the number
                write_string(handler, temp_buffer, len);

                // Write trailing padding for left justification
                if (left_justify) {
                    write_padding(handler, ' ', width_padding);
                }

                break;
            }

            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
                // Floating point formats
                double value = va_arg(args, double);

                // Handle NaN and Infinity
                if (isNan(value)) {
                    const char *nan_str = (*format == 'F' || *format == 'E' || *format == 'G' || *format == 'A') ? "NAN" : "nan";
                    write_string(handler, nan_str, -1);

                } else if (isInf(value)) {
                    const char *inf_str;
                    if (value < 0) {
                        inf_str = (*format == 'F' || *format == 'E' || *format == 'G' || *format == 'A') ? "-INF" : "-inf";
                    } else {
                        inf_str = (*format == 'F' || *format == 'E' || *format == 'G' || *format == 'A') ? "INF" : "inf";
                    }

                    write_string(handler, inf_str, -1);
                } else {
                    // For now, we only support %f using a simple ftoa conversion
                    // Future: Add full support for other float formats
                    char *result = ftoa(value);
                    write_string(handler, result, -1);
                }
                break;
            }

            case 'c': {
                // Character
                int value = va_arg(args, int);

                // Handle padding before the character
                if (!left_justify && min_width > 1) {
                    write_padding(handler, ' ', min_width - 1);
                }

                // Write the character
                write_char(handler, (char)value);

                // Handle padding after the character
                if (left_justify && min_width > 1) {
                    write_padding(handler, ' ', min_width - 1);
                }

                break;
            }

            case 's': {
                // String
                char *value = va_arg(args, char*);
                if (!value) value = "(null)";

                int len = strlen(value);

                // Apply precision if specified
                if (precision_specified && precision < len) {
                    len = precision;
                }

                // Handle padding before the string
                if (!left_justify && min_width > len) {
                    write_padding(handler, ' ', min_width - len);
                }

                // Write the string
                write_string(handler, value, len);

                // Handle padding after the string
                if (left_justify && min_width > len) {
                    write_padding(handler, ' ', min_width - len);
                }

                break;
            }

            case 'p': {
                // Pointer
                void *value = va_arg(args, void*);

                // Write prefix
                write_string(handler, "0x", 2);

                // Convert to hex string
                strint((uintptr_t)value, temp_buffer, 16);
                strrev(temp_buffer);  // Reverse the string
                strlwr(temp_buffer);  // Lowercase

                // Calculate padding
                int len = strlen(temp_buffer);
                int pad_len = (min_width > len + 2) ? min_width - len - 2 : 0;

                // Write padding if right-justified
                if (!left_justify && pad_len > 0)
                    write_padding(handler, pad_char, pad_len);

                // Write value
                write_string(handler, temp_buffer, len);

                // Write padding if left-justified
                if (left_justify && pad_len > 0)
                    write_padding(handler, ' ', pad_len);

                break;
            }

            case 'n': {
                // Store number of characters written so far
                int *value = va_arg(args, int*);
                if (value) *value = handler->count;
                break;
            }

            case '%': {
                // Literal % character
                write_char(handler, '%');
                break;
            }

            default: {
                // Unknown format specifier
                if (*format) {
                    write_char(handler, '%');
                    write_char(handler, *format);
                }
                break;
            }
        }

        // Move to next character in format string
        if (*format) format++;
    }

    // Null-terminate buffer output
    if (handler->destination == OUTPUT_BUFFER && handler->context) {
        char *buffer = (char *) handler->context;
        if (handler->max_size > 0) {
            buffer[MIN(handler->count, handler->max_size - 1)] = '\0';
        } else {
            buffer[handler->count] = '\0';
        }
    }

    return handler->count;
}


/* vfprintf implementation - handles formatting for all output types */
int vfprintf(FILE *stream, const char *format, va_list args) {
    OutputHandler handler;

    // Determine output destination based on stream
    if (stream == stdout || stream == stderr) {
        handler.destination = OUTPUT_CONSOLE;
        handler.context = NULL;
        handler.max_size = 0;

    } else if (stream == serial) {
        handler.destination = OUTPUT_SERIAL;
        handler.context = NULL;
        handler.max_size = 0;

    } else {
        // Unsupported stream
        return -1;
    }

    return format_output(&handler, format, args);
}


/* vprintf implementation - prints to stdout */
int vprintf(const char *format, va_list args) {
    return vfprintf(stdout, format, args);
}


/**
 * @brief Print a formatted string to a buffer
 */
int vsprintf(char *buffer, const char *format, va_list args) {
    OutputHandler handler;
    handler.destination = OUTPUT_BUFFER;
    handler.context = buffer;
    handler.max_size = 0;  // No limit

    return format_output(&handler, format, args);
}


/**
 * @brief Prints to buffer with size limit
 */
int vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
    OutputHandler handler;
    handler.destination = OUTPUT_BUFFER;
    handler.context = buffer;
    handler.max_size = size;

    return format_output(&handler, format, args);
}


/**
 * @brief Print a formatted string to the standard output.
 */
int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}


/**
 * @brief Print a formatted string to the standard output with a ANSI color escape sequence
 */
int printl(const char *prompt, const char *format, ...) {
    tty_printl(prompt);
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}




/**
 * @brief Print a formatted string to the specified stream
 */
int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}


/**
 * @brief Print a formatted string to the standard output
 */
int sprintf(char *buffer, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsprintf(buffer, format, args);
    va_end(args);
    return result;
}


/**
 * @brief Format a string and store it in a buffer with a size limit
 */
int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}


/**
 * @brief Print a string with a newline character
 */
int puts(char *string) {
    int i = 0;

    while (string[i] != '\0') {
        tty_plotc(string[i], -1, -1, NULL);
        i++;
    }
    tty_plotc('\n', -1, -1, NULL);
    return i;
}


/**
 * @brief Get a input string
 */
char *gets(void) {
    static char buffer[512];

    char *head = buffer;
    char *tail = buffer + sizeof(buffer) - 1;

    tty_cursor(0x00, 0, 0);

    while (true) {
        if (scancode == 0x00) {
            continue;
        }

        if (scancode >= ARRAY_LEN(key_layout)) {
            continue;
        }

        char character = key_layout[(int) scancode][capslock_enabled];
        char output[4] = {character, '\0'};

        if (character == '\n') {
            *head = '\0';
            scancode = 0x00;

            tty_plots(output, -1, -1, NULL);
            return buffer;

        } else if (character == '\b') {
            if (head > buffer) {
                head--;
                tty_plots(output, -1, -1, NULL);
            }

        } else if (character >= ' ' && head < tail) {
            *head++ = character;
            tty_plots(output, -1, -1, NULL);

        } else if (character == '\t') {
            for (int i = 4; i > 0; i--) {
                if (head < tail) {
                    *head++ = ' ';
                }
                tty_plotc(' ', -1, -1, NULL);
            }
        }

        scancode = 0x00;
    }
}
