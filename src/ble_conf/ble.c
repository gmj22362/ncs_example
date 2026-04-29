// ble.c
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <math.h>

#include "custom_service.h"

#include "main.h"
#include "ble.h"
#include "uart.h"

LOG_MODULE_REGISTER(ble_module, LOG_LEVEL_INF);

/********************蓝牙指令集************************/
#define TEST_MD_SWITCH					0X20
#define ACC_SAMPLE_RATE_SET				0X21
#define PPG_SAMPLE_INTERVAL_SET			0X22
#define LED_WARN_SWITCH					0X23

#define CMD_DEVICE_ID_SET               0x37
#define CMD_PRODUCT_ID_SET              0x39
#define CMD_DEVICE_ID_GET               0x36
#define CMD_PRODUCT_ID_GET              0x38
#define CMD_DEVICE_ID_REPORT            0xB6
#define CMD_PRODUCT_ID_REPORT           0xB8
#define CMD_SAMPLE_DATA_REPORT          0x80
/********************蓝牙指令集************************/

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define STACKSIZE CONFIG_BT_CUSTOM_SRV_THREAD_STACK_SIZE
#define PRIORITY 7

#define CON_STATUS_LED DK_LED2

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;
static struct k_work adv_work;
static K_SEM_DEFINE(ble_init_ok, 0, 1);

uint16_t mtu = 0;
static struct bt_gatt_exchange_params exchange_params;
static struct k_work_delayable set_tx_power_work;

//蓝牙广播数据
typedef struct{
    int ad_data_len;
    struct bt_data* ad_data;
    int sr_data_len;
    struct bt_data* sr_data;
}ble_adv_data_t;

//广播数据格式
struct beacon_info {
	uint8_t head;
	uint8_t id[14];
	uint8_t hw_version[2];
	uint8_t fw_version;
	uint8_t electricity;
	uint8_t device_status;
	uint16_t activity;
	uint8_t rr;
	uint8_t spo2;
	uint8_t hr;
	uint8_t temp[2];
	uint8_t tail;
};

ble_adv_data_t ble_adv_data;
struct beacon_info m_beacon_info;
static struct bt_data default_ad_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static struct bt_data default_sr_data[] = {
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 0xAA,  
		// 14bytes id
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		// 2bytes 硬件版本号	1byte 固件版本号
		PROJECT_NUM, HW_VERSION, FW_VERSION,
		// 1byte 电量    	1byte 设备状态
		0x00, 0x00,
		// 2bytes 活动量	1byte 呼吸		1byte 血氧		1byte 心率		2bytes 温度	
		0x00,0x00,	0x00, 0x00, 0x00, 0x00,0x00,
		// 1byte 帧尾
		0x55
	),
};

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	LOG_INF("Received data from: %s", addr);
	LOG_HEXDUMP_INF(data, len, "===");
    
	/******* 蓝牙数据接收处理 **********/
	uint8_t tmp = 0;
	uint8_t buf_len = data[1]-3;
	const uint8_t *buf = data+3;
	if(len>=buf_len+5 && data[0]==0xAA && data[buf_len+4]==0x55){
		switch(data[2]){
			case TEST_MD_SWITCH:
				LOG_INF("=========%s", enum_to_string(TEST_MD_SWITCH));
				// if(data[3] == 0){
				// 	if(device_mode == TEST_MODE) {
				// 		device_mode = MONITOR_MODE;
				// 	}
				// }else{
				// 	if(device_mode != TEST_MODE) {
				// 		device_mode = TEST_MODE;
				// 	}
				// }
				msg_send_func(&tmp, 1, TEST_MD_SWITCH|0x80);
				break;
			case ACC_SAMPLE_RATE_SET:
				LOG_INF("=========%s", enum_to_string(ACC_SAMPLE_RATE_SET));
				//if(data[3]<ACC_SAMPLE_RATE_MAX) bma_set_sample_rate(data[3], &bma580_dev);
				msg_send_func(&tmp, 1, ACC_SAMPLE_RATE_SET|0x80);
				break;
			case PPG_SAMPLE_INTERVAL_SET:
				LOG_INF("=========%s", enum_to_string(PPG_SAMPLE_INTERVAL_SET));
				// if(data[3]<PPG_SAMPLE_INTERVAL_MAX)	gh3026_ctrl.sample_interval = data[3];
				msg_send_func(&tmp, 1, PPG_SAMPLE_INTERVAL_SET|0x80);
				break;
			case LED_WARN_SWITCH:
				//
				LOG_INF("=========%s", enum_to_string(LED_WARN_SWITCH));
				break;
			case CMD_DEVICE_ID_GET:               
				// read_dev_id(&dev_id);
				msg_send_func((uint8_t*)dev_id.device_id, DEV_ID_LENGTH, CMD_DEVICE_ID_REPORT);
			break;
			case CMD_PRODUCT_ID_GET:               
				// read_dev_id(&dev_id);
				msg_send_func((uint8_t*)dev_id.product_id, PRO_ID_LENGTH, CMD_PRODUCT_ID_REPORT);
			break;
			case CMD_DEVICE_ID_SET:
				if(buf_len != DEV_ID_LENGTH){
					break;
				}
				memcpy(dev_id.device_id, buf, DEV_ID_LENGTH);
				// smart_write_dev_id(&dev_id);	
			break;
			case CMD_PRODUCT_ID_SET:
				if(buf_len != PRO_ID_LENGTH){
					break;
				}
				memcpy(dev_id.product_id, buf, PRO_ID_LENGTH);
				// smart_write_dev_id(&dev_id);
			break;	
				
			default:
				break;
		}	
	}
	/******* 蓝牙数据接收处理 **********/
}

