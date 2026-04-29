#ifndef M117_MTS01_I2C_H
#define M117_MTS01_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c_drv.h"

/* i2c bus id (0 or 1) */
#define M117_I2C_BUS_ID         0

/* i2c commands (16-bit: cmd[15:8], cmd[7:0]) */
#define M117_CMD_CONVERT_TEMP           0xCC44
#define M117_CMD_BREAK                  0x3093
#define M117_CMD_READ_STATUSCONFIG      0xF32D
#define M117_CMD_CLEAR_STATUS           0x3041
#define M117_CMD_WRITE_CONFIG           0x5206
#define M117_CMD_READ_ALERT_HI_SET      0xE11F
#define M117_CMD_READ_ALERT_LO_SET      0xE102
#define M117_CMD_WRITE_ALERT_HI_SET     0x611D
#define M117_CMD_WRITE_ALERT_LO_SET     0x6100
#define M117_CMD_WRITE_ALERT_HI_UNSET   0x6116
#define M117_CMD_WRITE_ALERT_LO_UNSET   0x610B
#define M117_CMD_READ_ALERT_HI_UNSET    0xE114
#define M117_CMD_READ_ALERT_LO_UNSET    0xE109
#define M117_CMD_COPY_PAGE0             0xCC48
#define M117_CMD_RECALL_EE              0xCCB8
#define M117_CMD_RECALL_PAGE0_RES       0xCCB6
#define M117_CMD_SOFT_RST               0x30A2

/* config register bit definitions */
#define CFG_MPS_MASK            0x1C
#define CFG_REPEATBILITY_MASK   0x03

#define CFG_MPS_SINGLE          0x00
#define CFG_MPS_HALF            0x04
#define CFG_MPS_1               0x08
#define CFG_MPS_2               0x0C
#define CFG_MPS_4               0x10
#define CFG_MPS_10              0x14

#define CFG_REPEATBILITY_LOW    0x00
#define CFG_REPEATBILITY_MEDIUM 0x01
#define CFG_REPEATBILITY_HIGH   0x02

/* status register bit definitions */
#define STATUS_MEASURE_MASK     0x81
#define STATUS_WRITE_CRC_MASK   0x20
#define STATUS_CMD_MASK         0x10
#define STATUS_POR_MASK         0x08
#define STATUS_T_ALERT_MASK     0x04

/* conversion times (ms), extra 2ms margin */
#define T_CONVERT_LOW           6
#define T_CONVERT_MEDIUM        8
#define T_CONVERT_HIGH          13

/**
 * @brief 单次温度采样结果回调类型
 * @param temp_celsius 温度值（摄氏度），仅在 success 为 true 时有效
 * @param success      采样是否成功
 * @param user_data    用户自定义数据指针
 */
typedef void (*m117_sample_cb_t)(float temp_celsius, bool success, void *user_data);


extern float tempratrue;
extern bool is_temp_conn_err;

/* public functions */
float m117_output_to_temp(int16_t out);
int16_t m117_temp_to_output(float temp);
bool m117_convert_temp(void);
bool m117_read_temp_waiting(uint16_t *i_temp);
bool m117_read_temp_polling(uint16_t *i_temp);
void m117_set_config(uint8_t mps, uint8_t repeatability);
void m117_read_status_config(uint8_t *status, uint8_t *cfg);
bool m117_clear_status(void);
void m117_set_temp_alert(float tha_set, float tla_set);
void m117_get_temp_alert(float *tha_set, float *tla_set);
bool m117_save_to_e2prom(void);
uint8_t m117_crc8(const uint8_t *data, uint8_t len);

int m117_single_sample_async(m117_sample_cb_t cb, void *user_data);

#endif /* M117_MTS01_I2C_H */
