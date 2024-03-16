#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "uart.h"

LOG_MODULE_REGISTER(uart, CONFIG_LOG_DEFAULT_LEVEL);

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_vt_uart)

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

static const struct gpio_dt_spec uart_tx_enable =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(uart_tx_enable), gpios, {0});

K_MUTEX_DEFINE(uart_tx_mutex);

static serial_cb user_callback = NULL;

static void
callback(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if (user_callback) {
			user_callback(c);
		}
	}
}

int uart_init(void)
{
	int ret;

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

	return 0;
}

int uart_set_rx_callback(serial_cb serial_cb)
{
	int ret;

	user_callback = serial_cb;

	ret = uart_irq_callback_user_data_set(uart_dev, callback, NULL);
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
	return 0;
}

void uart_write_byte(unsigned char out_char)
{
	k_mutex_lock(&uart_tx_mutex, K_FOREVER);
	uart_poll_out(uart_dev, out_char);
	k_mutex_unlock(&uart_tx_mutex);
}

void uart_write(const unsigned char buf[], size_t count)
{
	k_mutex_lock(&uart_tx_mutex, K_FOREVER);
	for (size_t i = 0; i < count; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
	k_mutex_unlock(&uart_tx_mutex);
}