static struct bt_custom_srv_cb custom_srv_cb = {
	.received = bt_receive_cb,
};

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err,
                             struct bt_gatt_exchange_params *params)
{
    if (!err) {
        mtu = bt_gatt_get_mtu(conn);
        LOG_INF("MTU exchange successful! Negotiated MTU: %d", mtu);
    } else {
        LOG_WRN("MTU exchange failed (err %d), using default MTU", err);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", addr);

	 if (!err) {
        // 发起MTU协商请求
        exchange_params.func = mtu_exchange_cb;
        int mtu_err = bt_gatt_exchange_mtu(conn, &exchange_params);
        if (mtu_err) {
            LOG_ERR("Failed to initiate MTU exchange (err %d)", mtu_err);
        } else {
            LOG_INF("MTU exchange initiated");
        }
    }

	current_conn = bt_conn_ref(conn);
	dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Disconnected: %s, reason 0x%02x %s", addr, reason, bt_hci_err_to_str(reason));

	if (auth_conn) {
		bt_conn_unref(auth_conn);
		auth_conn = NULL;
	}

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
		dk_set_led_off(CON_STATUS_LED);
	}
}

static void recycled_cb(void)
{
	LOG_INF("Connection object available from previous conn. Disconnect is complete!");
	ble_advertising_start();
}

// 在ble.c中添加连接质量监控
static void connection_param_updated(struct bt_conn *conn, 
                                      uint16_t interval, 
                                      uint16_t latency, 
                                      uint16_t timeout)
{
    LOG_INF("Connection parameters updated: interval %d, latency %d, timeout %d",
            interval, latency, timeout);
    
    // 获取协商后的MTU
    mtu = bt_gatt_get_mtu(conn);
    LOG_INF("Negotiated MTU: %d", mtu);
}

#ifdef CONFIG_BT_CUSTOM_SRV_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_WRN("Security failed: %s level %u err %d %s", addr, level, err,
			bt_security_err_to_str(err));
	}
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];
	auth_conn = bt_conn_ref(conn);
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Passkey for %s: %06u", addr, passkey);
	LOG_INF("Press Button 1 to confirm, Button 2 to reject.");
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Pairing cancelled: %s", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Pairing failed conn: %s, reason %d %s", addr, reason,
		bt_security_err_to_str(reason));
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled_cb,
	.le_param_updated = connection_param_updated,  // 添加这个回调
#ifdef CONFIG_BT_CUSTOM_SRV_SECURITY_ENABLED
	.security_changed = security_changed,
#endif
};

static void ble_adv_data_prov(ble_adv_data_t* adv_data)
{
    if(adv_data == NULL){
        return;
    }

    if(adv_data->ad_data == NULL){
        adv_data->ad_data = default_ad_data;
        adv_data->ad_data_len = ARRAY_SIZE(default_ad_data);
    }
    if(adv_data->sr_data == NULL){
        adv_data->sr_data = default_sr_data;
        adv_data->sr_data_len = ARRAY_SIZE(default_sr_data);
    }
}

void ble_ad_sd_update(uint8_t electri, uint8_t device_status, uint16_t activity, uint8_t hr, float tempratrue)
{
	uint16_t tmp = 0;
	m_beacon_info.electricity = electri;
	m_beacon_info.fw_version = FW_VERSION;
	m_beacon_info.hw_version[0] = PROJECT_NUM;
	m_beacon_info.hw_version[1] = HW_VERSION;
	
	//memcpy(m_beacon_info.id, ble_addr.addr, 6);
	for(int i=0; i<DEV_ID_LENGTH; i++){
		m_beacon_info.id[i] = dev_id.device_id[i];
	}
	
	m_beacon_info.device_status = device_status;
	m_beacon_info.hr = hr;
	m_beacon_info.activity = activity;
	
	tmp = fabs(tempratrue)*100;
	m_beacon_info.temp[0] = ((tempratrue>0) ? (tmp/1000):(tmp/1000 + 0x80))<<4 | (tmp / 100 % 10);
	m_beacon_info.temp[1] = ((tmp / 10 % 10)<<4) + (tmp%10);

	memcpy(default_sr_data[0].data, &m_beacon_info, sizeof(m_beacon_info));

	ble_adv_data_prov(&ble_adv_data);
	bt_le_adv_update_data(ble_adv_data.ad_data, ble_adv_data.ad_data_len, 
				  ble_adv_data.sr_data, ble_adv_data.sr_data_len);
}
 
