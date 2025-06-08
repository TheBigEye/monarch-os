#include "encrypt.h"
#include "common.h"

/**
 * @brief Caesar cipher, a type of substitution cipher in which each letter
 * in the plaintext is shifted a certain number of places down the alphabet
 *
 * @param encrypt True to encrypt, false to decrypt
 * @param string The string to encrypt or decrypt
 * @param shift The shift value
 */
char *caesar(bool encrypt, char *string, int shift) {
    if (!encrypt) {
        shift = 26 - (shift % 26);
    } else {
        shift %= 26;
    }

    for (char *c = string; *c; c++) {
        if (isUpper(*c)) {
            *c = ((*c - 'A' + shift) % 26) + 'A';
        } else if (isLower(*c)) {
            *c = ((*c - 'a' + shift) % 26) + 'a';
        }
    }

    return string;
}


/**
 * @brief Vigenère cipher, a method of encrypting alphabetic text
 * by using a simple form of polyalphabetic substitution. A keyword
 * determines the shift value for each letter in the plaintext
 *
 * @param encrypt True to encrypt, false to decrypt
 * @param string The string to encrypt or decrypt
 * @param key The keyword to use
 */
char *vigenere(bool encrypt, char *string, char *key) {
    int keyLength = strlen(key), keyIndex = 0;
    for (char *p = string; *p; p++) {

        int keyShift = (
            isUpper(key[keyIndex]) ? key[keyIndex] - 'A' :
            isLower(key[keyIndex]) ? key[keyIndex] - 'a' : 0
        );

        if (!encrypt) {
            keyShift = 26 - keyShift;
        }

        if (isUpper(*p)) {
            *p = ((*p - 'A' + keyShift) % 26) + 'A';

        } else if (isLower(*p)) {
            *p = ((*p - 'a' + keyShift) % 26) + 'a';
        }

        keyIndex = (keyIndex + 1) % keyLength;
    }

    return string;
}


/**
 * @brief Affine cipher, a type of monoalphabetic substitution cipher
 * where each letter in an alphabet is mapped to its numeric equivalent,
 * encrypted using a simple mathematical function, and converted back to a letter
 *
 * @param encrypt True to encrypt, false to decrypt
 * @param string The string to encrypt or decrypt
 * @param a The multiplicative key
 * @param b The additive key
 */
char *affine(bool encrypt, char *string, int a, int b) {
    char *p = string;
    if (encrypt) {
        while (*p) {
            if (isUpper(*p))
                *p = (((*p - 'A') * a + b) % 26) + 'A';
            else if (isLower(*p))
                *p = (((*p - 'a') * a + b) % 26) + 'a';
            p++;
        }
    } else {
        int a_inv = 0;
        while (a_inv < 26 && (a * a_inv) % 26 != 1)
            a_inv++;
        while (*p) {
            if (isUpper(*p))
                *p = ((((*p - 'A' + 26 - b) * a_inv) % 26)) + 'A';
            else if (isLower(*p))
                *p = ((((*p - 'a' + 26 - b) * a_inv) % 26)) + 'a';
            p++;
        }
    }
    return string;
}


/**
 * @brief Alberti cipher, a polyalphabetic substitution cipher
 * that uses two mixed alphabets, and a keyword to determine
 * the shift value
 *
 * @param encrypt True to encrypt, false to decrypt
 * @param string The string to encrypt or decrypt
 * @param key The keyword to use
 */
char *alberti(bool encrypt, char *string, char *key) {
    int keyIndex = 0;
    int keyLength = strlen(key);

    for (char *p = string; *p; p++) {
        int shift = isUpper(key[keyIndex]) ? key[keyIndex] - 'A' :
                    isLower(key[keyIndex]) ? key[keyIndex] - 'a' : 0;

        if (!encrypt) {
            shift = 26 - shift;
        }

        if (isUpper(*p)) {
            *p = (((*p - 'A') + shift) % 26) + 'A';

        } else if (isLower(*p)) {
            *p = (((*p - 'a') + shift) % 26) + 'a';
        }

        keyIndex = (keyIndex + 1) % keyLength;
    }

    return string;
}


/**
 * @brief Trithemius cipher, a simple polyalphabetic
 * substitution cipher, where each letter is shifted
 * by a fixed amount
 *
 * @param encrypt True to encrypt, false to decrypt
 * @param string The string to encrypt or decrypt
 * @param shift The shift value
 */
char *trithemius(bool encrypt, char *string, int shift) {
    char *p = string;

    if (encrypt) {
        while (*p) {
            if (isUpper(*p)) {
                *p = (((*p - 'A') + shift) % 26) + 'A';

            } else if (isLower(*p)) {
                *p = (((*p - 'a') + shift) % 26) + 'a';
            }

            p++;
            shift++;
        }

    } else {
        while (*p) {
            if (isUpper(*p)) {
                *p = (((*p - 'A') + (26 - (shift % 26))) % 26) + 'A';

            } else if (isLower(*p)) {
                *p = (((*p - 'a') + (26 - (shift % 26))) % 26) + 'a';
            }

            p++;
            shift++;
        }
    }

    return string;
}
