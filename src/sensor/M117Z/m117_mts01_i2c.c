#include "m117_mts01_i2c.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(m117, LOG_LEVEL_INF);

#define M117_I2C_ADDR           0x44
#define M117_CRC_POLYNOMIAL     0x131   /* x^8 + x^5 + x^4 + 1 */

bool is_temp_conn_err;
float tempratrue;

/* ========== internal helpers ========== */

/**
 * @brief write a 16-bit command followed by optional data
 *
 * i2c_write_reg(bus, dev_addr, reg_addr, data, len)
 *   reg_addr = cmd[7:0]
 *   for 16-bit commands: we need to send cmd[15:8] then cmd[7:0]
 *   so we build a 2-byte "register address" and use i2c_tx
 */
static int m117_write_cmd(uint16_t cmd, const uint8_t *data, uint8_t len)
{
    /* build tx buffer: cmd[15:8], cmd[7:0], data[0..len-1] */
    uint8_t buf[2 + 3]; /* max 2 cmd + 3 data */
    buf[0] = (uint8_t)(cmd >> 8);
    buf[1] = (uint8_t)(cmd & 0xFF);

    if (len > 0 && data != NULL) {
        memcpy(buf + 2, data, len);
    }

    return i2c_tx(M117_I2C_BUS_ID, M117_I2C_ADDR, buf, 2 + len);
}

/**
 * @brief write a 16-bit command then read response
 */
static int m117_read_cmd(uint16_t cmd, uint8_t *data, uint8_t len)
{
    int ret;

    /* step 1: write the 16-bit command (no stop) - 
     * since we don't have a native "write then read" combined API
     * for 16-bit commands, we use write + read separately.
     * The sensor expects: 
     *   START -> dev_addr(W) -> cmd[15:8] -> cmd[7:0] -> REPEATED START -> dev_addr(R) -> data -> STOP
     * 
     * i2c_read_reg() does: write 1-byte reg, read data.
     * For 16-bit commands, we need a custom sequence.
     */
    
    /* use raw i2c_write_read for the combined transaction */
    uint8_t cmd_buf[2];
    cmd_buf[0] = (uint8_t)(cmd >> 8);
    cmd_buf[1] = (uint8_t)(cmd & 0xFF);

    /* get dev from i2c_drv internals - we expose it through i2c_drv.h */
    /* for now, use the public API: i2c_write_reg doesn't support 2-byte reg.
     * We'll use i2c_tx to write cmd, then i2c_rx to read.
     * This works because the sensor supports this pattern.
     */
     /*
     * i2c_write_read() 执行复合传输:
     *   START -> dev_addr(W) -> cmd_buf -> REPEATED_START -> dev_addr(R) -> data -> STOP
     */
    struct device * dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    ret = i2c_write_read(dev, M117_I2C_ADDR,
                         &cmd_buf, 2,
                         data, len);

    // ret = i2c_tx_no_stop(M117_I2C_BUS_ID, M117_I2C_ADDR, cmd_buf, 2);
    // if (ret != I2C_OK) {
    //     return ret;
    // }

    // ret = i2c_rx(M117_I2C_BUS_ID, M117_I2C_ADDR, data, len);
    return ret;
}

/* ========== public functions ========== */

float m117_output_to_temp(int16_t out)
{
    return ((float)out / 256.0f) + 40.0f;
}

int16_t m117_temp_to_output(float temp)
{
    return (int16_t)((temp - 40.0f) * 256.0f);
}

bool m117_convert_temp(void)
{
    int ret;

    ret = m117_write_cmd(M117_CMD_CONVERT_TEMP, NULL, 0);
    if (ret != I2C_OK) {
        LOG_ERR("convert temp failed: %d", ret);
        return false;
    }

    return true;
}

bool m117_read_temp_waiting(uint16_t *i_temp)
{
    uint8_t data[3];
    int ret;

    /* 
     * wait for conversion to complete based on repeatability setting
     * (caller should have set the appropriate delay before calling this)
     */

    ret = m117_read_cmd(M117_CMD_READ_STATUSCONFIG, data, 3);
    if (ret != I2C_OK) {
        LOG_ERR("read temp waiting failed: %d", ret);
        return false;
    }

    *i_temp = ((uint16_t)data[0] << 8) | data[1];

    /* verify crc */
    if (data[2] != m117_crc8(data, 2)) {
        LOG_ERR("crc check failed");
        return false;
    }

    return true;
}

