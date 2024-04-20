#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/dlist.h>

#include "vtbt.h"
#include "lk201.h"
#include "beeper.h"
#include "bluetooth.h"
#include "leds.h"
#include "metronome.h"
#include "uart.h"
#include "keyboard.h"

LOG_MODULE_REGISTER(vtbt, CONFIG_LOG_DEFAULT_LEVEL);

static bool test_mode = false;

static sys_dlist_t keys_down;

K_MSGQ_DEFINE(msgq, sizeof(struct event), 32, 4);

static struct event metronome_evt = { EVT_METRONOME, 0 , { 0 } };

static void
metronome(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	k_msgq_put(&msgq, &metronome_evt, K_NO_WAIT);
}

K_TIMER_DEFINE(metronome_timer, metronome, NULL);

static struct event hid_evt = { EVT_KEYBOARD, HID_REPORT_SIZE, { 0 } };

static void
hid_report_cb(const uint8_t *hid_report)
{
	memcpy(hid_evt.buf, hid_report, HID_REPORT_SIZE);
	k_msgq_put(&msgq, &hid_evt, K_NO_WAIT);
}

static void
send_power_on_test_result(void) {
	const unsigned char test_result[] = {
		SPECIAL_KEYBOARD_ID_FIRMWARE,
		SPECIAL_KEYBOARD_ID_HARDWARE,
		0x00, /* ERROR */
		0x00, /* KEYCODE */
	};
	uart_write(test_result, sizeof(test_result));
}

static struct event callback_evt = { EVT_HOST, 0 , { 0 } };

static void
uart_callback(uint8_t c)
{
	if (c & 0x80) {
		/* no more parameters */
		callback_evt.buf[callback_evt.size++] = c;
		k_msgq_put(&msgq, &callback_evt, K_NO_WAIT);
		callback_evt.size = 0;
	} else if (callback_evt.size < 3) {
		callback_evt.buf[callback_evt.size++] = c;
	}
}

static void
init_defaults(void)
{
	test_mode = false;
	lk201_init_defaults();
	keyboard_init_defaults();
	beeper_set_keyclick_volume(2);
	beeper_set_bell_volume(2);
}

static void
light_leds(const struct event *event)
{
	if (event->size != 2) {
		uart_write_byte(SPECIAL_INPUT_ERROR);
		metronome_resend();
		return;
	}

	for (int i = 0; i < 4; i++) {
		if (event->buf[1] & (1 << i)) {
			leds_set(i, 1);
		}
	}
}

static void
turn_off_leds(const struct event *event)
{
	if (event->size != 2) {
		uart_write_byte(SPECIAL_INPUT_ERROR);
		metronome_resend();
		return;
	}

	for (int i = 0; i < 4; i++) {
		if (event->buf[1] & (1 << i)) {
			leds_set(i, 0);
		}
	}
}

static void
disable_keyclick(const struct event *event)
{
	ARG_UNUSED(event);

	beeper_set_keyclick_volume(-1);
}

static void
enable_keyclick_set_volume(const struct event *event)
{
	if (event->size != 2) {
		uart_write_byte(SPECIAL_INPUT_ERROR);
		metronome_resend();
		return;
	}

	beeper_set_keyclick_volume(event->buf[1] & 0x07);
}

static void
disable_ctrl_keyclick(const struct event *event)
{
	ARG_UNUSED(event);

	keyboard_ctrl_keyclick_disable();
}

static void
enable_ctrl_keyclick(const struct event *event)
{
	ARG_UNUSED(event);

	keyboard_ctrl_keyclick_enable();
}

static void
sound_keyclick(const struct event *event)
{
	ARG_UNUSED(event);

	beeper_sound_keyclick();
}

static void
disable_bell(const struct event *event)
{
	ARG_UNUSED(event);

	beeper_set_bell_volume(-1);
}

static void
enable_bell_set_volume(const struct event *event)
{
	if (event->size != 2) {
		uart_write_byte(SPECIAL_INPUT_ERROR);
		metronome_resend();
		return;
	}

	beeper_set_bell_volume(event->buf[1] & 0x07);
}

static void
sound_bell(const struct event *event)
{
	ARG_UNUSED(event);

	beeper_sound_bell();
}

static void
request_keyboard_id(const struct event *event)
{
	ARG_UNUSED(event);

	const unsigned char keyboard_id[] = {
		SPECIAL_KEYBOARD_ID_FIRMWARE,
		SPECIAL_KEYBOARD_ID_HARDWARE,
	};
	uart_write(keyboard_id, sizeof(keyboard_id));
}

