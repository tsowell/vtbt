#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include "lk201.h"
#include "bluetooth.h"
#include "config.h"

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_vt_uart)

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

static const struct gpio_dt_spec uart_tx_enable =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(uart_tx_enable), gpios, {0});

#define NUM_LEDS 4
static const struct gpio_dt_spec leds[NUM_LEDS] = {
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led0), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led1), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led2), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(led3), gpios, {0}),
};

static const struct pwm_dt_spec pwm_beeper0 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_beeper0));

LOG_MODULE_REGISTER(lk201, CONFIG_LOG_DEFAULT_LEVEL);

static int keys[NUM_KEYS];

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

static int
lk201_keycode_to_division(int keycode)
{
	if ((keycode >= 0x56) && (keycode <= 0x62)) {
		return DIVISION_FUNCTION_KEYS_1;
	} else if ((keycode >= 0x63) && (keycode <= 0x6E)) {
		return DIVISION_FUNCTION_KEYS_2;
	} else if ((keycode >= 0x6F) && (keycode <= 0x7A)) {
		return DIVISION_FUNCTION_KEYS_3;
	} else if ((keycode >= 0x7B) && (keycode <= 0x7D)) {
		return DIVISION_FUNCTION_KEYS_4;
	} else if ((keycode >= 0x7E) && (keycode <= 0x87)) {
		return DIVISION_FUNCTION_KEYS_5;
	} else if ((keycode >= 0x88) && (keycode <= 0x90)) {
		return DIVISION_SIX_EDITING_KEYS;
	} else if ((keycode >= 0x91) && (keycode <= 0xA5)) {
		return DIVISION_KEYPAD;
	} else if ((keycode >= 0xA6) && (keycode <= 0xA8)) {
		return DIVISION_HORIZONTAL_CURSORS;
	} else if ((keycode >= 0xA9) && (keycode <= 0xAC)) {
		return DIVISION_VERTICAL_CURSORS;
	} else if ((keycode >= 0xAD) && (keycode <= 0xAF)) {
		return DIVISION_SHIFT_AND_CTRL;
	} else if ((keycode >= 0xB0) && (keycode <= 0xB2)) {
		return DIVISION_LOCK_AND_COMPOSE;
	} else if (keycode == 0xBC) {
		return DIVISION_DELETE;
	} else if ((keycode >= 0xBD) && (keycode <= 0xBE)) {
		return DIVISION_RETURN_AND_TAB;
	} else if ((keycode >= 0xBF) && (keycode <= 0xFF)) {
		return DIVISION_MAIN_ARRAY;
	} else {
		return -1;
	}
}

static uint8_t last_report[HID_REPORT_SIZE] = { 0x00 };

static bool is_in_report(int keycode, const uint8_t *report)
{
	for (int i = HID_REPORT_FIRST_KEY; i < HID_REPORT_SIZE; i++) {
		if (keycode == report[i]) {
			return true;
		}
	}

	return false;
}

K_MUTEX_DEFINE(uart_tx_mutex);

static void uart_tx_code(unsigned char code)
{
	k_mutex_lock(&uart_tx_mutex, K_FOREVER);
	uart_poll_out(uart_dev, code);
	k_mutex_unlock(&uart_tx_mutex);
}

static void lk201_key_down(int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	/* TODO */
	uart_tx_code(keycode);
}

static void lk201_key_up(int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	/* TODO */
	uart_tx_code(SPECIAL_ALL_UPS);
}

void hid_report_cb(const uint8_t *this_report)
{
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

	const uint8_t this_modifiers = this_report[0];
	const uint8_t last_modifiers = last_report[0];

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

	memcpy(last_report, this_report, sizeof(last_report));
}

