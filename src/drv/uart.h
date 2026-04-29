// uart.h
#ifndef UART_H
#define UART_H

#include <zephyr/types.h>

typedef void (*uart_data_callback_t)(const uint8_t *data, uint16_t len);

int uart_init(void);
void uart_send_to_ble(uart_data_callback_t callback);
int uart_send_data(const uint8_t *data, uint16_t len);

#endif /* UART_H */
