#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/dlist.h>

#include "config.h"
#include "lk201.h"
#include "beeper.h"
#include "bluetooth.h"
#include "leds.h"
#include "uart.h"
#include "lk201_uart.h"

#define uart_write(args...) do { \
	_Pragma("GCC error \"use lk201_uart_write!\"") } while (0)
#define uart_write_byte(args...) do { \
	_Pragma("GCC error \"use lk201_uart_write_byte!\"") } while (0)

LOG_MODULE_REGISTER(lk201, CONFIG_LOG_DEFAULT_LEVEL);

static struct repeat_buffer repeat_buffers[NUM_REPEAT_BUFFERS];
static struct repeat_buffer repeat_buffers_default[NUM_REPEAT_BUFFERS] = {
	{ .timeout = 500, .interval = 30 },
	{ .timeout = 300, .interval = 30 },
	{ .timeout = 500, .interval = 40 },
	{ .timeout = 300, .interval = 40 },
};

static struct division divisions[NUM_DIVISIONS];
static struct division divisions_default[NUM_DIVISIONS] = {
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

K_MUTEX_DEFINE(mutex);

static sys_dlist_t keys_down;

struct keys_down_node {
	sys_dnode_t node;
	int keycode;
	int64_t time;
	bool repeating;
	bool sent;
};

K_MEM_SLAB_DEFINE(
	keys_down_slab,
	ROUND_UP(sizeof(struct keys_down_node), 4),
	16, 4
);

/* hid_to_lk201_map */
#include "lk201_map.c"

static int
hid_to_lk201(int hid)
{
	if (hid < sizeof(hid_to_lk201_map)) {
		return hid_to_lk201_map[hid];
	} else {
		return 0x00;
	}
}

static struct division *
lk201_keycode_to_division(int keycode)
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

static int repeating_keycode = 0;
static int repeating_next = 0;
static bool repeating_resend = false;

static void
metronome_ms(struct k_timer *timer_id)
{
	k_mutex_lock(&mutex, K_FOREVER);

	struct keys_down_node *repeating = NULL;
	struct division *division = NULL;
	struct keys_down_node *cn;
	SYS_DLIST_FOR_EACH_CONTAINER(&keys_down, cn, node) {
		division = lk201_keycode_to_division(cn->keycode);
		if (division == NULL) {
			continue;
		} else if (division->mode == MODE_AUTO_REPEAT) {
			repeating = cn;
			break;
		}
	}

	if (repeating == NULL) {
		repeating_keycode = 0;
		repeating_resend = false;
		k_mutex_unlock(&mutex);
		return;
	}

	int64_t now = k_uptime_get();

	if (repeating_keycode != repeating->keycode) {
		int timeout = repeat_buffers[division->buffer].timeout;
		if ((now - repeating->time) > timeout) {
			int interval =
				repeat_buffers[division->buffer].interval;

			if (repeating->repeating && repeating_keycode != 0) {
				lk201_uart_write_byte(repeating->keycode);
			} else {
				lk201_uart_write_byte(SPECIAL_METRONOME);
			}
			repeating_keycode = repeating->keycode;
			repeating_next = now + interval;
			repeating_resend = false;
			repeating->repeating = true;
		}
		k_mutex_unlock(&mutex);
		return;
	}

	if ((repeating_next - now) < 0) {
		int interval =
			repeat_buffers[division->buffer].interval;
		repeating_next = now + interval;
		if (repeating_resend) {
			repeating_resend = false;
			lk201_uart_write_byte(repeating->keycode);
		} else {
			lk201_uart_write_byte(SPECIAL_METRONOME);
		}
	}

	k_mutex_unlock(&mutex);
}

struct k_timer metronome_timer;
K_TIMER_DEFINE(metronome_timer, metronome_ms, NULL);

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
lk201_key_down(int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	int ret;
	struct keys_down_node *node;
	ret = k_mem_slab_alloc(&keys_down_slab, (void **)&node, K_NO_WAIT);
	if (ret < 0) {
		LOG_ERR("Slab alloc failed: %d", ret);
		return;
	}

	sys_dnode_init(&node->node);

	node->keycode = keycode;
	node->time = k_uptime_get();
	node->repeating = false;

	sys_dlist_prepend(&keys_down, &node->node);

	int sent = lk201_uart_write_byte(keycode);
	node->sent = sent > 0;

	repeating_resend = true;
}

/* Track Down/Up keys released in this report */
static int up_down_ups[16];
static int up_down_ups_count = 0;

/* Send codes for released Down/Up keys (or ALL UPS if none left pressed ) */
static void
send_up_down_ups(void) {
	if (up_down_ups_count <= 0) {
		return;
	}

	struct keys_down_node *cn;
	bool other_down_up = false;
	SYS_DLIST_FOR_EACH_CONTAINER(&keys_down, cn, node) {
		struct division *division =
			lk201_keycode_to_division(cn->keycode);
		if (division == NULL) {
			continue;
		} else if (division->mode == MODE_DOWN_UP) {
			other_down_up = true;
			break;
		}
	}

	if (!other_down_up) {
		lk201_uart_write_byte(SPECIAL_ALL_UPS);
		repeating_resend = true;
	} else {
		while (up_down_ups_count--) {
			lk201_uart_write_byte(up_down_ups[up_down_ups_count]);
			repeating_resend = true;
		}
	}
}

static void
lk201_key_up(int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	struct keys_down_node *cn, *cns;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&keys_down, cn, cns, node) {
		if (cn->keycode == keycode) {
			sys_dlist_remove(&cn->node);
			k_mem_slab_free(&keys_down_slab, (void *)cn);
			break;
		}
	}

