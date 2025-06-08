#include "speaker.h"

#include "../CPU/PIT/timer.h"
#include "../CPU/HAL.h"

/** @see https://wiki.osdev.org/PC_Speaker **/


/* Private functions */

/**
 * Play sound with the specified frequency.
 *
 * @param frequency The frequency of the sound.
 */
static void playSound(uint32_t frequency) {
    if (frequency == 0) {
        writeByteToPort(0x61, readByteFromPort(0x61) & ~3);
        return;
    }
    uint32_t scale = 1193182 / frequency;
    writeByteToPort(0x43, 0xb6);
    writeByteToPort(0x42, scale & 0xff);
    writeByteToPort(0x42, scale >> 8);
    writeByteToPort(0x61, readByteFromPort(0x61) | 3);
}


/**
 * Stop the currently playing sound.
 */
static inline void stopSound(void) {
    writeByteToPort(0x61, (readByteFromPort(0x61) & 0xFC));
}


/* Public functions */

/**
 * Make a beep sound with the specified frequency and duration.
 *
 * @param frequency The frequency of the beep sound.
 * @param duration The duration of the beep sound in milliseconds.
 */
void playBeep(uint32_t frequency, uint32_t duration) {
    if (!frequency) frequency = 950;
    if (!duration) duration = 255;
    playSound(frequency);
    timerSleep(duration);
    stopSound();
}


/**
 * Read a music chord and play their notes.
 *
 * @param chord The chord notes.
 * @param position The starting position of the chord.
 */
void playChord(char chord[], int position) {
    int len = strlen(chord);  // Length of the chord array
    int frequency = 1;  // Default frequency value

    // Iterate over the chord starting at the given position
    for (int i = position; i < len - 1; i++) {
        if (chord[i] != ',') {  // Check if the character is not a comma
            if ((chord[i + 1] != ',') && (i + 1 < len - 1)) {
                // Check if the next character is also not a comma and not the last note
                frequency = chord[i] - '0';  // Convert the note character to integer frequency
                playBeep(frequency, (30 - i));  // Play the beep with adjusted duration
                i++;  // Skip the next character
            } else {
                frequency = chord[i] - '0';  // Convert the note character to integer frequency
                playBeep(frequency, 30);  // Play the beep with default duration
            }
        }
    }
}


/**
 * Play the startup sound.
 */
void startupSound(void) {
    timerSleep(30);
    playBeep(262, 30);  // C4
    playBeep(294, 15);  // D4
    playBeep(330, 15);  // E4
    timerSleep(1);
    playBeep(440, 15);  // A4
    playBeep(494, 15);  // B4
    playBeep(523, 30);  // C5
    timerSleep(5);
}


/**
 * Play the shutdown sound.
 */
void shutdownSound(void) {
    timerSleep(5);
    playBeep(523, 30);  // C5
    playBeep(494, 15);  // B4
    playBeep(440, 15);  // A4
    timerSleep(1);
    playBeep(330, 15);  // E4
    playBeep(294, 15);  // D4
    playBeep(262, 30);  // C4
    timerSleep(30);
}
