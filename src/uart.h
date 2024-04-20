#ifndef UART_H
#define UART_H

typedef void (*serial_cb)(uint8_t c);

int uart_init(void);
int uart_set_rx_callback(serial_cb serial_cb);
int uart_write_byte(unsigned char out_char);
int uart_write(const unsigned char buf[], size_t count);
void uart_lock(void);
void uart_unlock(void);
bool uart_get_overflow(void);
void uart_flush(void);

#endif /* UART_H */
