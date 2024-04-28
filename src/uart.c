#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "uart.h"

LOG_MODULE_REGISTER(uart, CONFIG_LOG_DEFAULT_LEVEL);

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_vt_uart)

/* 4-byte TX buffer, just like on the LK201. */
RING_BUF_DECLARE(tx_buf, 4);

/* Given by TX callback when new space is available in the TX buffer. */
K_SEM_DEFINE(tx_space_sem, 0, 1);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* Enable the RS-423 driver. It is disabled by default in order to prevent
 * transmission of bootloader, etc. output from the devkit. */
static const struct gpio_dt_spec uart_tx_enable =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(uart_tx_enable), gpios, {0});

static serial_cb user_callback = NULL;

static atomic_t locked;
static bool overflow;

static void
callback_tx(void)
{
	uint32_t size;
	int filled_size;
	uint8_t *data;

	if (atomic_get(&locked)) {
		uart_irq_tx_disable(uart_dev);
		return;
	}

	while (!ring_buf_is_empty(&tx_buf)) {
		size = ring_buf_get_claim(&tx_buf, &data, 4);
		filled_size = uart_fifo_fill(uart_dev, data, size);
		if (filled_size < 0) {
			ring_buf_get_finish(&tx_buf, 0);
			return;
		}
		int ret = ring_buf_get_finish(&tx_buf, (uint32_t)filled_size);
		if (filled_size > 0) {
			k_sem_give(&tx_space_sem);
		}
		if (ret < 0) {
			return;
		}
	}

	uart_irq_tx_disable(uart_dev);
}

static void
callback_rx(void)
{
	uint8_t c;

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if (user_callback) {
			user_callback(c);
		}
	}
}


static void
callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (uart_irq_update(uart_dev) < 0) {
		return;
	}

	if (uart_irq_tx_ready(uart_dev) > 0) {
		callback_tx();
	}

	if (uart_irq_rx_ready(uart_dev) > 0) {
		callback_rx();
	}
}

int
uart_init(void)
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

int
uart_set_rx_callback(serial_cb serial_cb)
{
	int ret;

	user_callback = serial_cb;

	ret = uart_irq_callback_user_data_set(uart_dev, callback, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			LOG_ERR("Interrupt-driven UART not enabled");
		} else if (ret == -ENOSYS) {
			LOG_ERR("UART does not support interrupt-driven API");
		} else {
			LOG_ERR("Error setting UART callback: %d", ret);
		}
		return -1;
	}
	uart_irq_rx_enable(uart_dev);
	return 0;
}

int
uart_write_byte(unsigned char out_char)
{
	while (ring_buf_put(&tx_buf, &out_char, 1) < 1) {
		if (atomic_get(&locked)) {
			overflow = true;
			return 0;
		}
		k_sem_take(&tx_space_sem, K_FOREVER);
	}
	uart_irq_tx_enable(uart_dev);
	return 1;
}

int
uart_write(const unsigned char buf[], size_t count)
{
	uint32_t total = 0;
	while (true) {
		uint32_t wrote = ring_buf_put(
			&tx_buf, &buf[total], count - total);
		if (atomic_get(&locked)) {
			if (wrote < count) {
				overflow = true;
			}
			return (int)wrote;
		}
		if (wrote > 0) {
			uart_irq_tx_enable(uart_dev);
		}
		total += wrote;
		if (total >= count) {
			break;
		}
		k_sem_take(&tx_space_sem, K_FOREVER);
	}

	return count;
}

void
uart_flush(void)
{
	uart_irq_tx_enable(uart_dev);
	while (!ring_buf_is_empty(&tx_buf)) {
		k_sem_take(&tx_space_sem, K_FOREVER);
	}
}

void
uart_lock(void)
{
	if (atomic_get(&locked)) {
		return;
	}

	atomic_set(&locked, 1);
	overflow = false;
}

void
uart_unlock(void)
{
	if (!atomic_get(&locked)) {
		return;
	}

	atomic_set(&locked, 0);

	uart_flush();
}

bool
uart_overflow_get(void)
{
	return overflow;
}
