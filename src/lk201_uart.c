#include <stdbool.h>

#include "lk201_uart.h"

#include "lk201.h"
#include "uart.h"

static bool locked = false;
static uint8_t locked_buf[4];
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
		for (int i = 0; i < locked_count; i++) {
			uart_write_byte(locked_buf[i]);
		}
		if (locked_missed) {
			uart_write_byte(SPECIAL_OUTPUT_ERROR);
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
	int ret = 0;

	for (size_t i = 0; i < count; i++) {
		ret += lk201_uart_write_byte(buf[i]);
	}

	return ret;
}
