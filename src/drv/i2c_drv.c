#include "i2c_drv.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_drv, LOG_LEVEL_INF);

/* ========== devicetree node labels ========== */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#define I2C0_NODE DT_NODELABEL(i2c0)
#else
#error "i2c0 node not found in devicetree"
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
#define I2C1_NODE DT_NODELABEL(i2c1)
#define I2C1_ENABLED 1
#else
#define I2C1_ENABLED 0
#endif

#define STACK_TX_BUF_MAX 32

/* ========== device pointer cache ========== */
static const struct device *i2c0_dev;
static const struct device *i2c1_dev;

/* ========== internal helpers ========== */

static inline const struct device *get_dev(uint8_t bus_id)
{
    switch (bus_id) {
    case 0:
        return i2c0_dev;
    case 1:
        return i2c1_dev;
    default:
        return NULL;
    }
}

/* ========== public functions ========== */

int i2c_init(uint8_t bus_id)
{
    //CONFIG_I2C_NRFX_TRANSFER_TIMEOUT i2c_transfer每条msg默认500ms超时
    const struct device *dev;

    switch (bus_id) {
    case 0:
        dev = DEVICE_DT_GET(I2C0_NODE);
        if (!device_is_ready(dev)) {
            LOG_ERR("i2c0 bus not ready");
            return I2C_ERR_IO;
        }
        i2c0_dev = dev;
        /* 直接从 devicetree 属性读取频率 */
        uint32_t freq = DT_PROP(I2C0_NODE, clock_frequency);
        LOG_INF("i2c0 ready (freq=%u Hz)", freq);
        break;
#if I2C1_ENABLED
    case 1:
        dev = DEVICE_DT_GET(I2C1_NODE);
        if (!device_is_ready(dev)) {
            LOG_ERR("i2c1 bus not ready");
            return I2C_ERR_IO;
        }
        i2c1_dev = dev;
        /* 直接从 devicetree 属性读取频率 */
        uint32_t freq = DT_PROP(I2C1_NODE, clock_frequency);
        LOG_INF("i2c1 ready (freq=%u Hz)", freq);
        break;
#endif
    default:
        LOG_ERR("invalid bus_id: %d", bus_id);
        return I2C_ERR_PARAM;
    }
    return I2C_OK;
}

int i2c_read_reg(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr,
                 uint8_t *data, uint16_t len)
{
    const struct device *dev = get_dev(bus_id);
    int ret;

    if (dev == NULL) {
        LOG_ERR("invalid bus_id: %d", bus_id);
        return I2C_ERR_PARAM;
    }

    if (data == NULL || len == 0) {
        LOG_ERR("invalid params: data=%p, len=%d", (void *)data, len);
        return I2C_ERR_PARAM;
    }

    /*
     * i2c_write_read() 执行复合传输:
     *   START -> dev_addr(W) -> reg_addr -> REPEATED_START -> dev_addr(R) -> data -> STOP
     */
    ret = i2c_write_read(dev, dev_addr,
                         &reg_addr, 1,
                         data, len);
    if (ret != 0) {
        LOG_ERR("i2c%d read_reg failed: dev=0x%02x reg=0x%02x err=%d",
                bus_id, dev_addr, reg_addr, ret);
        return I2C_ERR_IO;
    }

    return I2C_OK;
}

int i2c_write_reg(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr,
                  const uint8_t *data, uint16_t len)
{
    const struct device *dev = get_dev(bus_id);
    int ret;
    uint8_t *tx_buf;
    uint16_t total_len;
    bool use_stack;

    if (dev == NULL) {
        LOG_ERR("invalid bus_id: %d", bus_id);
        return I2C_ERR_PARAM;
    }

    total_len = 1 + len; /* reg_addr + data */

    /* allocate tx buffer */
    if (total_len <= STACK_TX_BUF_MAX) {
        uint8_t stack_buf[STACK_TX_BUF_MAX];
        tx_buf = stack_buf;
        use_stack = true;
    } else {
        tx_buf = k_malloc(total_len);
        if (tx_buf == NULL) {
            LOG_ERR("alloc failed for %d bytes", total_len);
            return I2C_ERR_ALLOC;
        }
        use_stack = false;
    }

    /* build tx buffer: reg_addr + data */
    tx_buf[0] = reg_addr;
    if (len > 0 && data != NULL) {
        memcpy(tx_buf + 1, data, len);
    }

    ret = i2c_write(dev, dev_addr, tx_buf, total_len);

    if (!use_stack) {
        k_free(tx_buf);
    }

    if (ret != 0) {
        LOG_ERR("i2c%d write_reg failed: dev=0x%02x reg=0x%02x err=%d",
                bus_id, dev_addr, reg_addr, ret);
        return I2C_ERR_IO;
    }

    return I2C_OK;
}

int i2c_tx(uint8_t bus_id, uint8_t dev_addr,
           const uint8_t *data, uint16_t len)
{
    const struct device *dev = get_dev(bus_id);
    int ret;

    if (dev == NULL) {
        LOG_ERR("invalid bus_id: %d", bus_id);
        return I2C_ERR_PARAM;
    }

    if (data == NULL && len > 0) {
        LOG_ERR("invalid params: data=%p, len=%d", (void *)data, len);
        return I2C_ERR_PARAM;
    }

    struct i2c_msg msg;
    msg.buf = (uint8_t *)data;
    msg.len = len;
    msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

    ret = i2c_transfer(dev, &msg, 1, dev_addr);
    if (ret != 0) {
        LOG_ERR("i2c%d tx failed: dev=0x%02x err=%d", bus_id, dev_addr, ret);
        return I2C_ERR_IO;
    }

    return I2C_OK;
}
int i2c_tx_no_stop(uint8_t bus_id, uint8_t dev_addr,
                   const uint8_t *data, uint16_t len)
{
    const struct device *dev = get_dev(bus_id);
    struct i2c_msg msg;
    int ret;

    if (dev == NULL) {
        LOG_ERR("invalid bus_id: %d", bus_id);
        return I2C_ERR_PARAM;
    }

    if (data == NULL && len > 0) {
        LOG_ERR("invalid params: data=%p, len=%d", (void *)data, len);
        return I2C_ERR_PARAM;
    }

    msg.buf = (uint8_t *)data;
    msg.len = len;
    msg.flags = I2C_MSG_WRITE;  /* no I2C_MSG_STOP flag */

    ret = i2c_transfer(dev, &msg, 1, dev_addr);
    if (ret != 0) {
        LOG_ERR("i2c%d tx_no_stop failed: dev=0x%02x err=%d", bus_id, dev_addr, ret);
        return I2C_ERR_IO;
    }

    return I2C_OK;
}

int i2c_rx(uint8_t bus_id, uint8_t dev_addr,
           uint8_t *data, uint16_t len)
{
    const struct device *dev = get_dev(bus_id);
    struct i2c_msg msg;
    int ret;

    if (dev == NULL) {
        LOG_ERR("invalid bus_id: %d", bus_id);
        return I2C_ERR_PARAM;
    }

    if (data == NULL || len == 0) {
        LOG_ERR("invalid params: data=%p, len=%d", (void *)data, len);
        return I2C_ERR_PARAM;
    }

    msg.buf = data;
    msg.len = len;
    msg.flags = I2C_MSG_READ | I2C_MSG_STOP;

    ret = i2c_transfer(dev, &msg, 1, dev_addr);
    if (ret != 0) {
        LOG_ERR("i2c%d rx failed: dev=0x%02x err=%d", bus_id, dev_addr, ret);
        return I2C_ERR_IO;
    }

    return I2C_OK;
}