static void
jump_to_power_up(const struct event *event)
{
	ARG_UNUSED(event);

	init_defaults();
	send_power_on_test_result();
}

static void
jump_to_test_mode(const struct event *event)
{
	ARG_UNUSED(event);

	test_mode = true;

	uart_write_byte(SPECIAL_TEST_MODE_ACK);
}

static void
reinstate_defaults(const struct event *event)
{
	ARG_UNUSED(event);

	init_defaults();
}

static void
inhibit_keyboard_transmission(const struct event *event)
{
	ARG_UNUSED(event);

	leds_set(LED_LOCK, 1);

	uart_write_byte(SPECIAL_KBD_LOCKED_ACK);
	uart_flush();
	uart_lock();
}

#define SYS_DLIST_PEEK_TAIL_CONTAINER(__dl, __cn, __n) \
	SYS_DLIST_CONTAINER(sys_dlist_peek_tail(__dl), __cn, __n)

#define SYS_DLIST_PEEK_PREV_CONTAINER(__dl, __cn, __n) \
	((__cn != NULL) ? \
	 SYS_DLIST_CONTAINER(sys_dlist_peek_prev(__dl, &(__cn->__n)),   \
	                              __cn, __n) : NULL)

#define SYS_DLIST_FOR_EACH_CONTAINER_REVERSE(__dl, __cn, __n)           \
	for (__cn = SYS_DLIST_PEEK_TAIL_CONTAINER(__dl, __cn, __n);     \
	     __cn != NULL;                                              \
	     __cn = SYS_DLIST_PEEK_PREV_CONTAINER(__dl, __cn, __n))

static void
resume_keyboard_transmission(const struct event *event)
{
	ARG_UNUSED(event);

	leds_set(LED_LOCK, 0);

	uart_unlock();
	if (uart_overflow_get()) {
		uart_write_byte(SPECIAL_OUTPUT_ERROR);
	}

	/* Send unsent key down in reverse order */
	struct key_down *cn;
	SYS_DLIST_FOR_EACH_CONTAINER_REVERSE(&keys_down, cn, node) {
		if (cn->sent) {
			continue;
		}

		uart_write_byte(cn->keycode);
		cn->sent = true;
	}

	metronome_resend();
}

static void
temporary_auto_repeat_inhibit(const struct event *event)
{
	ARG_UNUSED(event);

	struct key_down *cn;
	SYS_DLIST_FOR_EACH_CONTAINER(&keys_down, cn, node) {
		struct division *division =
			lk201_division_get_from_keycode(cn->keycode);
		if (division == NULL) {
			continue;
		} else if (cn->inhibit_auto_repeat == true) {
			continue;
		} else if (division->mode == MODE_AUTO_REPEAT) {
			cn->inhibit_auto_repeat = true;
			break;
		}
	}
}

static void
enable_auto_repeat_across_keyboard(const struct event *event)
{
	ARG_UNUSED(event);

	metronome_auto_repeat_enable();
}

static void
disable_auto_repeat_across_keyboard(const struct event *event)
{
	ARG_UNUSED(event);

	metronome_auto_repeat_disable();
}

static void
change_all_auto_repeat_to_down_only(const struct event *event)
{
	ARG_UNUSED(event);

	lk201_change_all_auto_repeat_to_down_only();
}

static void
test_mode_jump_to_power_up(const struct event *event)
{
	ARG_UNUSED(event);

	init_defaults();
	send_power_on_test_result();
}

