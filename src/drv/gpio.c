// gpio.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "gpio.h"

LOG_MODULE_REGISTER(gpio_module, LOG_LEVEL_INF);

// GPIO 设备实例
static const struct device *gpio_dev[2] = {
    DEVICE_DT_GET(DT_NODELABEL(gpio0)),  // GPIO0
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio1), okay)
    DEVICE_DT_GET(DT_NODELABEL(gpio1)),  // GPIO0
#else
    NULL,
#endif
};

// 引脚所属端口映射表（用于简化函数）
// 自动识别预定义引脚属于哪个端口
struct pin_port_map {
    uint32_t pin;
    uint8_t port;
};


// 初始化配置的引脚列表
static const struct gpio_pin_config init_pins[] = {
    //port pin 是否为输出 初始电平 上其他标志(上拉/下拉) 见 <zephyr/drivers/gpio.h>: GPIO_OUTPUT_LOW
    // GPIO0 上的引脚  
    {LED_VDD_PORT, LED_VDD_PIN,     true,  false, 0},                    // LED VDD 使能，初始关闭
    {AVDD_EN_PORT, AVDD_EN_PIN,     true,  false, 0},                    // AVDD 使能，初始关闭
    
    // GPIO1 上的引脚
    // {GPIO_PORT_1, LED_BLE_PIN,     true,  false, 0},                    // BLE 连接灯
};

/**
 * @brief 初始化 GPIO
 */
int gpio_init(void)
{
    int ret = 0;
    
    // 检查 GPIO 设备是否就绪
    for (int i = 0; i < 2; i++) {
        if (!device_is_ready(gpio_dev[i])) {
            LOG_ERR("GPIO%d device not ready", i);
            return -ENODEV;
        }
        LOG_INF("GPIO%d device initialized", i);
    }
    
    // 配置所有引脚
    for (int i = 0; i < ARRAY_SIZE(init_pins); i++) {
        const struct gpio_pin_config *cfg = &init_pins[i];
        
        if (cfg->is_output) {
            // 配置为输出模式
            ret = gpio_pin_configure(gpio_dev[cfg->port], cfg->pin, 
                                      GPIO_OUTPUT | cfg->flags);
            if (ret < 0) {
                LOG_ERR("Failed to configure GPIO%d.%d as output", 
                        cfg->port, cfg->pin);
                return ret;
            }
            
            // 设置默认状态
            ret = gpio_pin_set(gpio_dev[cfg->port], cfg->pin, 
                               cfg->default_state);
            if (ret < 0) {
                LOG_ERR("Failed to set GPIO%d.%d to default state", 
                        cfg->port, cfg->pin);
                return ret;
            }
            
            LOG_DBG("GPIO%d.%d configured as output, default: %d", 
                    cfg->port, cfg->pin, cfg->default_state);
        } else {
            // 配置为输入模式
            ret = gpio_pin_configure(gpio_dev[cfg->port], cfg->pin, 
                                      GPIO_INPUT | cfg->flags);
            if (ret < 0) {
                LOG_ERR("Failed to configure GPIO%d.%d as input", 
                        cfg->port, cfg->pin);
                return ret;
            }
            LOG_DBG("GPIO%d.%d configured as input", cfg->port, cfg->pin);
        }
    }
    
    LOG_INF("GPIO initialization complete");
    return 0;
}

/**
 * @brief 获取指定端口的设备实例
 */
const struct device *gpio_get_device(uint8_t port)
{
    if (port >= 2) {
        LOG_ERR("Invalid GPIO port: %d", port);
        return NULL;
    }
    return gpio_dev[port];
}

/**
 * @brief 配置单个 GPIO 引脚
 */
int gpio_pin_config(uint8_t port, uint32_t pin, gpio_flags_t flags)
{
    if (port >= 2) {
        LOG_ERR("Invalid GPIO port: %d", port);
        return -EINVAL;
    }
    
    if (!device_is_ready(gpio_dev[port])) {
        LOG_ERR("GPIO%d not ready", port);
        return -ENODEV;
    }
    
    return gpio_pin_configure(gpio_dev[port], pin, flags);
}

/**
 * @brief 使能/关闭 LED VDD 电源
 */
void gpio_led_vdd_enable(bool enable)
{
    uint8_t value = enable ? 1 : 0;
    int ret = gpio_pin_set(gpio_dev[GPIO_PORT_0], LED_VDD_PIN, value);
    
    if (ret < 0) {
        LOG_ERR("Failed to %s VDD (GPIO%d.%d)", 
                enable ? "enable" : "disable", GPIO_PORT_0, LED_VDD_PIN);
    } else {
        LOG_INF("VDD %s", enable ? "enabled" : "disabled");
    }
}

/**
 * @brief 设置 gpio 状态
 */
void gpio_set(uint8_t port, uint32_t pin, uint8_t state)
{
    if (port >= 2) {
        LOG_ERR("Invalid GPIO port: %d", port);
        return;
    }
    
    int ret = gpio_pin_set(gpio_dev[port], pin, state);
    
    if (ret < 0) {
        LOG_ERR("Failed to set GPIO%d.%d to %d", port, pin, state);
    }
}

/**
 * @brief 翻转 gpio 状态
 */
void gpio_toggle(uint8_t port, uint32_t pin)
{
    if (port >= 2) {
        LOG_ERR("Invalid GPIO port: %d", port);
        return;
    }
    
    int ret = gpio_pin_toggle(gpio_dev[port], pin);
    
    if (ret < 0) {
        LOG_ERR("Failed to toggle GPIO%d.%d", port, pin);
    }
}

/**
 * @brief 获取引脚当前状态
 */
int gpio_get_pin_state(uint8_t port, uint32_t pin)
{
    if (port >= 2) {
        LOG_ERR("Invalid GPIO port: %d", port);
        return -EINVAL;
    }
    
    return gpio_pin_get(gpio_dev[port], pin);
}