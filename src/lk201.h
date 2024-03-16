#ifndef LK201_H
#define LK201_H

#include <stdint.h>

#define NUM_KEYS 256

#define NUM_DIVISIONS 14

#define NUM_REPEAT_BUFFERS 4

#define MODE_DOWN_ONLY         0x00
#define MODE_AUTO_REPEAT       0x01
#define MODE_DOWN_UP           0x03

/* These are one less than their protocol values */
#define DIVISION_MAIN_ARRAY              0
#define DIVISION_KEYPAD                  1
#define DIVISION_DELETE                  2
#define DIVISION_RETURN_AND_TAB          3
#define DIVISION_LOCK_AND_COMPOSE        4
#define DIVISION_SHIFT_AND_CTRL          5
#define DIVISION_HORIZONTAL_CURSORS      6
#define DIVISION_VERTICAL_CURSORS        7
#define DIVISION_SIX_EDITING_KEYS        8
#define DIVISION_FUNCTION_KEYS_1         9
#define DIVISION_FUNCTION_KEYS_2        10
#define DIVISION_FUNCTION_KEYS_3        11
#define DIVISION_FUNCTION_KEYS_4        12
#define DIVISION_FUNCTION_KEYS_5        13

/* Keyboard IDs */
#define SPECIAL_KEYBOARD_ID_FIRMWARE        0x01
#define SPECIAL_KEYBOARD_ID_HARDWARE        0x00
/* Key down during self-test */
#define SPECIAL_KEY_DOWN_ON_POWER_UP_ERROR  0x3D
/* Self test failed */
#define SPECIAL_POWER_UP_SELF_TEST_ERROR    0x3E
/* Down/up key was released and no other down/up keys pressed */
#define SPECIAL_ALL_UPS                     0xb3
/* Auto-repeat interval has passed with key down */
#define SPECIAL_METRONOME                   0xb4
/* Output buffer overflow during keyboard inhibit */
#define SPECIAL_OUTPUT_ERROR                0xb5
/* Received invalid command or parameters */
#define SPECIAL_INPUT_ERROR                 0xb6
/* Keyboard received inhibit transmission command */
#define SPECIAL_KBD_LOCKED_ACK              0xb7
/* Keyboard has entered test mode */
#define SPECIAL_TEST_MODE_ACK               0xb8
/* Next byte is keycode for a down key in a division that changed to down/up */
#define SPECIAL_PREFIX_TO_KEYS_DOWN         0xb9
/* Keyboard has processed a mode change command */
#define SPECIAL_MODE_CHANGE_ACK             0xba
#define SPECIAL_RESERVED                    0x7f

/* Power-up transmission
 * Byte 1: KBID (firmware) 0x01
 * Byte 2: KBID (hardware) 0x00
 * Byte 3: ERROR (0x3D or 0x3E)
 * Byte 4: KEYCODE (0x00 for no key down
 */

#define LK201_SHIFT  0xAE
#define LK201_CTRL   0xAF

struct repeat_buffer {
	/* Milliseconds before auto-repeating */
	int timeout;
	/* Metronome codes per second */
	int interval;
};

struct division {
	int mode;
	int buffer;
};

void lk201_main(void);
void lk201_handle_hid_report(const uint8_t *this_report);

#endif /* LK201_H */
