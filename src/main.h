#ifndef _MAIN_H_
#define _MAIN_H_

#define PROJECT_NUM		29
#define FW_VERSION		3
#define HW_VERSION		3

/*! Enum to string converter*/
#ifndef enum_to_string
#define enum_to_string(a)  #a
#endif

/** @brief Device ID 长度 (字节) */
/** @brief Device ID length (bytes) */
#define DEV_ID_LENGTH          14
#define PRO_ID_LENGTH          3

typedef struct{
    char device_id[DEV_ID_LENGTH];
    char product_id[PRO_ID_LENGTH];
} __attribute__((packed)) dev_id_t;

typedef enum{
	MONITOR_MODE,		//监测模式
	LOW_POWER_MODE,		//低功耗模式
	TEST_MODE,			//测试模式
}device_mode_t;

extern dev_id_t dev_id;
extern device_mode_t device_mode;
#endif

