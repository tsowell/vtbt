#ifndef LK201_H
#define LK201_H

#include <stdint.h>

#define NUM_KEYS 256

#define NUM_DIVISIONS 14

#define NUM_REPEAT_BUFFERS 4

#define MODE_DOWN_ONLY         0x00
#define MODE_AUTO_REPEAT       0x01
#define MODE_DOWN_UP           0x03

/* These are one less than their one-indexed protocol values. */
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
#define SPECIAL_KEY_DOWN_ON_POWER_UP_ERROR  0x3d
/* Self test failed */
#define SPECIAL_POWER_UP_SELF_TEST_ERROR    0x3e
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

/* FLOW CONTROL */
#define COMMAND_RESUME_KEYBOARD_TRANSMISSION         0x8b
#define COMMAND_INHIBIT_KEYBOARD_TRANSMISSION        0x89

/* INDICATORS */
#define COMMAND_LIGHT_LEDS                           0x13
#define COMMAND_TURN_OFF_LEDS                        0x11

/* AUDIO */
#define COMMAND_DISABLE_KEYCLICK                     0x99
#define COMMAND_ENABLE_KEYCLICK_SET_VOLUME           0x1b
#define COMMAND_DISABLE_CTRL_KEYCLICK                0xb9
#define COMMAND_ENABLE_CTRL_KEYCLICK                 0xbb
#define COMMAND_SOUND_KEYCLICK                       0x9f
#define COMMAND_DISABLE_BELL                         0xa1
#define COMMAND_ENABLE_BELL_SET_VOLUME               0x23
#define COMMAND_SOUND_BELL                           0xa7

/* AUTO-REPEAT */
#define COMMAND_TEMPORARY_AUTO_REPEAT_INHIBIT        0xc1
#define COMMAND_ENABLE_AUTO_REPEAT_ACROSS_KEYBOARD   0xe3
#define COMMAND_DISABLE_AUTO_REPEAT_ACROSS_KEYBOARD  0xe1
#define COMMAND_CHANGE_ALL_AUTO_REPEAT_TO_DOWN_ONLY  0xd9

/* OTHER */
#define COMMAND_REQUEST_KEYBOARD_ID                  0xab
#define COMMAND_JUMP_TO_POWER_UP                     0xfd
#define COMMAND_JUMP_TO_TEST_MODE                    0xcb
#define COMMAND_REINSTATE_DEFAULTS                   0xd3

/* TEST MODE */
#define TEST_MODE_COMMAND_JUMP_TO_POWER_UP           0x80

/* Power-up transmission
 * Byte 1: KBID (firmware) 0x01
 * Byte 2: KBID (hardware) 0x00
 * Byte 3: ERROR (0x3d or 0x3e)
 * Byte 4: KEYCODE (0x00 for no key down)
 */

#define LK201_SHIFT  0xae
#define LK201_CTRL   0xaf

struct repeat_buffer {
	/* Milliseconds before auto-repeating. */
	int timeout;
	/* Milliseconds between metronome codes. (This is metronome codes per
	 * second in the spec). */
	int interval;
};

struct division {
	int mode;
	int buffer;
};

void lk201_init_defaults(void);
struct repeat_buffer *lk201_repeat_buffer_get(int repeat_buffer);
struct division *lk201_division_get(int division);
struct division *lk201_division_get_from_keycode(int keycode);
int lk201_keycode_get_from_hid(int hid);
void lk201_change_all_auto_repeat_to_down_only(void);

#endif /* LK201_H */
