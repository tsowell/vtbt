#ifndef UART_H
#define UART_H

/* This implements an LK201-style UART with a 4-byte TX buffer and flow control
 * via locking. */

typedef void (*serial_cb)(uint8_t c);

int uart_init(void);
int uart_set_rx_callback(serial_cb serial_cb);

/* These return the number of bytes written. When unlocked, the functions block
 * until all bytes have been written, but when locked, they return once the TX
 * buffer is full. */
int uart_write_byte(unsigned char out_char);
int uart_write(const unsigned char buf[], size_t count);

/* Lock the UART LK201-style. Let the TX buffer fill up. */
void uart_lock(void);
/* Unlock the UART and flush the TX buffer, blocking until it is empty. */
void uart_unlock(void);
/* Returns true if an overflow occurred since the keyboard was last locked. */
bool uart_overflow_get(void);
/* Flush the TX buffer and block until it is empty. */
void uart_flush(void);

#endif /* UART_H */
