#include "keyboard.h"

#include "../CPU/PIT/timer.h"
#include "../CPU/ISR/ISR.h"
#include "../CPU/HAL.h"

#include "../modules/terminal.h"

volatile uint8_t scancode = 0x00;

volatile bool capslock_enabled = false;
volatile bool keyboard_enabled = false;


/* US Keyboard layout */

/* NOTE: 'Unsigned char' are used instead of 'signed char' due to
   an overflow if we try use the extended ascii symbols hight to 127
*/

const uint8_t key_layout[128][2] = {
    /* {U ,  L}  | CAPS OFF          | CAPS ON           | SC   */
    { 0 ,  0 }, /* Unknown           | Unknown           | 0x00 */

    { 0 ,  0 }, /* Escape            | None              | 0x01 */
    {'1', '!'}, /* 1 num             | Exclamation       | 0x02 */
    {'2', '@'}, /* 2 num             | At Sign           | 0x03 */
    {'3', '#'}, /* 3 num             | Pound Sign        | 0x04 */
    {'4', '$'}, /* 4 num             | Dollar Sign       | 0x05 */
    {'5', '%'}, /* 5 num             | Percent Sign      | 0x06 */
    {'6', '^'}, /* 6 num             | Caret             | 0x07 */
    {'7', '&'}, /* 7 num             | Ampersand         | 0x08 */
    {'8', '*'}, /* 8 num             | Asterisk          | 0x09 */
    {'9', '('}, /* 9 num             | L Parenthesis     | 0x0A */
    {'0', ')'}, /* 0 num             | R Parenthesis     | 0x0B */
    {'-', '_'}, /* Minus             | Underscore        | 0x0C */
    {'=', '+'}, /* Equal             | Plus              | 0x0D */
    {'\b', '\b'}, /* Backspace       | Backspace         | 0x0E */

    {'\t', '\t'}, /* Tab             | Tab               | 0x0F */
    {'Q', 'q'}, /* Q upper key       | q lower key       | 0x10 */
    {'W', 'w'}, /* W upper key       | w lower key       | 0x11 */
    {'E', 'e'}, /* E upper key       | e lower key       | 0x12 */
    {'R', 'r'}, /* R upper key       | r lower key       | 0x13 */
    {'T', 't'}, /* T upper key       | t lower key       | 0x14 */
    {'Y', 'y'}, /* Y upper key       | y lower key       | 0x15 */
    {'U', 'u'}, /* U upper key       | u lower key       | 0x16 */
    {'I', 'i'}, /* I upper key       | i lower key       | 0x17 */
    {'O', 'o'}, /* O upper key       | o lower key       | 0x18 */
    {'P', 'p'}, /* P upper key       | p lower key       | 0x19 */
    {'[', '{'}, /* L Bracket         | L Brace           | 0x1A */
    {']', '}'}, /* R Bracket         | R Brace           | 0x1B */
    {'\n', '\n'}, /* Enter           | Enter             | 0x1C */

    { 0 ,  0 }, /* Control           | None              | 0x1D */
    {'A', 'a'}, /* A upper key       | a lower key       | 0x1E */
    {'S', 's'}, /* S upper key       | s lower key       | 0x1F */
    {'D', 'd'}, /* D upper key       | d lower key       | 0x20 */
    {'F', 'f'}, /* F upper key       | f lower key       | 0x21 */
    {'G', 'g'}, /* G upper key       | g lower key       | 0x22 */
    {'H', 'h'}, /* H upper key       | h lower key       | 0x23 */
    {'J', 'j'}, /* J upper key       | j lower key       | 0x24 */
    {'K', 'k'}, /* K upper key       | k lower key       | 0x25 */
    {'L', 'l'}, /* L upper key       | l lower key       | 0x26 */
    {';', ':'}, /* Semicolon         | Colon             | 0x27 */
    {'\'', '\"'}, /* Quote           | Double Quote      | 0x28 */
    {'`', '`'}, /* Backquote         | Tilde             | 0x29 */
    { 0 ,  0 }, /* L Shift           | L Shift           | 0x2A */

    {'\\', '|'}, /* Backslash        | Pipe              | 0x2B */
    {'Z', 'z'}, /* Z upper key       | z lower key       | 0x2C */
    {'X', 'x'}, /* X upper key       | x lower key       | 0x2D */
    {'C', 'c'}, /* C upper key       | c lower key       | 0x2E */
    {'V', 'v'}, /* V upper key       | v lower key       | 0x2F */
    {'B', 'b'}, /* B upper key       | b lower key       | 0x30 */
    {'N', 'n'}, /* N upper key       | n lower key       | 0x31 */
    {'M', 'm'}, /* M upper key       | m lower key       | 0x32 */
    {',', '<'}, /* Comma             | Less Than         | 0x33 */
    {'.', '>'}, /* Period            | Greater Than      | 0x34 */
    {'/', '?'}, /* Slash             | Question Mark     | 0x35 */
    { 0 ,  0 }, /* R Shift           | R Shift           | 0x36 */
    {'*', '*'}, /* N Asterisk        | None              | 0x37 */
    { 0 ,  0 }, /* Alt left          | None              | 0x38 */
    {' ', ' '}, /* Spacebar          | Spacebar          | 0x39 */
    { 0 ,  0 }, /* Caps Lock         | Caps Lock         | 0x3A */

    { 0 ,  0 }, /* F1 key            | None              | 0x3B */
    { 0 ,  0 }, /* F2 key            | None              | 0x3C */
    { 0 ,  0 }, /* F3 key            | None              | 0x3D */
    { 0 ,  0 }, /* F4 key            | None              | 0x3E */
    { 0 ,  0 }, /* F5 key            | None              | 0x3F */
    { 0 ,  0 }, /* F6 key            | None              | 0x40 */
    { 0 ,  0 }, /* F7 key            | None              | 0x41 */
    { 0 ,  0 }, /* F8 key            | None              | 0x42 */
    { 0 ,  0 }, /* F9 key            | None              | 0x43 */
    { 0 ,  0 }, /* F10 key           | None              | 0x44 */

    { 0 ,  0 }, /* Num Lock          | None              | 0x45 */
    { 0 ,  0 }, /* Scroll Lock       | None              | 0x46 */
    { 0 ,  0 }, /* Home              | None              | 0x47 */
    { 24, 227}, /* Up Arrow          | Pi Sign           | 0x48 */
    { 0 ,  0 }, /* Page Up           | None              | 0x49 */
    {'-', '-'}, /* Minus             | Minus             | 0x4A */
    { 27,  27}, /* Left Arrow        | Left Arrow        | 0x4B */
    { 0 ,  0 }, /* Center            | None              | 0x4C */
    { 26,  26}, /* Right Arrow       | Right Arrow       | 0x4D */
    {'+', '+'}, /* Plus              | Plus              | 0x4E */
    { 0 ,  0 }, /* End               | None              | 0x4F */
    { 25,  25}, /* Down Arrow        | Down Arrow        | 0x50 */
    { 0 ,  0 }, /* Page Down         | None              | 0x51 */
    { 0 ,  0 }, /* Insert            | None              | 0x52 */
    { 0 ,  0 }, /* Delete            | None              | 0x53 */

    { 0 ,  0 }, /* PrtSc             | SysRq             | 0x54 */
    { 0 ,  0 }, /* Unknown           | Unknown           | 0x55 */
    { 0 ,  0 }, /* Unknown           | Unknown           | 0x56 */

    { 0 ,  0 }, /* F11               | None              | 0x57 */
    { 0 ,  0 }, /* F12               | None              | 0x58 */

    { 0 ,  0 }, /* Unknown           | Unknown           | 0x59 */
    { 0 ,  0 }, /* Unknown           | Unknown           | 0x5A */
    { 0 ,  0 }, /* Left Windows      | None              | 0x5B */
    { 0 ,  0 }, /* Right Windows     | None              | 0x5C */
    { 0 ,  0 }, /* Right Click       | None              | 0x5D */
    { 0 ,  0 }, /* Unknown           | Unknown           | 0x5E */
    { 0 ,  0 }, /* Unknown           | Unknown           | 0x5F */

};


/**
 * Callback function for keyboard interrupt.
 *
 * @param regs  Register state at the time of interrupt.
 */
static void keyboardCallback(registers_t *regs) {
    if (!keyboard_enabled) {
        return;
    }

    uint8_t sc = readByteFromPort(0x60);

    // If key released (bit 7 set)
    if (sc & 0x80) {
        // No action needed on key release
    } else {
        // Key pressed
        if (sc == 0x3A) {
            capslock_enabled = !capslock_enabled; // Toggle caps lock state
        }
        scancode = sc;
    }

    UNUSED(regs);
}


/**
 * Initializes the keyboard by registering the keyboard callback function.
 */
void initializeKeyboard(void) {
    keyboard_enabled = true;

    fprintf(serial, "[i] Initializing keyboard handler at IRQ1\n");
    registerInterruptHandler(IRQ1, keyboardCallback);
}


void disableKeyboard(void) {
    keyboard_enabled = false;

    fprintf(serial, "[i] Cleaning keyboard handler at IRQ1\n");
    unregisterInterruptHandler(IRQ1);
}
