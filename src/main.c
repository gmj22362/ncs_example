// main.c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/drivers/i2c.h>

#include "uart.h"
#include "gpio.h"
#include "ble.h"
#include "main.h"

#include "i2c_drv.h"

#include "m117_mts01_i2c.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define RUN_STATUS_LED DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

dev_id_t dev_id;
device_mode_t device_mode;

/* 温度采样完成回调 */
static void temp_sample_done(float temp_celsius, bool success, void *user_data)
{
	int tmp = temp_celsius*100;
    if (success) {
        LOG_INF("temperature: %d °C", tmp);
        tempratrue = temp_celsius;

        /* 可以在这里处理后续逻辑，如显示、上报等 */
    } else {
        LOG_ERR("temperature sample failed");
    }
}

static void configure_gpio(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
}

// MPU6050 测试
void mpu6050_test(void)
{
    uint8_t reg = 0x75;
    uint8_t who_am_i = 0;
    int ret;
    
    // 方式1：分步读写（推荐）
    ret = i2c_read_reg(0, 0x68, reg, &who_am_i, 1);
    if (ret != I2C_OK) {
        LOG_ERR("Write failed: %d", ret);
        return;
    }
    LOG_INF("[I2C_Read] MPU6050 WHO_AM_I = 0x%02X", who_am_i);

}

void error_handler(void)
{
	LOG_ERR("error_handler");
	dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);
	while (1) {
		k_sleep(K_MSEC(1000));
	}
}
int main(void)
{
	int ret = 0;
	int blink_status = 0;
	int electric = 0;
	uint8_t status, cfg;
	float temp;

	configure_gpio();
    gpio_init();
    gpio_set(LED_VDD_PORT, LED_VDD_PIN, 1);

	if (ble_init() != 0) {
		error_handler();
	}

	// 初始化 I2C
    i2c_init(0);
	k_sleep(K_MSEC(1000));
	
	//读取温度
	m117_single_sample_async(temp_sample_done, NULL);
    
	/******* 添加其他传感器代码 *******/


	/******* 添加其他传感器代码 *******/
	
    LOG_INF("Enter while(1)");
	k_sleep(K_MSEC(1000));

	for (;;) {
        electric = !electric;
        if(electric == 0){
		    dk_set_led(DK_LED2, (++blink_status) % 2);
		    dk_set_led(DK_LED1, (blink_status) % 2);
        }
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));

		// mpu6050_test();
		m117_single_sample_async(temp_sample_done, NULL);
	}
	
	return 0;
}