static void
peripheral_command(const struct event *event)
{
	switch (event->buf[0]) {
		/* FLOW CONTROL */
		case COMMAND_RESUME_KEYBOARD_TRANSMISSION:
			resume_keyboard_transmission(event);
			break;
		case COMMAND_INHIBIT_KEYBOARD_TRANSMISSION:
			inhibit_keyboard_transmission(event);
			break;
		/* INDICATORS */
		case COMMAND_TURN_OFF_LEDS:
			turn_off_leds(event);
			break;
		case COMMAND_LIGHT_LEDS:
			light_leds(event);
			break;
		/* AUDIO */
		case COMMAND_DISABLE_KEYCLICK:
			disable_keyclick(event);
			break;
		case COMMAND_ENABLE_KEYCLICK_SET_VOLUME:
			enable_keyclick_set_volume(event);
			break;
		case COMMAND_DISABLE_CTRL_KEYCLICK:
			disable_ctrl_keyclick(event);
			break;
		case COMMAND_ENABLE_CTRL_KEYCLICK:
			enable_ctrl_keyclick(event);
			break;
		case COMMAND_SOUND_KEYCLICK:
			sound_keyclick(event);
			break;
		case COMMAND_DISABLE_BELL:
			disable_bell(event);
			break;
		case COMMAND_ENABLE_BELL_SET_VOLUME:
			enable_bell_set_volume(event);
			break;
		case COMMAND_SOUND_BELL:
			sound_bell(event);
			break;
		/* AUTO-REPEAT */
		case COMMAND_TEMPORARY_AUTO_REPEAT_INHIBIT:
			temporary_auto_repeat_inhibit(event);
			break;
		case COMMAND_ENABLE_AUTO_REPEAT_ACROSS_KEYBOARD:
			enable_auto_repeat_across_keyboard(event);
			break;
		case COMMAND_DISABLE_AUTO_REPEAT_ACROSS_KEYBOARD:
			disable_auto_repeat_across_keyboard(event);
			break;
		case COMMAND_CHANGE_ALL_AUTO_REPEAT_TO_DOWN_ONLY:
			change_all_auto_repeat_to_down_only(event);
			break;
		/* OTHER */
		case COMMAND_REQUEST_KEYBOARD_ID:
			request_keyboard_id(event);
			break;
		case COMMAND_JUMP_TO_POWER_UP:
			jump_to_power_up(event);
			break;
		case COMMAND_JUMP_TO_TEST_MODE:
			jump_to_test_mode(event);
			break;
		case COMMAND_REINSTATE_DEFAULTS:
			reinstate_defaults(event);
			break;
		default:
			uart_write_byte(SPECIAL_INPUT_ERROR);
			break;
	}
}

static void
transmission_command(const struct event *event)
{
	int division = (event->buf[0] >> 3) & 0x0f;
	if (division == 0x0f) {
		if (event->size != 3) {
			uart_write_byte(SPECIAL_INPUT_ERROR);
			metronome_resend();
			return;
		}
		int buffer = (event->buf[0] >> 1) & 0x03;
		int timeout = ((event->buf[1]) & 0x7f) * 5;
		int interval = 1000 / ((event->buf[2]) & 0x7f);
		lk201_repeat_buffer_get(buffer)->timeout = timeout;
		lk201_repeat_buffer_get(buffer)->interval = interval;
	} else if (division == 0x00) {
		uart_write_byte(SPECIAL_INPUT_ERROR);
		metronome_resend();
		return;
	} else {
		int mode = ((event->buf[0]) >> 1) & 0x03;
		lk201_division_get(division-1)->mode = mode;
		if (mode == MODE_AUTO_REPEAT) {
			if (event->size == 2) {
				int buffer = event->buf[1] & 0x7f;
				lk201_division_get(division-1)->buffer = buffer;
			} else {
				/* Default buffer? */
				lk201_division_get(division-1)->buffer = 0;
			}
		}

		uart_write_byte(SPECIAL_MODE_CHANGE_ACK);
		metronome_resend();
	}
}

static void
host_event(const struct event *event)
{
	if (event->size == 0) {
		return;
	}

	if (test_mode) {
		if (event->buf[0] == TEST_MODE_COMMAND_JUMP_TO_POWER_UP) {
			test_mode_jump_to_power_up(event);
		}
	} else {
		if (event->buf[0] & 0x01) {
			peripheral_command(event);
		} else {
			transmission_command(event);
		}
	}
}

static void
handle_events(void)
{
	struct event event;

	while (k_msgq_get(&msgq, &event, K_FOREVER) == 0) {
		switch (event.source) {
			case EVT_HOST:
				host_event(&event);
				break;
			case EVT_METRONOME:
				metronome_event(&keys_down, &event);
				break;
			case EVT_KEYBOARD:
				keyboard_event(&keys_down, &event);
				break;
			default:
				break;
		}
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
		lk201_repeat_buffer_get(i)->timeout = 300;
		lk201_repeat_buffer_get(i)->interval = 30;
	}

	ret = uart_init();
	if (ret < 0) {
		LOG_ERR("UART init failed: %d", ret);
		return -1;
	}

	ret = uart_set_rx_callback(uart_callback);
	if (ret < 0) {
		LOG_ERR("UART set rx callback failed: %d", ret);
		return -1;
	}

	send_power_on_test_result();
	uart_write_byte(SPECIAL_INPUT_ERROR);
	uart_write_byte(SPECIAL_MODE_CHANGE_ACK);

	k_timer_start(&metronome_timer, K_MSEC(1), K_MSEC(1));

	ret = bluetooth_listen(hid_report_cb);
	if (ret < 0) {
		LOG_ERR("Bluetooth listening failed");
		return -1;
	}

	handle_events();
}