static void
send_power_on_test_result(void) {
	k_mutex_lock(&uart_tx_mutex, K_FOREVER);
	uart_poll_out(uart_dev, SPECIAL_KEYBOARD_ID_FIRMWARE);
	uart_poll_out(uart_dev, SPECIAL_KEYBOARD_ID_HARDWARE);
	uart_poll_out(uart_dev, 0x00); /* ERROR */
	uart_poll_out(uart_dev, 0x00); /* KEYCODE */
	k_mutex_unlock(&uart_tx_mutex);
}

struct message {
	uint8_t size;
	uint8_t buf[4];
};

K_MSGQ_DEFINE(uart_msgq, sizeof(struct message), 10, 4);

static void
serial_cb(const struct device *dev, void *user_data)
{
	struct message message = { 0 , { 0 } };

	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if (c & 0x80) {
			/* no more parameters */
			message.buf[message.size++] = c;
			k_msgq_put(&uart_msgq, &message, K_NO_WAIT);
			message.size = 0;
		} else if (message.size < (sizeof(message.buf) - 1)) {
			message.buf[message.size++] = c;
		}
	}
}

static int keyclick_volume = -1;
static int bell_volume = -1;

K_MUTEX_DEFINE(beeper_mutex);

static void
beeper_on(int volume)
{
	const uint32_t pulse = (pwm_beeper0.period / 2U) * (8 - volume) / 8;
	k_mutex_lock(&beeper_mutex, K_FOREVER);
	int ret = pwm_set_dt(&pwm_beeper0, pwm_beeper0.period, pulse);
	k_mutex_unlock(&beeper_mutex);
	if (ret) {
		LOG_ERR("Error %d: failed to set pulse width", ret);
	}
}

static void
beeper_off(void)
{
	k_mutex_lock(&beeper_mutex, K_FOREVER);
	int ret = pwm_set_dt(&pwm_beeper0, pwm_beeper0.period, 0);
	k_mutex_unlock(&beeper_mutex);
	if (ret) {
		LOG_ERR("Error %d: failed to set pulse width", ret);
	}
}

void beeper_off_work_handler(struct k_work *work)
{
	beeper_off();
}

K_WORK_DEFINE(beeper_off_work, beeper_off_work_handler);

void beeper_off_timer_handler(struct k_timer *dummy)
{
    k_work_submit(&beeper_off_work);
}

K_TIMER_DEFINE(beeper_off_timer, beeper_off_timer_handler, NULL);

static void
sound_keyclick(void)
{
	if (keyclick_volume < 0) {
		return;
	}

	beeper_on(keyclick_volume);

	k_timer_start(&beeper_off_timer, K_MSEC(2), K_FOREVER);
}

static void
sound_bell(void)
{
	if (bell_volume < 0) {
		return;
	}

	beeper_on(bell_volume);

	k_timer_start(&beeper_off_timer, K_MSEC(125), K_FOREVER);
}

static void
init_defaults(void)
{
	memcpy(repeat_buffers, repeat_buffers_default, sizeof(repeat_buffers));
	memcpy(divisions, divisions_default, sizeof(divisions));
	keyclick_volume = -1;
	bell_volume = -1;
}