	struct division *division = lk201_keycode_to_division(keycode);
	if (division == NULL) {
		return;
	} else if (division->mode == MODE_DOWN_UP) {
		up_down_ups[up_down_ups_count++] = keycode;
	}
}

void
hid_report_cb(const uint8_t *this_report)
{
	k_mutex_lock(&mutex, K_FOREVER);

	const uint8_t this_modifiers = this_report[0];
	const uint8_t last_modifiers = last_report[0];

	up_down_ups_count = 0;

	for (int i = 0; i < 8; i++) {
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
			lk201_key_down(key);
		}
		if ((last_modifiers & (1 << i)) &&
		    !(this_modifiers & (1 << i))) {
			lk201_key_up(key);
		}
	}

	for (int i = HID_REPORT_FIRST_KEY; i < HID_REPORT_SIZE; i++) {
		if ((this_report[i] != 0x00) &&
		    !is_in_report(this_report[i], last_report)) {
			lk201_key_down(hid_to_lk201(this_report[i]));
		}
		if ((last_report[i] != 0x00) &&
		    !is_in_report(last_report[i], this_report)) {
			lk201_key_up(hid_to_lk201(last_report[i]));
		}
	}

	memcpy(last_report, this_report, sizeof(last_report));

	send_up_down_ups();

	k_mutex_unlock(&mutex);
}

static void
send_power_on_test_result(void) {
	const unsigned char test_result[] = {
		SPECIAL_KEYBOARD_ID_FIRMWARE,
		SPECIAL_KEYBOARD_ID_HARDWARE,
		0x00, /* ERROR */
		0x00, /* KEYCODE */
	};
	lk201_uart_write(test_result, sizeof(test_result));
}

struct message {
	uint8_t size;
	uint8_t buf[4];
};

K_MSGQ_DEFINE(uart_msgq, sizeof(struct message), 10, 4);

static struct message callback_message = { 0 , { 0 } };

static void
uart_callback(uint8_t c)
{
	if (c & 0x80) {
		/* no more parameters */
		callback_message.buf[callback_message.size++] = c;
		k_msgq_put(&uart_msgq, &callback_message, K_NO_WAIT);
		callback_message.size = 0;
	} else if (callback_message.size < (sizeof(callback_message.buf) - 1)) {
		callback_message.buf[callback_message.size++] = c;
	}
}

static void
init_defaults(void)
{
	memcpy(repeat_buffers, repeat_buffers_default, sizeof(repeat_buffers));
	memcpy(divisions, divisions_default, sizeof(divisions));
	beeper_set_keyclick_volume(-1);
	beeper_set_bell_volume(-1);
}

