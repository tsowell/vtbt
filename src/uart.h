#ifndef UART_H
#define UART_H

typedef void (*serial_cb)(uint8_t c);

int uart_init(void);
int uart_set_rx_callback(serial_cb serial_cb);
void uart_write_byte(unsigned char out_char);
void uart_write(const unsigned char buf[], size_t count);

#endif /* UART_H */