static int
handle_light_leds(struct message *message)
{
	if (message->size != 2) {
		return SPECIAL_INPUT_ERROR;
	}

	for (int i = 0; i < 4; i++) {
		if (message->buf[1] & (1 << i)) {
			gpio_pin_set_dt(&leds[i], 1);
		}
	}

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_turn_off_leds(struct message *message)
{
	if (message->size != 2) {
		return SPECIAL_INPUT_ERROR;
	}

	for (int i = 0; i < 4; i++) {
		if (message->buf[1] & (1 << i)) {
			gpio_pin_set_dt(&leds[i], 0);
		}
	}

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_disable_keyclick(struct message *message)
{
	keyclick_volume = -1;

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_enable_keyclick_set_volume(struct message *message)
{
	if (message->size != 2) {
		return SPECIAL_INPUT_ERROR;
	}

	keyclick_volume = message->buf[1] & 0x07;

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_sound_keyclick(struct message *message)
{
	sound_keyclick();

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_disable_bell(struct message *message)
{
	bell_volume = -1;

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_enable_bell_set_volume(struct message *message)
{
	if (message->size != 2) {
		return SPECIAL_INPUT_ERROR;
	}

	bell_volume = message->buf[1] & 0x07;

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_sound_bell(struct message *message)
{
	sound_bell();

	return SPECIAL_MODE_CHANGE_ACK;
}

static int
handle_jump_to_power_up(struct message *message)
{
	send_power_on_test_result();

	return 0;
}

static int
handle_reinstate_defaults(struct message *message)
{
	init_defaults();

	return 0;
}

static void
handle_message(struct message *message)
{
	if (message->size == 0) {
		return;
	}

	int ret = 0;

	switch (message->buf[0]) {
		/* INDICATORS */
		case COMMAND_LIGHT_LEDS:
			ret = handle_light_leds(message);
			break;
		case COMMAND_TURN_OFF_LEDS:
			ret = handle_turn_off_leds(message);
			break;
		/* AUDIO */
		case COMMAND_DISABLE_KEYCLICK:
			ret = handle_disable_keyclick(message);
			break;
		case COMMAND_ENABLE_KEYCLICK_SET_VOLUME:
			ret = handle_enable_keyclick_set_volume(message);
			break;
		case COMMAND_SOUND_KEYCLICK:
			ret = handle_sound_keyclick(message);
			break;
		case COMMAND_DISABLE_BELL:
			ret = handle_disable_bell(message);
			break;
		case COMMAND_ENABLE_BELL_SET_VOLUME:
			ret = handle_enable_bell_set_volume(message);
			break;
		case COMMAND_SOUND_BELL:
			ret = handle_sound_bell(message);
			break;
		/* OTHER */
		case COMMAND_JUMP_TO_POWER_UP:
			ret = handle_jump_to_power_up(message);
			break;
		case COMMAND_REINSTATE_DEFAULTS:
			ret = handle_reinstate_defaults(message);
			break;
	}

	if (ret > 0) {
		uart_tx_code(ret);
	}
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

	init_defaults();

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not found");
		return -1;
	}

	if (!gpio_is_ready_dt(&uart_tx_enable)) {
		LOG_ERR("UART TX enable pin GPIO port is not ready.");
		return -1;
	}

	ret = gpio_pin_configure_dt(&uart_tx_enable, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Configuring GPIO pin failed: %d", ret);
		return -1;
	}

	for (int i = 0; i < NUM_LEDS; i++) {
		if (!gpio_is_ready_dt(&leds[i])) {
			LOG_ERR("led%d pin GPIO port is not ready.", i);
			return -1;
		}

		ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		if (ret != 0) {
			LOG_ERR("Configuring GPIO pin failed: %d", ret);
			return -1;
		}
	}

        if (!pwm_is_ready_dt(&pwm_beeper0)) {
                printk("Error: PWM device %s is not ready\n",
                       pwm_beeper0.dev->name);
                return 0;
        }

	for (int i = 0; i < NUM_KEYS; i++) {
		keys[i] = -1;
	}

	for (int i = 0; i < NUM_REPEAT_BUFFERS; i++) {
		repeat_buffers[i].timeout = 300;
		repeat_buffers[i].interval = 30;
	}

	send_power_on_test_result();

	ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			LOG_ERR("Interrupt-driven UART API support not enabled");
		} else if (ret == -ENOSYS) {
			LOG_ERR("UART device does not support interrupt-driven API");
		} else {
			LOG_ERR("Error setting UART callback: %d", ret);
		}
		return -1;
	}
	uart_irq_rx_enable(uart_dev);

	ret = bluetooth_listen(hid_report_cb);
	if (ret < 0) {
		LOG_ERR("Bluetooth listening failed");
		return -1;
	}

	handle_messages();
}
