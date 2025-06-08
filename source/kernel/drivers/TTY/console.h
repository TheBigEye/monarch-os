#ifndef DRIVER_CONSOLE_H_
#define DRIVER_CONSOLE_H_ 1

#include "../../../common/common.h"

/* Foreground colors */
#define FG_BLACK            0x00 // Black foreground attribute
#define FG_BLUE             0x01 // Blue foreground attribute
#define FG_GREEN            0x02 // Green foreground attribute
#define FG_CYAN             0x03 // Cyan foreground attribute
#define FG_RED              0x04 // Red foreground attribute
#define FG_MAGENTA          0x05 // Magenta foreground attribute
#define FG_BROWN            0x06 // Brown foreground attribute
#define FG_LTGRAY           0x07 // Light gray foreground attribute
#define FG_DKGRAY           0x08 // Dark gray foreground attribute
#define FG_LTBLUE           0x09 // Light blue foreground attribute
#define FG_LTGREEN          0x0A // Light green foreground attribute
#define FG_LTCYAN           0x0B // Light cyan foreground attribute
#define FG_LTRED            0x0C // Light red foreground attribute
#define FG_LTMAGENTA        0x0D // Light magenta foreground attribute
#define FG_YELLOW           0x0E // Yellow foreground attribute
#define FG_WHITE            0x0F // White foreground attribute


/* Background colors */
#define BG_BLACK            0x00 // Black background attribute
#define BG_BLUE             0x10 // Blue background attribute
#define BG_GREEN            0x20 // Green background attribute
#define BG_CYAN             0x30 // Cyan background attribute
#define BG_RED              0x40 // Red background attribute
#define BG_MAGENTA          0x50 // Magenta background attribute
#define BG_BROWN            0x60 // Brown background attribute
#define BG_LTGRAY           0x70 // Light gray background attribute
#define BG_BKBLACK          0x80 // Blinking black background attribute
#define BG_BKBLUE           0x90 // Blinking blue background attribute
#define BG_BKGREEN          0xA0 // Blinking green background attribute
#define BG_BKCYAN           0xB0 // Blinking cyan background attribute
#define BG_BKRED            0xC0 // Blinking red background attribute
#define BG_BKMAGENTA        0xD0 // Blinking magenta background attribute
#define BG_BKYELLOW         0xE0 // Blinking yellow background attribute
#define BG_BKWHITE          0xF0 // Blinking white background attribute

#define TTY_CURSOR_SHOW   0x00
#define TTY_CURSOR_HIDE   0x3F



/**
 * Clear the text-mode screen content and change color scheme
 *
 * @param color The color to which the screen will be set
 * @note If the color is NULL, the screen will be cleaned with the default scheme
 */
void tty_clear(uint8_t color);


/**
 * Print a character at the specified position with the given attribute
 *
 * @note - Supports newline, tabs and backspace characters
 * @note - Scrolls the screen if necessary
 * @exception - If the position is invalid, an error character 'E' will be printed at the bottom right corner
 *
 * @param character         The character to print
 * @param col               The column index of the position
 * @param row               The row index of the position
 * @param color             The color of the character
 *
 * @return The new offset of the cursor in video memory.
 */
int tty_plotc(char character, int col, int row, uint8_t color);


/**
 * Print a string at the specified location on the screen
 *
 * @note - If column and row are negative, the current cursor position will be used
 *
 * @param string  The string to print.
 * @param col     The column index of the position.
 * @param row     The row index of the position.
 * @param color   The color attribute of the message.
 */
int tty_plots(const char *string, int col, int row, uint8_t color);


/**
 * Print a string at the current cursor position on the screen.
 *
 * @param string The string to print.
 */
void tty_prints(const char *string);


/**
 * Print a string at the current cursor position on the screen
 *
 * @note Support ANSI color escape sequences
 *
 * @param string The string to print.
 */
void tty_printl(const char *string);


/**
 * Get and print all the avialible charaters from the BIOS charset
 */
void tty_probe(void);


/**
 * @brief Moves the hardware cursor to the specified position in the text mode console
 *
 * @param shape The new shape of the cursor.
 * @param col Number of columns to move right from current position (X offset).
 * @param row Number of rows to move down from current position (Y offset).
 *
 * @note Position is bound-checked against TEXTMODE_WIDTH and TEXTMODE_HEIGHT.
 * If the resulting position would be out of bounds, the function returns without
 * moving the cursor.
 */
void tty_cursor(uint8_t shape, uint16_t col, uint16_t row);


#endif /* DRIVER_SCREEN_H_ */