bool m117_read_temp_polling(uint16_t *i_temp)
{
    uint8_t data[3];
    int timeout = 0;
    int ret;

    /* minimum 1ms wait after convert */
    k_msleep(1);

    while (timeout < 50) {
        ret = i2c_rx(M117_I2C_BUS_ID, M117_I2C_ADDR, data, 3);
        if (ret == I2C_OK) {
            /* got ACK, conversion complete */
            break;
        }
        /* NACK means still converting */
        k_msleep(1);
        timeout++;
    }

    if (timeout >= 50) {
        LOG_ERR("read temp polling timeout");
        return false;
    }

    *i_temp = ((uint16_t)data[0] << 8) | data[1];

    if (data[2] != m117_crc8(data, 2)) {
        LOG_ERR("crc check failed");
        return false;
    }

    return true;
}

void m117_set_config(uint8_t mps, uint8_t repeatability)
{
    uint8_t scr_rd[3], scr_wr[3];
    int ret;

    /* read current status/config */
    ret = m117_read_cmd(M117_CMD_READ_STATUSCONFIG, scr_rd, 3);
    if (ret != I2C_OK) {
        LOG_ERR("read status/config failed: %d", ret);
        return;
    }

    /* modify config byte */
    scr_wr[0] = scr_rd[1] & ~CFG_REPEATBILITY_MASK;
    scr_wr[0] |= repeatability;
    scr_wr[0] = scr_wr[0] & ~CFG_MPS_MASK;
    scr_wr[0] |= mps;
    scr_wr[1] = 0xFF;
    scr_wr[2] = m117_crc8(scr_wr, 2);

    /* write config */
    ret = m117_write_cmd(M117_CMD_WRITE_CONFIG, scr_wr, 3);
    if (ret != I2C_OK) {
        LOG_ERR("write config failed: %d", ret);
    }
}

void m117_read_status_config(uint8_t *status, uint8_t *cfg)
{
    uint8_t scr_rd[3];
    int ret;

    ret = m117_read_cmd(M117_CMD_READ_STATUSCONFIG, scr_rd, 3);
    if (ret != I2C_OK) {
        LOG_ERR("read status/config failed: %d", ret);
        return;
    }

    *status = scr_rd[0];
    *cfg = scr_rd[1];
}

bool m117_clear_status(void)
{
    int ret;

    ret = m117_write_cmd(M117_CMD_CLEAR_STATUS, NULL, 0);
    if (ret != I2C_OK) {
        LOG_ERR("clear status failed: %d", ret);
        return false;
    }

    return true;
}

void m117_set_temp_alert(float tha_set, float tla_set)
{
    uint16_t hs, ls;
    uint8_t scr_wr[3];
    int ret;

    /* write high alert limit */
    hs = (uint16_t)(m117_temp_to_output(tha_set) >> 7) & 0x01FF;
    scr_wr[0] = (uint8_t)(hs >> 8);
    scr_wr[1] = (uint8_t)(hs & 0xFF);
    scr_wr[2] = m117_crc8(scr_wr, 2);

    ret = m117_write_cmd(M117_CMD_WRITE_ALERT_HI_SET, scr_wr, 3);
    if (ret != I2C_OK) {
        LOG_ERR("write alert hi failed: %d", ret);
        return;
    }

    /* write low alert limit */
    ls = (uint16_t)(m117_temp_to_output(tla_set) >> 7) & 0x01FF;
    scr_wr[0] = (uint8_t)(ls >> 8);
    scr_wr[1] = (uint8_t)(ls & 0xFF);
    scr_wr[2] = m117_crc8(scr_wr, 2);

    ret = m117_write_cmd(M117_CMD_WRITE_ALERT_LO_SET, scr_wr, 3);
    if (ret != I2C_OK) {
        LOG_ERR("write alert lo failed: %d", ret);
    }
}

