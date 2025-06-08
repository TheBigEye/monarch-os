#ifndef _LIB_COMMON_H
#define _LIB_COMMON_H 1

    // Coss-compiler heaaders
    #include <stdbool.h>
    #include <stdint.h>


    /* Common typedefs */
    #ifndef _COMMON_TYPEDEFS
    #define _COMMON_TYPEDEFS 1

        /**
         * @brief Sometimes we want to keep parameters to a function for later use
         * and this is a solution to avoid the 'unused parameter' compiler warning
         */
        #define UNUSED(x) (void)(x)

        #define PRIVATE static
        #define PUBLIC  extern

        #define NULL ((void *) 0UL)

        #ifndef EOF
        # define EOF (-1)
        #endif

        #define INF __builtin_inf()
        #define NAN __builtin_nan("")

        /**
         * @brief This definition of PI has 17 decimal digits, which is sufficient
         * to accurately represent a `double` value according to the IEEE 754 standard.
         * Although the GCC M_PI constant has 53 digits, only the first 17 are correct
         * due to the 53-bit precision of a `double`.
         */
        #define PI 3.141592653589793

        /*
        * NOTE: (355 ⁄ 113) and (22 / 7) are PI aproximations :)
        */

        #define EPSILON 1e-9

        #define isInf __builtin_isinf
        #define isNan __builtin_isnan

        // #define MIN(a, b) ((a) < (b) ? (a) : (b)) /* @brief The minimum of two values */
        // #define MAX(a, b) ((a) > (b) ? (a) : (b)) /* @brief The maximum of two values */
        #define ABS(n) ((n) > 0 ? (n) : -(n)) /* @brief The absolute value of a number */
        #define SQR(n) ((n) * (n)) /* @brief The square value of a number */


        /* @brief The minimum of two values */
        #define MIN(a, b)            \
        ({                           \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a < _b ? _a : _b;       \
        })

        /* @brief The maximum of two values */
        #define MAX(a, b)            \
        ({                           \
            __typeof__ (a) _a = (a); \
            __typeof__ (b) _b = (b); \
            _a > _b ? _a : _b;       \
        })


        #define SIN(x)   __builtin_sin((x))
        #define COS(x)   __builtin_cos((x))
        #define TAN(x)   __builtin_tan((x))

        #define CEIL(x)  __builtin_ceil((x))
        #define FLOOR(x) __builtin_floor((x))

        #define FOREVER for (;;)
        #define NOTHING ;

        #define PACKED __attribute__ ((__packed__))
        #define PADDED __attribute__ ((aligned(4)))

        #define DOUBLE_MAX 1E37
        #define DOUBLE_MIN -1E37

        #define ARRAY_LEN(a) (sizeof((a)) / sizeof(*(a)))

        #define isDigit(c) ((unsigned int) ((c) - '0') < 10u)
        #define isLower(c) ((unsigned int) ((c) - 'a') < 26u)
        #define isUpper(c) ((unsigned int) ((c) - 'A') < 26u)
        #define isPrint(c) ((unsigned int) ((c) - ' ') < 95u)
        #define isAscii(c) ((unsigned int) (c) < 128u)

        /** Character to lowercase */
        #define toLower(c) (isUpper((c)) ? ((c) + 0x20) : (c))

        /** Character to uppercase */
        #define toUpper(c) (isLower((c)) ? ((c) - 0x20) : (c))

        /** Character to number */
        #define toDigit(c) ((unsigned int) ((c) - '0'))

        /* Definition of common types used in the codebase */

        typedef unsigned long    ULONG;
        typedef unsigned int     DWORD;
        typedef unsigned short   WORD;
        typedef unsigned char    BYTE;

        typedef unsigned int    size_t;

        /* Some math (yeah, maths sucks) */

        typedef struct { int16_t x, y; } Point;
        typedef struct { uint16_t w, h; } Size;
        typedef struct { int16_t x, y; uint16_t w, h; } Rect;
        typedef struct { float x, y, z; } Vector;
        typedef struct { int numerator, denominator; } Fraction;

        /* Builtin C definitions */

        typedef __builtin_va_list va_list;

        #define va_start(ap, last_arg)  __builtin_va_start(ap, last_arg)
        #define va_arg(ap, type)        __builtin_va_arg(ap, type)
        #define va_end(ap)              __builtin_va_end(ap)
        #define va_copy(d, s)           __builtin_va_copy(d, s)

        #define ASM       __asm__
        #define VOLATILE  __volatile__


    #endif /* _COMMON_TYPEDEFS */

    /* Common system functions */

    double sqrt(int n);
    double factorial(int n);
    double pow(double base, int exp);

    int atoi(const char* string);

    char *itoa(int integer);
    char *ftoa(double value);
    char *htoa(int integer);

    int strlen(const char *string);
    void strrev(char *string);

    char *strcat(char *destination, const char *source);
    char *strncat(char *destination, const char *source, unsigned int limit);
    char *strcpy(char *destination, const char *source);
    char *strncpy(char *destination, const char *source, unsigned int limit);

    void strlwr(char *string);
    void strupr(char *string);

    void strint(int integer, char *buffer, int base);

    bool streql(char *a, char *b);
    int strcmp(const char *a, const char *b);
    int strncmp(const char *a, const char *b, unsigned int limit);
    int substrcmp(const char *a, const char *b);

    char *strchr(const char *string, int character);
    char *strrchr(const char *string, int character);

    int strspn(const char *string, const char *accept);
    int strcspn(const char *string, const char *reject);
    char *strtok(char *string, const char *delimiter);

    void strtrim(char *s);
    int strfind(const char *s, char c);

#endif /* ! _LIB_COMMON_H */
