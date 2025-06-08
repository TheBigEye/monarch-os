#ifndef _STDIO_H
#define _STDIO_H 1

#include "../../common/common.h"

/* Standard file descriptors */
typedef void FILE;

/* File stream definitions */
static char _stdin_buffer[1];
static char _stdout_buffer[1];
static char _stderr_buffer[1];
static char _serial_buffer[1];

/* Global file stream pointers */
FILE *stdin =     (FILE *)_stdin_buffer;
FILE *stdout =    (FILE *)_stdout_buffer;
FILE *stderr =    (FILE *)_stderr_buffer;
FILE *serial =    (FILE *)_serial_buffer;


#define FAIL    "\033[91;40m[!] "
#define INFO    "\033[92;40m[i] "
#define LINE    "\033[93;40m[o] \033[0m"
#define WARN    "\033[33;40m[!] \033[0m"
#define INIT    "\033[92;40m[...] \033[0m"


/* printf family of functions */

/* Format output to stdout */
int printf(const char *format, ...);
int printl(const char *prompt, const char *format, ...);

/* Format output to specified stream */
int fprintf(FILE *stream, const char *format, ...);

/* Format output to string buffer */
int sprintf(char *buffer, const char *format, ...);

/* Format output to string buffer with size limit */
int snprintf(char *buffer, size_t size, const char *format, ...);

/* va_list versions of the above functions */
int vprintf(const char *format, va_list args);
int vfprintf(FILE *stream, const char *format, va_list args);
int vsprintf(char *buffer, const char *format, va_list args);
int vsnprintf(char *buffer, size_t size, const char *format, va_list args);

int puts(char *string);

/* Get input string */
char *gets(void);

#endif /* !_STDIO_H */