void m117_get_temp_alert(float *tha_set, float *tla_set)
{
    uint8_t scr_rd[3];
    uint16_t hs, ls;
    int ret;

    /* read high alert limit */
    ret = m117_read_cmd(M117_CMD_READ_ALERT_HI_SET, scr_rd, 3);
    if (ret != I2C_OK) {
        LOG_ERR("read alert hi failed: %d", ret);
        return;
    }
    hs = ((uint16_t)scr_rd[0] << 8 | scr_rd[1]) << 7;
    *tha_set = m117_output_to_temp((int16_t)hs);

    /* read low alert limit */
    ret = m117_read_cmd(M117_CMD_READ_ALERT_LO_SET, scr_rd, 3);
    if (ret != I2C_OK) {
        LOG_ERR("read alert lo failed: %d", ret);
        return;
    }
    ls = ((uint16_t)scr_rd[0] << 8 | scr_rd[1]) << 7;
    *tla_set = m117_output_to_temp((int16_t)ls);
}

bool m117_save_to_e2prom(void)
{
    bool ret = true;
    int result;

    result = m117_write_cmd(M117_CMD_COPY_PAGE0, NULL, 0);
    if (result != I2C_OK) {
        LOG_ERR("save to e2prom failed: %d", result);
        ret = false;
    }

    /* wait for eeprom erase/program (max 50ms) */
    k_msleep(50);

    return ret;
}

uint8_t m117_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    uint8_t byte_ctr;
    uint8_t bit;

    for (byte_ctr = 0; byte_ctr < len; byte_ctr++) {
        crc ^= data[byte_ctr];
        for (bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ M117_CRC_POLYNOMIAL;
            } else {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}


/* ========== work handler ========== */
/* work 结构体 - 文件作用域静态变量 */
static struct k_work_delayable m117_sample_work;
/* 回调缓存 */
static m117_sample_cb_t m117_cached_cb;
static void *m117_cached_user_data;
static void m117_sample_work_handler(struct k_work *work)
{
    uint16_t raw_temp;
    float temp;

    if (m117_read_temp_polling(&raw_temp)) {
        temp = m117_output_to_temp((int16_t)raw_temp);
        LOG_DBG("temperature: %.2f °C", (double)temp);
        is_temp_conn_err = false;

        if (m117_cached_cb) {
            m117_cached_cb(temp, true, m117_cached_user_data);
        }
    } else {
        LOG_ERR("read temp failed");
        is_temp_conn_err = true;

        if (m117_cached_cb) {
            m117_cached_cb(0.0f, false, m117_cached_user_data);
        }
    }
}


/**
 * @brief 启动单次温度采样（异步模式，使用 k_work_delayable）
 *
 * 配置传感器为单次转换 + 高重复性，发送 Convert 命令后，
 * 通过 k_work_delayable 延迟等待转换完成，在 work 回调中读取结果。
 *
 * @param cb        结果回调函数
 * @param user_data 用户自定义数据（传递给回调）
 * @return I2C_OK 成功启动转换，其他为错误码
 */
int m117_single_sample_async(m117_sample_cb_t cb, void *user_data)
{
    int ret;

    /* cache callback and user_data */
    m117_cached_cb = cb;
    m117_cached_user_data = user_data;

    /* step 1: init work (first time only, idempotent) */
    k_work_init_delayable(&m117_sample_work, m117_sample_work_handler);

    /* step 2: set config - single shot, high repeatability */
    m117_set_config(CFG_MPS_SINGLE, CFG_REPEATBILITY_HIGH);

    /* step 3: start conversion */
    ret = m117_write_cmd(M117_CMD_CONVERT_TEMP, NULL, 0);
    if (ret != I2C_OK) {
        LOG_ERR("convert temp failed: %d", ret);
        is_temp_conn_err = true;
        return ret;
    }

    is_temp_conn_err = false;

    /* step 4: schedule delayed work
     * high repeatability = ~11ms conversion + 2ms margin = 13ms
     * use 15ms for safety
     */
    k_work_schedule(&m117_sample_work, K_MSEC(T_CONVERT_HIGH+2));

    return I2C_OK;
}

