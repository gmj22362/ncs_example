#ifndef I2C_DRV_H
#define I2C_DRV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 返回码 */
#define I2C_OK              0
#define I2C_ERR_PARAM       0xFE
#define I2C_ERR_TIMEOUT     0xFF
#define I2C_ERR_ALLOC       0xFD
#define I2C_ERR_IO          0xFC

/**
 * @brief 初始化 I2C 总线
 *
 * @param bus_id 总线 ID (0 或 1)
 * @return I2C_OK 成功，其他为错误码
 */
int i2c_init(uint8_t bus_id);

/**
 * @brief I2C 读寄存器（标准模式：先写寄存器地址，再读数据）
 *
 * @param bus_id      总线 ID (0 或 1)
 * @param dev_addr    设备地址（7位）
 * @param reg_addr    寄存器地址
 * @param data        接收数据缓冲区
 * @param len         要读取的字节数
 * @return I2C_OK 成功，其他为错误码
 */
int i2c_read_reg(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr,
                 uint8_t *data, uint16_t len);

/**
 * @brief I2C 写寄存器（标准模式：寄存器地址 + 数据）
 *
 * @param bus_id      总线 ID (0 或 1)
 * @param dev_addr    设备地址（7位）
 * @param reg_addr    寄存器地址
 * @param data        要写入的数据
 * @param len         要写入的字节数
 * @return I2C_OK 成功，其他为错误码
 */
int i2c_write_reg(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr,
                  const uint8_t *data, uint16_t len);

/**
 * @brief I2C 纯发送（不发送寄存器地址，带 STOP）
 *
 * 此函数发送完最后一个字节后产生 STOP 条件。
 * 如需不产生 STOP（用于复合传输），使用 i2c_tx_no_stop()
 */
int i2c_tx(uint8_t bus_id, uint8_t dev_addr,
           const uint8_t *data, uint16_t len);

/**
 * @brief I2C 纯发送（不发送寄存器地址，不带 STOP）
 *
 * 发送完最后一个字节后不产生 STOP 条件，保持总线占用，
 * 以便后续发送 REPEATED START 和读取数据。
 */
int i2c_tx_no_stop(uint8_t bus_id, uint8_t dev_addr,
                   const uint8_t *data, uint16_t len);

/**
 * @brief I2C 纯接收（不发送寄存器地址）
 *
 * @param bus_id      总线 ID (0 或 1)
 * @param dev_addr    设备地址（7位）
 * @param data        接收数据缓冲区
 * @param len         接收字节数
 * @return I2C_OK 成功，其他为错误码
 */
int i2c_rx(uint8_t bus_id, uint8_t dev_addr,
           uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* I2C_DRV_H */
