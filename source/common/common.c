#include "common.h"


/*
 * Get the square root from an exponent
 */
double sqrt(int n) {
    double a = (double) n;
    double x = 1;
    for (int i = 0; i < n; i++) {
        x = 0.5 * (x + a / x);
    }
    return x;
}


double factorial(int n) {
    double fact = 1;
    while (n > 1) fact *= n--;
    return fact;
}


double pow(double base, int exp) {
    double result = 1;
    for (int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}


int atoi(const char *string) {
    int sign = 1;
    int base = 0;

    // Ignore whitespaces
    while (*string == ' ' || *string == '\t' || *string == '\n') string++;

    // Check sign
    if (*string == '-' || *string == '+') {
        if (*string == '-') {
            sign = -1;
        }
        string++;
    }

    // Convert string to integer
    while (*string >= '0' && *string <= '9') {
        int digit = toDigit(*string);

        // Handle overflow
        if (base > (INT32_MAX - digit) / 10) {
            return (sign == 1) ? INT32_MAX : INT32_MIN;
        }

        // Accumulate the integer value
        base = base * 10 + digit;

        // Move to the next character
        string++;
    }

    // Return the final integer value
    return base * sign;
}


/**
 * Convert an integer to a string.
 *
 * @param integer The integer to convert.
 * @return The converted string.
 */
char *itoa(int integer) {
    static char string[16]; // Static buffer to store the converted string
    int index = 0;
    bool sign = (integer < 0);

    if (integer < 0) integer = -integer; // Make the number positive for conversion

    do {
        string[index++] = (char)(integer % 10 + '0'); // Convert the least significant digit to character and store it in the buffer
        integer /= 10; // Divide the number by 10 to move to the next digit
    } while (integer > 0);

    if (sign) string[index++] = '-'; // If the number was negative, add a minus sign to the buffer

    string[index] = '\0'; // Add null-terminator to mark the end of the string

    // Reverse the string
    for (int i = 0, j = index - 1; i < j; ++i, --j) {
        char temp = string[i];
        string[i] = string[j];
        string[j] = temp;
    }

    return string; // Return the converted string
}


int normalize(double *val) {
    int exponent = 0;
    double value = *val;

    while (value >= 1.0) {
        value /= 10.0;
        ++exponent;
    }

    while (value < 0.1) {
        value *= 10.0;
        --exponent;
    }
    *val = value;
    return exponent;
}


/**
 * Convert a float or double to a string.
 *
 * @param value The value to convert.
 * @return The converted string.
 */
char *ftoa(double value) {
    static char buffer[32]; // Buffer to store the resulting string
    char* pointer = buffer; // Pointer for traversing the buffer

    int exponent = 0; // Exponent for normalizing the value
    int places = 0; // Decimal places counter
    static const int width = 6; // Desired width for the decimal part

    // Handle special cases: zero and extreme values
    if (ABS(value) < EPSILON) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    if (value >= DOUBLE_MAX || value <= DOUBLE_MIN) {
        strcpy(buffer, (value > 0) ? "INFINITE" : "-INFINITE");
        return buffer;
    }

    // Adjust the sign if negative
    if (value < 0.0) {
        *pointer++ = '-';
        value = -value;
    }

    // Normalize the value and calculate the exponent
    exponent = normalize(&value);

    // Convert integer part
    while (exponent > 0) {
        int digit = value * 10;
        *pointer++ = digit + '0';
        value = value * 10 - digit;
        ++places;
        --exponent;
    }

    // Add leading zero if no integer part
    if (places == 0) {
        *pointer++ = '0';
    }

    // Add decimal point if needed
    if (places < width) {
        *pointer++ = '.';
    }

    // Convert fractional part
    while (exponent < 0 && places < width) {
        *pointer++ = '0';
        --exponent;
        ++places;
    }

    // Fill remaining places with digits
    while (places < width) {
        int digit = value * 10.0;
        *pointer++ = digit + '0';
        value = value * 10.0 - digit;
        ++places;
    }

    *pointer = '\0'; // Null-terminate the string

    return buffer;
}


/**
 * Convert an integer to a hexadecimal string.
 *
 * @param integer The integer to convert.
 * @return The converted hexadecimal string.
 */
char *htoa(int integer) {
    int index = 0;
    static char buffer[16]; // Static buffer to store the converted hexadecimal string
    buffer[index++] = '0'; // Add the '0' character to the buffer
    buffer[index++] = 'x'; // Add the 'x' character to the buffer

    // Convert each nibble (4 bits) of the integer to a hexadecimal digit
    for (int i = 28; i >= 0; i -= 4) {
        int temp = (integer >> i) & 0xF; // Extract the current nibble
        if (temp >= 0xA) {
            buffer[index++] = (char)(temp - 0xA + 'A'); // Convert and add the hexadecimal digit (A-F) to the buffer
        } else {
            buffer[index++] = (char)(temp + '0'); // Convert and add the hexadecimal digit (0-9) to the buffer
        }
    }

    buffer[index] = '\0'; // Add null-terminator to mark the end of the string
    return buffer; // Return the converted hexadecimal string
}


/**
 * Calculate the length of a string
 */
int strlen(const char *string) {
    register const char *pointer = string;
    while (*(pointer++));
    return pointer - string - 1;
}


/**
 * Reverse a string (strrev).
 */
void strrev(char *string) {
    // Check for a null pointer or an empty string
    if (string == NULL || *string == '\0') {
        return;
    }

    int length = strlen(string);

    for (int index = 0; index < length / 2; index++) {
        char temp = string[index];
        string[index] = string[length - index - 1];
        string[length - index - 1] = temp;
    }
}


/**
 * Concatenate two strings (strcat).
 */
char *strcat(char *destination, const char *source) {
    char *pointer = destination;
    while (*pointer) pointer++;        // We go util the null terminator
    while ((*pointer++ = *source++));  // Copy the source, and the '\0'
    return destination;
}


/**
 * Concatenate up to n characters from source string to destination string.
 *
 * @param destination Pointer to the destination string buffer.
 * @param source Pointer to the source string to be concatenated.
 * @param limit Maximum number of characters to concatenate.
 * @return Pointer to the destination string.
 */
char *strncat(char *destination, const char *source, unsigned int limit) {
    char *end = destination + strlen(destination);
    unsigned int i = 0;

    while (i < limit && source[i] != '\0') {
        end[i] = source[i];
        i++;
    }

    end[i] = '\0';
    return destination;
}


/**
 * Copy a null-terminated string from source to destinatio (strcpy).
 *
 * @param destination Pointer to the destination string buffer.
 * @param source Pointer to the source string to be copied.
 */
char *strcpy(char *destination, const char *source) {
    // Copy characters until reaching the null terminator
    while ((*destination++ = *source++));

    return destination;
}


// strncpy
char *strncpy(char *destination, const char *source, unsigned int limit) {
    unsigned int i;

    for (i = 0; (i < (limit - 1)) && (source[i] != '\0'); i++) {
        destination[i] = source[i];
    }

    destination[i] = '\0';

    return destination;
}


/** String to uppercase */
void strlwr(char *string) {
    do {
        *string = toLower(*string);
    } while (*(string++));
}


/** String to uppercase */
void strupr(char *string) {
    do {
        *string = toUpper(*string);
    } while (*(string++));
}


/**
 * Converts an integer to a string representation in the specified base.
 *
 * @param integer The integer value to convert.
 * @param buffer The buffer where the resulting string will be stored.
 * @param base The base to use for the conversion (e.g., 10 for decimal, 16 for hexadecimal).
 */
void strint(int integer, char *buffer, int base) {
    // Static array for digit conversion in bases up to 16
    static const char digits[17] = "0123456789ABCDEF";

    // Pointer to navigate through the buffer
    char *pointer = buffer;

    // Determine if the number is negative in base 10
    bool sign = (integer < 0 && base == 10);

    // Convert negative integer to positive if base 10 and negative
    if (sign) integer = -integer;

    do {
        *pointer++ = digits[integer % base]; // Store the least significant digit in the buffer
        integer /= base; // Reduce integer by base for next digit conversion
    } while (integer);

    if (sign) *pointer++ = '-'; // Append '-' sign if originally negative in base 10
    *pointer-- = '\0'; // Null-terminate the string

    // Reverse the string in place
    for (char *start = buffer; start < pointer; ++start, --pointer) {
        char temp = *start; // Temporarily store character from start
        *start = *pointer; // Swap character at start with character at pointer
        *pointer = temp; // Store original character from start at pointer
    }
}


/**
 * Compares two strings for equality.
 *
 * @note streq() is a more efficient but less secure alternative to streql().
 *
 * @param a The first string.
 * @param b The second string.
 * @return true if the strings are equal, false otherwise.
 */
bool streql(char *a, char *b) {
    // If the lengths are different, the strings are not equal
    if (strlen(a) != strlen(b)) {
        return false;
    }

    // Compare each character of the strings
    for (int i = 0; a[i] != '\0'; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}


/**
 * Compares two strings for equality.
 *
 * @param a The first string.
 * @param b The second string.
 * @return true if the strings are equal, false otherwise.
 */
bool streq(char *a, char *b) {
    while (*a && *b) {
        if (*a++ != *b++) {
            return false;
        }
    }
    return *a == *b;
}


/**
 * Compare two strings lexicographically.
 *
 * @param a The first string.
 * @param b The second string.
 * @return 0 if the strings are equal, a negative value if the first string is less than the second,
 *         or a positive value if the first string is greater than the second.
 */
int strcmp(const char *a, const char *b) {
    // Protection against NULL pointers (segfault prevention)
    if (!a || !b) return a ? 1 : b ? -1 : 0;

    // Compare characters until a difference is found or end of strings
    while (*a && *a == *b) a++, b++;

    // Return the difference in ASCII values
    return (unsigned char) *a - (unsigned char) *b;
}


int strncmp(const char *a, const char *b, unsigned int limit) {
    while (limit && *a && (*a == *b)) {
        ++a; ++b; --limit;
    }

    if (limit == 0) {
        return 0;
    } else {
        return (*(unsigned char *)a - *(unsigned char *)b);
    }
}


/**
 * Check if a string starts with another string.
 *
 * @param a The string to check.
 * @param b The prefix to compare.
 * @return 0 if the string starts with the prefix, otherwise any other number.
 */
int substrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    // If we reached the end of a, it means all characters of a matched the beginning of b
    return *a == '\0' ? 0 : (unsigned char)*a - (unsigned char)*b;
}


/**
 * Search the first occurrence of a character in a string
 *
 * @param string   The string to search within.
 * @param character The character to find.
 * @return A pointer to the first occurrence of the character in the string,
 *         or NULL if the character is not found.
 */
char *strchr(const char *string, int character) {
    while (*string != '\0') {
        if (*string == character) {
            return ((char *) string);
        }
        string++;
    }
    return NULL;
}


char *strrchr(const char *string, int character) {
    const char *last = NULL;
    while (*string != '\0') {
        if (*string == character) {
            last = string;
        }
        string++;
    }
    return ((char *) last);
}


/**
 * Calculates the length of the initial segment of `string`
 * that consists of characters from `accept`.
 *
 * @param string The input string.
 * @param accept The set of accepted characters.
 * @return The length of the initial segment.
 */
int strspn(const char *string, const char *accept) {
    int length = 0;

    // Iterate over each character in the string
    while (*string && strchr(accept, *string++)) {
        length++;
    }

    return length;
}


/**
 * Calculates the length of the initial segment of `string`
 * that contains no characters from `reject`.
 *
 * @param string The input string.
 * @param reject The set of rejected characters.
 * @return The length of the initial segment.
 */
int strcspn(const char *string, const char *reject) {
    int length = 0;

    while(*string) {
        if(strchr(reject, *string)) {
            return length;
        } else {
            string++, length++;
        }
    }

    return length;
}


/**
 * Tokenizes a string based on the specified delimiter.
 *
 * @param string The input string (or NULL for subsequent calls).
 * @param delimiter The delimiter string.
 * @return A pointer to the next token or NULL if no more tokens.
 */
char *strtok(char *string, const char *delimiter) {
    static char* position = 0;

    // If a new string is provided, update the position
    if (string) {
        position = string;

    // If no string is provided and the position is NULL, return NULL
    } else if (!position) {
        return 0;
    }

    // Move position past any delimiter characters at the start
    string = position + strspn(position, delimiter);

    // Find the end of the current token
    position  = string + strcspn(string, delimiter);

    // If no delimiter was found, reset position and return NULL
    if (position == string) {
        return position = 0;
    }

    // Null-terminate the token and update position for the next call
    position = *position ? (*position = 0, position + 1) : 0;

    return string;
}


/**
 * @brief Removes leading and trailing whitespace characters from a string.
 * Whitespace characters include spaces, tabs, and newlines.
 *
 * @param string The string to trim
 */
void strtrim(char *string) {
    if (!string || !*string) {
        return;
    }

    uint32_t head = 0; // Pointer to the start of the string
    uint32_t tail = strlen(string) - 1; // Pointer to the end of the string

    // First we find the first non-whitespace character
    while (string[head] == ' ' || string[head] == '\t' || string[head] == '\n') {
        head++;
    }

    // Then we find the last non-whitespace character
    while (string[tail] == ' ' || string[tail] == '\t' || string[tail] == '\n') {
        tail--;
    }

    // If the string is empty or all whitespace, set it to an empty string
    if (head > tail) {
        string[0] = '\0';
        return;
    }

    // Move the non-whitespace characters to the beginning of the string
    for (uint32_t i = 0; i <= tail - head; i++) {
        string[i] = string[i + head];
    }

    // Null-terminate the string
    string[tail - head + 1] = '\0';
}


int strfind(const char *s, char c) {
    for (uint32_t i = 0; s[i] != '\0'; i++) {
        if (s[i] == c) {
            return i;
        }
    }
    return -1;
}
