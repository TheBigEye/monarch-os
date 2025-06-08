#ifndef _DRIVER_KEYBOARD_H
#define _DRIVER_KEYBOARD_H 1

#include "../../common/common.h"


#define KBD_PORT 0x60
#define KBD_CTRL 0x64

#define KEY_BACKSPACE       0x0E
#define KEY_CAPSLOCK        0x3A

#define KEY_RETURN          0x1C
#define KEY_ENTER           0xFC

#define KEY_L_SHIFT         0x2A
#define KEY_R_SHIFT         0x36

#define KEY_L_CTRL          0x1D
#define KEY_R_CTRL          0xFD

#define KEY_L_ALT           0x38
#define KEY_R_ALT           0xF8

#define KEY_SPACE           0x39
#define KEY_DELETE          0x53

#define KEY_TAB             0x0F
#define KEY_ESC             0x01

#define KEY_F01             0x3B
#define KEY_F02             0x3C
#define KEY_F03             0x3D
#define KEY_F04             0x3E
#define KEY_F05             0x3F
#define KEY_F06             0x40
#define KEY_F07             0x41
#define KEY_F08             0x42
#define KEY_F09             0x43
#define KEY_F10             0x44


extern volatile uint8_t scancode;

extern volatile bool keyboard_enabled;
extern volatile bool capslock_enabled;

extern const uint8_t key_layout[128][2];


/**
 * Initializes the keyboard by registering the keyboard callback function.
 */
void initializeKeyboard(void);


void disableKeyboard(void);


#endif /* _DRIVER_KEYBOARD_H */
