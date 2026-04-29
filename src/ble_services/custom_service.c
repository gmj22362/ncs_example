/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

#include "custom_service.h"

LOG_MODULE_REGISTER(bt_custom_srv);

static struct bt_custom_srv_cb custom_srv_cb;

static void custom_srv_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				  uint16_t value)
{
	if (custom_srv_cb.send_enabled) {
		LOG_DBG("Notification has been turned %s",
			value == BT_GATT_CCC_NOTIFY ? "on" : "off");
		custom_srv_cb.send_enabled(value == BT_GATT_CCC_NOTIFY ?
			BT_CUSTOM_SRV_SEND_STATUS_ENABLED : BT_CUSTOM_SRV_SEND_STATUS_DISABLED);
	}
}

static ssize_t on_receive(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr,
			  const void *buf,
			  uint16_t len,
			  uint16_t offset,
			  uint8_t flags)
{
	LOG_DBG("Received data, handle %d, conn %p",
		attr->handle, (void *)conn);

	if (custom_srv_cb.received) {
		custom_srv_cb.received(conn, buf, len);
}
	return len;
}

static void on_sent(struct bt_conn *conn, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_DBG("Data send, conn %p", (void *)conn);

	if (custom_srv_cb.sent) {
		custom_srv_cb.sent(conn);
	}
}

/* UART Service Declaration */
BT_GATT_SERVICE_DEFINE(custom_srv_svc,
BT_GATT_PRIMARY_SERVICE(BT_UUID_CUSTOM_SRV_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_CUSTOM_SRV_TX,
			       BT_GATT_CHRC_NOTIFY,
#ifdef CONFIG_BT_CUSTOM_SRV_AUTHEN
			       BT_GATT_PERM_READ_AUTHEN,
#else
			       BT_GATT_PERM_READ,
#endif /* CONFIG_BT_CUSTOM_SRV_AUTHEN */
			       NULL, NULL, NULL),
	BT_GATT_CCC(custom_srv_ccc_cfg_changed,
#ifdef CONFIG_BT_CUSTOM_SRV_AUTHEN
			       BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN),
#else
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
#endif /* CONFIG_BT_CUSTOM_SRV_AUTHEN */
	BT_GATT_CHARACTERISTIC(BT_UUID_CUSTOM_SRV_RX,
			       BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
#ifdef CONFIG_BT_CUSTOM_SRV_AUTHEN
			       BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
#else
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
#endif /* CONFIG_BT_CUSTOM_SRV_AUTHEN */
			       NULL, on_receive, NULL),
);

int bt_custom_srv_init(struct bt_custom_srv_cb *callbacks)
{
	if (callbacks) {
		custom_srv_cb.received = callbacks->received;
		custom_srv_cb.sent = callbacks->sent;
		custom_srv_cb.send_enabled = callbacks->send_enabled;
	}

	return 0;
}

int bt_custom_srv_send(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
	struct bt_gatt_notify_params params = {0};
	const struct bt_gatt_attr *attr = &custom_srv_svc.attrs[2];

	params.attr = attr;
	params.data = data;
	params.len = len;
	params.func = on_sent;

	if (!conn) {
		LOG_DBG("Notification send to all connected peers");
		return bt_gatt_notify_cb(NULL, &params);
	} else if (bt_gatt_is_subscribed(conn, attr, BT_GATT_CCC_NOTIFY)) {
		return bt_gatt_notify_cb(conn, &params);
	} else {
		return -EINVAL;
	}
}
