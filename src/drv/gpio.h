// gpio.h
#ifndef GPIO_H
#define GPIO_H

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// GPIO 端口定义
#define GPIO_PORT_0     0
#define GPIO_PORT_1     1

// LED 定义（根据你的硬件修改引脚号）
// 示例：GPIO0 上的引脚
#define LED_VDD_PORT    0      // GPIO0.29 - VDD 使能引脚
#define LED_VDD_PIN     29      // GPIO0.29 - VDD 使能引脚

#define AVDD_EN_PORT    0      // GPIO0.28 - AVDD 使能引脚
#define AVDD_EN_PIN     28      // GPIO0.28 - AVDD 使能引脚


/**
 * @brief GPIO 引脚配置结构体
 */
struct gpio_pin_config {
    uint8_t port;           // GPIO_PORT_0 或 GPIO_PORT_1
    uint32_t pin;           // 引脚号 (0-31)
    bool is_output;         // 是否为输出
    bool default_state;     // 默认状态 (仅输出有效)
    gpio_flags_t flags;     // 额外标志（如上拉/下拉）
};

/**
 * @brief 初始化 GPIO
 * @return 0 成功，负数失败
 */
int gpio_init(void);

/**
 * @brief 获取指定端口的设备实例
 * @param port GPIO_PORT_0 或 GPIO_PORT_1
 * @return GPIO 设备指针，失败返回 NULL
 */
const struct device *gpio_get_device(uint8_t port);

/**
 * @brief 配置单个 GPIO 引脚
 * @param port GPIO 端口
 * @param pin 引脚号
 * @param flags 配置标志 (GPIO_OUTPUT, GPIO_INPUT, GPIO_PULL_UP 等)
 * @return 0 成功，负数失败
 */
int gpio_pin_config(uint8_t port, uint32_t pin, gpio_flags_t flags);

/**
 * @brief 使能/关闭 LED VDD 电源
 * @param enable true: 使能, false: 关闭
 */
void gpio_led_vdd_enable(bool enable);

/**
 * @brief 设置 gpio 状态
 * @param port GPIO 端口
 * @param pin 引脚号
 * @param state LED_ON 或 LED_OFF
 */
void gpio_set(uint8_t port, uint32_t pin, uint8_t state);

/**
 * @brief 翻转 gpio 状态
 * @param port GPIO 端口
 * @param pin 引脚号
 */
void gpio_toggle(uint8_t port, uint32_t pin);


/**
 * @brief 获取引脚当前状态
 * @param port GPIO 端口
 * @param pin 引脚号
 * @return 引脚的电平状态（0或1），负数表示错误
 */
int gpio_get_pin_state(uint8_t port, uint32_t pin);

#endif /* GPIO_H */
