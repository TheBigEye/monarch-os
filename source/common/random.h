#ifndef _LIB_SYSTEM_H
#define _LIB_SYSTEM_H 1

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the random number generator with the provided seed.
 *
 * If the seed is 0, it defaults to 1 to avoid trivial sequences.
 *
 * @param seed The seed value to initialize the generator.
 */
void rng_seed(unsigned int seed);


/**
 * @brief Generates the next pseudo-random number using a 32-bit Linear Congruential Generator.
 *
 * Uses the parameters: a = 1664525, c = 1013904223, and modulus 2^32.
 *
 * @return The next pseudo-random number as a 32-bit unsigned integer.
 */
uint32_t rng_next(void);


/**
 * @brief Returns a pseudo-random integer (31 bits) ensuring a positive value.
 *
 * @return A pseudo-random integer with 31-bit precision.
 */
int rng_geti(void);


/**
 * @brief Returns a pseudo-random integer within the range [min, max).
 *
 * If max is less than or equal to min, returns min as a safe value.
 *
 * @param min The lower bound (inclusive).
 * @param max The upper bound (exclusive).
 * @return A pseudo-random integer within the specified range.
 */
int rng_getr(int min, int max);


/**
 * @brief Returns a pseudo-random floating-point number in the range [0, 1).
 *
 * @return A pseudo-random float between 0 (inclusive) and 1 (exclusive).
 */
float rng_getf(void);


/**
 * @brief Returns a pseudo-random boolean value (0 or 1).
 *
 * @return 0 or 1, selected pseudo-randomly.
 */
int rng_bool(void);


/**
 * @brief Returns 1 with the specified probability and 0 otherwise.
 *
 * The probability should be a float between 0.0 and 1.0.
 *
 * @param probability The chance (between 0.0 and 1.0) of returning 1.
 * @return 1 with the given probability; otherwise, 0.
 */
int rng_getp(float probability);


/**
 * @brief Returns a pseudo-random character within the range [min, max].
 *
 * If the range is invalid (max is less than or equal to min), returns min.
 *
 * @param min The lower bound character.
 * @param max The upper bound character.
 * @return A pseudo-random character within the specified range.
 */
char rng_getc(char min, char max);


/**
 * @brief Returns a pseudo-random floating-point number in the range [min, max).
 *
 * If the range is invalid (max is less than or equal to min), returns min.
 *
 * @param min The lower bound (inclusive).
 * @param max The upper bound (exclusive).
 * @return A pseudo-random float within the specified range.
 */
float rng_getrf(float min, float max);


/**
 * @brief Generates a pseudo-random 32-bit integer with a certain percentage of bits set to 1.
 *
 * @param percentage Percentage of bits to set (0-100).
 * @return A 32-bit integer with the specified percentage of bits set.
 */
uint32_t rng_bits(uint8_t percentage);


#endif /* _LIB_SYSTEM_H */
