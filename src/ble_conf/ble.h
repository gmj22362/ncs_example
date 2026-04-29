// ble.h
#ifndef BLE_H
#define BLE_H

#include <zephyr/types.h>

int ble_init(void);
void ble_advertising_start(void);
int ble_send_data(const uint8_t *data, uint16_t len);
uint8_t msg_send_func(uint8_t* buf, uint16_t length, uint8_t ctrl);
void ble_ad_sd_update(uint8_t electri, uint8_t device_status, uint16_t activity, uint8_t hr, float tempratrue);

#endif /* BLE_H */