static void
handle_light_leds(const struct message *message)
{
	if (message->size != 2) {
		lk201_uart_write_byte(SPECIAL_INPUT_ERROR);
		repeating_resend = true;
		return;
	}

	for (int i = 0; i < 4; i++) {
		if (message->buf[1] & (1 << i)) {
			leds_set(i, 1);
		}
	}

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_turn_off_leds(const struct message *message)
{
	if (message->size != 2) {
		lk201_uart_write_byte(SPECIAL_INPUT_ERROR);
		repeating_resend = true;
		return;
	}

	for (int i = 0; i < 4; i++) {
		if (message->buf[1] & (1 << i)) {
			leds_set(i, 0);
		}
	}

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_disable_keyclick(const struct message *message)
{
	beeper_set_keyclick_volume(-1);

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_enable_keyclick_set_volume(const struct message *message)
{
	if (message->size != 2) {
		lk201_uart_write_byte(SPECIAL_INPUT_ERROR);
		repeating_resend = true;
		return;
	}

	beeper_set_keyclick_volume(message->buf[1] & 0x07);

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_sound_keyclick(const struct message *message)
{
	beeper_sound_keyclick();

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_disable_bell(const struct message *message)
{
	beeper_set_bell_volume(-1);

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_enable_bell_set_volume(const struct message *message)
{
	if (message->size != 2) {
		lk201_uart_write_byte(SPECIAL_INPUT_ERROR);
		repeating_resend = true;
		return;
	}

	beeper_set_bell_volume(message->buf[1] & 0x07);

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_sound_bell(const struct message *message)
{
	beeper_sound_bell();

	lk201_uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
	repeating_resend = true;
}

static void
handle_jump_to_power_up(const struct message *message)
{
	send_power_on_test_result();
}

static void
handle_reinstate_defaults(const struct message *message)
{
	init_defaults();
}

static void
handle_inhibit_keyboard_transmission(const struct message *message)
{
	leds_set(LED_LOCK, 1);

	lk201_uart_write_byte(SPECIAL_KBD_LOCKED_ACK);

	lk201_uart_lock();
}

static void
handle_resume_keyboard_transmission(const struct message *message)
{
	leds_set(LED_LOCK, 0);

	lk201_uart_unlock();

	/* Send unsent key down messages in reverse order */

	unsigned char buf[16];
	int count = 0;

	struct keys_down_node *cn, *cns;
	SYS_DLIST_FOR_EACH_CONTAINER(&keys_down, cn, node) {
		if (cn->sent) {
			continue;
		}

		buf[count++] = cn->keycode;
		cn->sent = true;
	}

	while (count > 0) {
		lk201_uart_write_byte(buf[--count]);
	}

	repeating_resend = true;
}

static void
handle_peripheral_command(const struct message *message)
{
	switch (message->buf[0]) {
		/* FLOW CONTROL */
		case COMMAND_RESUME_KEYBOARD_TRANSMISSION:
			handle_resume_keyboard_transmission(message);
			break;
		case COMMAND_INHIBIT_KEYBOARD_TRANSMISSION:
			handle_inhibit_keyboard_transmission(message);
			break;
		/* INDICATORS */
		case COMMAND_LIGHT_LEDS:
			handle_light_leds(message);
			break;
		case COMMAND_TURN_OFF_LEDS:
			handle_turn_off_leds(message);
			break;
		/* AUDIO */
		case COMMAND_DISABLE_KEYCLICK:
			handle_disable_keyclick(message);
			break;
		case COMMAND_ENABLE_KEYCLICK_SET_VOLUME:
			handle_enable_keyclick_set_volume(message);
			break;
		case COMMAND_SOUND_KEYCLICK:
			handle_sound_keyclick(message);
			break;
		case COMMAND_DISABLE_BELL:
			handle_disable_bell(message);
			break;
		case COMMAND_ENABLE_BELL_SET_VOLUME:
			handle_enable_bell_set_volume(message);
			break;
		case COMMAND_SOUND_BELL:
			handle_sound_bell(message);
			break;
		/* OTHER */
		case COMMAND_JUMP_TO_POWER_UP:
			handle_jump_to_power_up(message);
			break;
		case COMMAND_REINSTATE_DEFAULTS:
			handle_reinstate_defaults(message);
			break;
		default:
			break;
	}
}

static void
handle_transmission_command(const struct message *message)
{
}

static void
handle_message(const struct message *message)
{
	k_mutex_lock(&mutex, K_FOREVER);

	if (message->size == 0) {
		k_mutex_unlock(&mutex);
		return;
	}

	if (message->buf[0] & 0x01) {
		handle_peripheral_command(message);
	} else {
		handle_transmission_command(message);
	}

	k_mutex_unlock(&mutex);
}

static void
handle_messages(void)
{
	struct message message;

	while (k_msgq_get(&uart_msgq, &message, K_FOREVER) == 0) {
		handle_message(&message);
	}
}

int
main(void)
{
	int ret;

	sys_dlist_init(&keys_down);

	init_defaults();

	leds_init();
	beeper_init();

	for (int i = 0; i < NUM_REPEAT_BUFFERS; i++) {
		repeat_buffers[i].timeout = 300;
		repeat_buffers[i].interval = 30;
	}

	ret = uart_init();
	if (ret < 0) {
		LOG_ERR("UART init failed: %d", ret);
		return -1;
	}

	send_power_on_test_result();

	k_timer_start(&metronome_timer, K_MSEC(1), K_MSEC(1));

	ret = uart_set_rx_callback(uart_callback);
	if (ret < 0) {
		LOG_ERR("UART set rx callback failed: %d", ret);
		return -1;
	}

	ret = bluetooth_listen(hid_report_cb);
	if (ret < 0) {
		LOG_ERR("Bluetooth listening failed");
		return -1;
	}

	handle_messages();
}
