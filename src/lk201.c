#include <stdlib.h>
#include <string.h>

#include "lk201.h"

static struct repeat_buffer repeat_buffers[NUM_REPEAT_BUFFERS];
static const struct repeat_buffer repeat_buffers_default[NUM_REPEAT_BUFFERS] = {
	{ .timeout = 500, .interval = 1000 / 30 },
	{ .timeout = 300, .interval = 1000 / 30 },
	{ .timeout = 500, .interval = 1000 / 40 },
	{ .timeout = 300, .interval = 1000 / 40 },
};

static struct division divisions[NUM_DIVISIONS];
static const struct division divisions_default[NUM_DIVISIONS] = {
	{ .mode = MODE_AUTO_REPEAT,   .buffer =  0 }, /* Main array */
	{ .mode = MODE_AUTO_REPEAT,   .buffer =  0 }, /* Keypad */
	{ .mode = MODE_AUTO_REPEAT,   .buffer =  1 }, /* Delete */
	{ .mode = MODE_DOWN_ONLY,     .buffer = -1 }, /* Return and tab */
	{ .mode = MODE_DOWN_ONLY,     .buffer = -1 }, /* Lock and compose */
	{ .mode = MODE_DOWN_UP,       .buffer = -1 }, /* Shift and control */
	{ .mode = MODE_AUTO_REPEAT,   .buffer =  1 }, /* Horizontal cursors */
	{ .mode = MODE_AUTO_REPEAT,   .buffer =  1 }, /* Vertical cursors */
	{ .mode = MODE_DOWN_UP,       .buffer = -1 }, /* Six editing keys */
	{ .mode = MODE_DOWN_UP,       .buffer = -1 }, /* Function keys 1 */
	{ .mode = MODE_DOWN_UP,       .buffer = -1 }, /* Function keys 2 */
	{ .mode = MODE_DOWN_UP,       .buffer = -1 }, /* Function keys 3 */
	{ .mode = MODE_DOWN_UP,       .buffer = -1 }, /* Function keys 4 */
	{ .mode = MODE_DOWN_UP,       .buffer = -1 }, /* Function keys 5 */
};

void
lk201_init_defaults(void)
{
	memcpy(repeat_buffers, repeat_buffers_default, sizeof(repeat_buffers));
	memcpy(divisions, divisions_default, sizeof(divisions));
}

struct repeat_buffer *
lk201_repeat_buffer_get(int repeat_buffer)
{
	return &repeat_buffers[repeat_buffer];
}

struct division *
lk201_division_get(int division)
{
	return &divisions[division];
}

struct division *
lk201_division_get_from_keycode(int keycode)
{
	int division = -1;
	if ((keycode >= 0x56) && (keycode <= 0x62)) {
		division = DIVISION_FUNCTION_KEYS_1;
	} else if ((keycode >= 0x63) && (keycode <= 0x6E)) {
		division = DIVISION_FUNCTION_KEYS_2;
	} else if ((keycode >= 0x6F) && (keycode <= 0x7A)) {
		division = DIVISION_FUNCTION_KEYS_3;
	} else if ((keycode >= 0x7B) && (keycode <= 0x7D)) {
		division = DIVISION_FUNCTION_KEYS_4;
	} else if ((keycode >= 0x7E) && (keycode <= 0x87)) {
		division = DIVISION_FUNCTION_KEYS_5;
	} else if ((keycode >= 0x88) && (keycode <= 0x90)) {
		division = DIVISION_SIX_EDITING_KEYS;
	} else if ((keycode >= 0x91) && (keycode <= 0xA5)) {
		division = DIVISION_KEYPAD;
	} else if ((keycode >= 0xA6) && (keycode <= 0xA8)) {
		division = DIVISION_HORIZONTAL_CURSORS;
	} else if ((keycode >= 0xA9) && (keycode <= 0xAC)) {
		division = DIVISION_VERTICAL_CURSORS;
	} else if ((keycode >= 0xAD) && (keycode <= 0xAF)) {
		division = DIVISION_SHIFT_AND_CTRL;
	} else if ((keycode >= 0xB0) && (keycode <= 0xB2)) {
		division = DIVISION_LOCK_AND_COMPOSE;
	} else if (keycode == 0xBC) {
		division = DIVISION_DELETE;
	} else if ((keycode >= 0xBD) && (keycode <= 0xBE)) {
		division = DIVISION_RETURN_AND_TAB;
	} else if ((keycode >= 0xBF) && (keycode <= 0xFF)) {
		division = DIVISION_MAIN_ARRAY;
	}

	return (division >= 0) ? &divisions[division] : NULL;
}

static int hid_to_lk201_map[] = {
	#include "lk201_map.txt"
};

int
lk201_keycode_get_from_hid(int hid)
{
	if (hid < (int)sizeof(hid_to_lk201_map)) {
		return hid_to_lk201_map[hid];
	} else {
		return 0x00;
	}
}

void
lk201_change_all_auto_repeat_to_down_only(void)
{
	for (int i = 0; i < NUM_DIVISIONS; i++) {
		struct division *division = &divisions[i];
		if (division->mode == MODE_AUTO_REPEAT) {
			division->mode = MODE_DOWN_ONLY;
		}
	}
}
