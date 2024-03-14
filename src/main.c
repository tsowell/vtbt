#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "lk201.h"
#include "bluetooth.h"
#include "config.h"

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_vt_uart)

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

LOG_MODULE_REGISTER(lk201, CONFIG_LOG_DEFAULT_LEVEL);

static int keys[NUM_KEYS];

static struct repeat_buffer repeat_buffers[NUM_REPEAT_BUFFERS] = {
	{ .timeout = 500, .interval = 30 },
	{ .timeout = 300, .interval = 30 },
	{ .timeout = 500, .interval = 40 },
	{ .timeout = 300, .interval = 40 },
};

static struct division divisions[NUM_DIVISIONS] = {
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

static void lk201_key_down(int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	/* TODO */
}

static void lk201_key_up(int keycode)
{
	if (keycode == 0x00) {
		return;
	}

	/* TODO */
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
			key = 0xAF; /* Ctrl */
		} else if ((i == 1) || (i == 5)) {
			key = 0xAE; /* Shift*/
		} else {
			continue;
		}
		if ((this_modifiers & (1 << i)) &&
		    !(last_modifiers & (1 << i))) {
			lk201_key_down(hid_to_lk201(key));
		}
		if ((last_modifiers & (1 << i)) &&
		    !(this_modifiers & (1 << i))) {
			lk201_key_up(hid_to_lk201(key));
		}
	}

	memcpy(last_report, this_report, sizeof(last_report));
}

static void
send_power_on_test_result(void) {
	uart_poll_out(uart_dev, SPECIAL_KEYBOARD_ID_FIRMWARE);
	uart_poll_out(uart_dev, SPECIAL_KEYBOARD_ID_HARDWARE);
	uart_poll_out(uart_dev, 0x00); /* ERROR */
	uart_poll_out(uart_dev, 0x00); /* KEYCODE */
}

struct message {
	uint8_t size;
	uint8_t buf[4];
};

K_MSGQ_DEFINE(msgq, sizeof(struct message), 10, 4);

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
		if (!(c & 0x80)) {
			/* no more parameters */
			message.buf[message.size] = c;
			k_msgq_put(&msgq, &message, K_NO_WAIT);
			message.size = 0;
		} else if (message.size < (sizeof(message.buf) - 1)) {
			message.buf[message.size++] = c;
		}
	}
}

static void
handle_message(struct message *messsage)
{
}

static void
handle_messages(void)
{
	struct message message;

	while (k_msgq_get(&msgq, &message, K_FOREVER) == 0) {
		handle_message(&message);
	}
}

int
main(void)
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not found");
		return -1;
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
}
