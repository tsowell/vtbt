#ifndef LK201_UART_H
#define LK201_UART_H

#include <stdint.h>
#include <stddef.h>

/* A UART interface supporting LK201-style flow control. */

void lk201_uart_lock(void);
void lk201_uart_unlock(void);
int lk201_uart_write_byte(uint8_t out_char);
int lk201_uart_write(const uint8_t *buf, size_t count);

#endif