static void adv_work_handler(struct k_work *work)
{
	ble_adv_data_prov(&ble_adv_data);
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ble_adv_data.ad_data, ble_adv_data.ad_data_len, 
				  ble_adv_data.sr_data, ble_adv_data.sr_data_len);

	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}

void ble_advertising_start(void)
{
	k_work_submit(&adv_work);
}

/**
 * @brief 设置蓝牙 LE 发射功率
 * @param tx_power 目标功率，单位为 dBm。nRF52833 支持的值是：-20, -16, -12, -8, -4, 0, 4, 8。
 */
int set_ble_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_power) 
{
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				sizeof(*cp));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;
	cp->tx_power_level = tx_power;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				   buf, &rsp);
	if (err) {
		printk("Set Tx power err: %d\n", err);
	}

	rp = (void *)rsp->data;
	printk("Actual Tx Power: %d\n", rp->selected_tx_power);

	net_buf_unref(rsp);

    return err;
}

static void set_tx_power_work_handler(struct k_work *work)
{
    int err = set_ble_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, 4);
    if (err) {
        LOG_ERR("set ble tx power failed (err %d)", err);
    }
	err = set_ble_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, 0, 4);
    if (err) {
        LOG_ERR("set ble tx power failed (err %d)", err);
    }
}

int ble_init(void)
{
	int err;

#ifdef CONFIG_BT_CUSTOM_SRV_SECURITY_ENABLED
	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_ERR("Failed to register authorization callbacks. (err: %d)", err);
		return err;
	}

	err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
	if (err) {
		LOG_ERR("Failed to register authorization info callbacks. (err: %d)", err);
		return err;
	}
#endif

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");
	
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	// 延迟500ms后再设置TX功率，确保HCI通道就绪
    // k_work_init_delayable(&set_tx_power_work, set_tx_power_work_handler);
    // k_work_schedule(&set_tx_power_work, K_MSEC(500));

	err = bt_custom_srv_init(&custom_srv_cb);
	if (err) {
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return err;
	}

	k_work_init(&adv_work, adv_work_handler);
	k_sem_give(&ble_init_ok);
	
	ble_advertising_start();
	
	return 0;
}

int ble_send_data(const uint8_t *data, uint16_t len)
{
	if (!current_conn) {
		return -ENOTCONN;
	}
	
	// 检查MTU
	if(mtu == 0){
		mtu = bt_gatt_get_mtu(current_conn);
	}
	if (len > mtu) {
		LOG_ERR("Data length %d exceeds MTU %d", len, mtu);
		return -EMSGSIZE;
	}

	return bt_custom_srv_send(current_conn, data, len);
}

uint8_t msg_send_func(uint8_t* buf, uint16_t length, uint8_t ctrl)
{
	int ret = 0;
	uint8_t *str_ret = NULL;
	uint16_t total_len = length + 5;  // 总长度：头(1) + 长度(1) + 控制字(1) + 数据(len) + 校验(1) + 尾(1)
	uint16_t i = 0;
	static uint8_t static_buf[64];
	
	// 参数检查
	if(length > 234) return 1;
	
	// 根据长度选择使用静态数组还是动态分配
	if(total_len <= 64) {
		// 长度<=64，使用静态数组
		str_ret = static_buf;
	} else {
		// 长度>64，动态分配内存
		str_ret = (uint8_t*)k_malloc(total_len);
		if(str_ret == NULL) {
			return 1;  // 内存分配失败
		}
	}
	
	// 填充数据格式
	str_ret[0] = 0xAA;                         // 帧头
	str_ret[1] = (total_len - 2) & 0xFF;       // 长度（不含帧头和帧尾本身）
	str_ret[2] = ctrl;                          // 控制字
	memcpy(&str_ret[3], buf, length);           // 数据
	
	// 计算校验和（从第1个字节开始，到数据结束）
	uint8_t checksum = 0;
	for(i = 0; i < length + 2; i++) {
		checksum += str_ret[i + 1];
	}
	str_ret[3 + length] = checksum;             // 校验和
	
	str_ret[4 + length] = 0x55;                 // 帧尾
	
	// 发送数据
	if(total_len != 0) {
		ret = ble_send_data(str_ret, total_len);
	}
	
	// 如果是动态分配的，需要释放内存
	if(str_ret != static_buf) {
		k_free(str_ret);
	}
	
	return ret;
}

