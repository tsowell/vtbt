#include <string.h>

#include <zephyr/kernel.h>

#include "vtbt.h"
#include "keyboard.h"
#include "uart.h"
#include "beeper.h"
#include "metronome.h"
#include "lk201.h"

K_MEM_SLAB_DEFINE(
	keys_down_slab,
	ROUND_UP(sizeof(struct key_down), 4),
	16, 4
);

static uint8_t last_report[HID_REPORT_SIZE] = { 0x00 };

static bool
is_in_report(int keycode, const uint8_t *report)
{
	for (int i = HID_REPORT_FIRST_KEY; i < HID_REPORT_SIZE; i++) {
		if (keycode == report[i]) {
			return true;
		}
	}

	return false;
}

static void
lk201_key_down(sys_dlist_t *keys_down, int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	int ret;
	struct key_down *node;
	ret = k_mem_slab_alloc(&keys_down_slab, (void **)&node, K_NO_WAIT);
	if (ret < 0) {
		return;
	}

	sys_dnode_init(&node->node);

	node->keycode = keycode;
	node->time = k_uptime_get();
	node->repeating = false;
	node->inhibit_auto_repeat = false;

	sys_dlist_prepend(keys_down, &node->node);

	int sent = uart_write_byte(keycode);
	node->sent = sent > 0;
	if (sent > 0) {
		beeper_sound_keyclick();
	}

	metronome_resend();
}

/* Track Down/Up keys released in this report */
static int up_down_ups[16];
static int up_down_ups_count = 0;

/* Send codes for released Down/Up keys (or ALL UPS if none left pressed ) */
static void
send_up_down_ups(sys_dlist_t *keys_down) {
	if (up_down_ups_count <= 0) {
		return;
	}

	struct key_down *cn;
	bool other_down_up = false;
	SYS_DLIST_FOR_EACH_CONTAINER(keys_down, cn, node) {
		struct division *division =
			lk201_division_get_from_keycode(cn->keycode);
		if (division == NULL) {
			continue;
		} else if (division->mode == MODE_DOWN_UP) {
			other_down_up = true;
			break;
		}
	}

	if (!other_down_up) {
		uart_write_byte(SPECIAL_ALL_UPS);
		metronome_resend();
	} else {
		while (up_down_ups_count--) {
			uart_write_byte(up_down_ups[up_down_ups_count]);
			metronome_resend();
		}
	}
}

static void
lk201_key_up(sys_dlist_t *keys_down, int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	struct key_down *cn, *cns;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(keys_down, cn, cns, node) {
		if (cn->keycode == keycode) {
			sys_dlist_remove(&cn->node);
			k_mem_slab_free(&keys_down_slab, (void *)cn);
			break;
		}
	}

	struct division *division = lk201_division_get_from_keycode(keycode);
	if (division == NULL) {
		return;
	} else if (division->mode == MODE_DOWN_UP) {
		up_down_ups[up_down_ups_count++] = keycode;
	}
}

void
keyboard_event(sys_dlist_t *keys_down, const struct event *event)
{
	const uint8_t *this_report = event->buf;

	const uint8_t this_modifiers = this_report[0];
	const uint8_t last_modifiers = last_report[0];

	up_down_ups_count = 0;

	for (int i = 0; i < HID_REPORT_SIZE; i++) {
		int key = 0;
		if ((i == 0) || (i == 4)) {
			key = LK201_CTRL; /* Ctrl */
		} else if ((i == 1) || (i == 5)) {
			key = LK201_SHIFT; /* Shift*/
		} else {
			continue;
		}
		if ((this_modifiers & (1 << i)) &&
		    !(last_modifiers & (1 << i))) {
			lk201_key_down(keys_down, key);
		}
		if ((last_modifiers & (1 << i)) &&
		    !(this_modifiers & (1 << i))) {
			lk201_key_up(keys_down, key);
		}
	}

	for (int i = HID_REPORT_FIRST_KEY; i < HID_REPORT_SIZE; i++) {
		if ((this_report[i] != 0x00) &&
		    !is_in_report(this_report[i], last_report)) {
			int keycode = lk201_keycode_get_from_hid(this_report[i]);
			lk201_key_down(keys_down, keycode);
		}
		if ((last_report[i] != 0x00) &&
		    !is_in_report(last_report[i], this_report)) {
			int keycode = lk201_keycode_get_from_hid(last_report[i]);
			lk201_key_up(keys_down, keycode);
		}
	}

	memcpy(last_report, this_report, sizeof(last_report));

	send_up_down_ups(keys_down);
}
