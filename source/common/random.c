#include "random.h"

/**
 * @brief Global seed for the random number generator.
 */
static unsigned int random_seed = 1;


/**
 * @brief Initializes the random number generator with the provided seed.
 *
 * If the seed is 0, it defaults to 1 to avoid trivial sequences.
 *
 * @param seed The seed value to initialize the generator.
 */
void rng_seed(unsigned int seed) {
    random_seed = (seed == 0) ? 1 : seed;
}


/**
 * @brief Generates the next pseudo-random number using a 32-bit Linear Congruential Generator.
 *
 * Uses the parameters: a = 1664525, c = 1013904223, and modulus 2^32.
 *
 * @return The next pseudo-random number as a 32-bit unsigned integer.
 */
uint32_t rng_next(void) {
    random_seed = 1664525u * random_seed + 1013904223u;
    return random_seed;
}


/**
 * @brief Returns a pseudo-random integer (31 bits) ensuring a positive value.
 *
 * @return A pseudo-random integer with 31-bit precision.
 */
int rng_geti(void) {
    return (int)(rng_next() & 0x7FFFFFFF);
}


/**
 * @brief Returns a pseudo-random integer within the range [min, max).
 *
 * If max is less than or equal to min, returns min as a safe value.
 *
 * @param min The lower bound (inclusive).
 * @param max The upper bound (exclusive).
 * @return A pseudo-random integer within the specified range.
 */
int rng_getr(int min, int max) {
    if (max <= min) {
        return min;
    }
    int range = max - min;
    return min + (rng_geti() % range);
}


/**
 * @brief Returns a pseudo-random floating-point number in the range [0, 1).
 *
 * @return A pseudo-random float between 0 (inclusive) and 1 (exclusive).
 */
float rng_getf(void) {
    return (float)rng_next() / 4294967296.0f;
}


/**
 * @brief Returns a pseudo-random boolean value (0 or 1).
 *
 * @return 0 or 1, selected pseudo-randomly.
 */
int rng_bool(void) {
    return (int)(rng_next() & 1);
}


/**
 * @brief Returns 1 with the specified probability and 0 otherwise.
 *
 * The probability should be a float between 0.0 and 1.0.
 *
 * @param probability The chance (between 0.0 and 1.0) of returning 1.
 * @return 1 with the given probability; otherwise, 0.
 */
int rng_getp(float probability) {
    if (probability <= 0.0f) {
        return 0;
    }
    if (probability >= 1.0f) {
        return 1;
    }
    return (rng_getf() < probability) ? 1 : 0;
}


/**
 * @brief Returns a pseudo-random character within the range [min, max].
 *
 * If the range is invalid (max is less than or equal to min), returns min.
 *
 * @param min The lower bound character.
 * @param max The upper bound character.
 * @return A pseudo-random character within the specified range.
 */
char rng_getc(char min, char max) {
    if (max <= min) {
        return min;
    }
    int range = max - min + 1;
    return (char)(min + (rng_geti() % range));
}


/**
 * @brief Returns a pseudo-random floating-point number in the range [min, max).
 *
 * If the range is invalid (max is less than or equal to min), returns min.
 *
 * @param min The lower bound (inclusive).
 * @param max The upper bound (exclusive).
 * @return A pseudo-random float within the specified range.
 */
float rng_getrf(float min, float max) {
    if (max <= min) {
        return min;
    }
    return min + (max - min) * rng_getf();
}


/**
 * @brief Generates a pseudo-random 32-bit integer with a certain percentage of bits set to 1.
 *
 * @param percentage Percentage of bits to set (0-100).
 * @return A 32-bit integer with the specified percentage of bits set.
 */
uint32_t rng_bits(uint8_t percentage) {
    if (percentage >= 100) {
        return 0xFFFFFFFF;  /* All bits set */
    }
    if (percentage <= 0) {
        return 0;  /* No bits set */
    }

    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        if (rng_getp((float)percentage / 100.0f)) {
            result |= (1u << i);
        }
    }

    return result;
}
