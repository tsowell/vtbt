#include <stdbool.h>

#include "lk201_uart.h"

#include "lk201.h"
#include "uart.h"

static bool locked = false;
static uint8_t locked_buf[5];
static int locked_count;
static bool locked_missed;

void
lk201_uart_lock(void)
{
	if (!locked) {
		locked_count = 0;
		locked_missed = false;
		locked = true;
	}
}

void
lk201_uart_unlock(void)
{
	if (locked) {
		if (locked_missed) {
			locked_buf[locked_count] = SPECIAL_OUTPUT_ERROR;
			uart_write(locked_buf, locked_count + 1);
		} else {
			uart_write(locked_buf, locked_count);
		}
		locked = false;
	}
}

int
lk201_uart_write_byte(uint8_t out_char)
{
	if (!locked) {
		uart_write_byte(out_char);
		return 1;
	} else {
		if (locked_count < 4) {
			locked_buf[locked_count++] = out_char;
			return 1;
		} else {
			locked_missed = true;
			return 0;
		}
	}
}

int
lk201_uart_write(const uint8_t *buf, size_t count)
{
	if (!locked) {
		uart_write(buf, count);
		return count;
	} else {
		size_t written = 0;
		while (locked_count < 4 && written < count) {
			locked_buf[locked_count++] = buf[written++];
		}
		if (written < count) {
			locked_missed = true;
		}
		return written;
	}
}
