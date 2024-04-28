/*
 * Copyright (c) 2015-2016 Intel Corporation
 * Copyright (c) 2024 Thomas Sowell
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Adapted from zephyr/samples/bluetooth/central_hr/src/main.c */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include <zephyr/drivers/uart.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/drivers/led_strip.h>

#include "vtbt.h"
#include "lk201.h"

#define STRIP_NODE              DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS        DT_PROP(DT_ALIAS(led_strip), chain_length)

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static void (*hid_report_cb)(const uint8_t *report);

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

static const struct led_rgb color_black = { .r = 0x00, .g = 0x00, .b = 0x00 };
static const struct led_rgb color_amber = { .r = 0x03, .g = 0x01, .b = 0x00 };
static const struct led_rgb color_blue  = { .r = 0x00, .g = 0x00, .b = 0x04 };
static const struct led_rgb color_green = { .r = 0x00, .g = 0x04, .b = 0x00 };

static void
rgb_led_set(const struct led_rgb *color)
{
	memset(&pixels, 0x00, sizeof(pixels));
	for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
		memcpy(&pixels[i], color, sizeof(struct led_rgb));
	}
	int rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
	if (rc) {
		LOG_ERR("Couldn't update strip: %d", rc);
	}
}

static void start_scan(void);

static struct bt_conn *default_conn;

static struct bt_uuid_16 discover_uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

static void
pairing_complete_func(struct bt_conn *conn, bool bonded)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(bonded);

	rgb_led_set(&color_amber);
}

static struct bt_conn_auth_info_cb auth_info_cb = {
	.pairing_complete = pairing_complete_func,
	.pairing_failed = NULL,
	.bond_deleted = NULL,
};

static uint8_t
notify_func(struct bt_conn *conn,
            struct bt_gatt_subscribe_params *params,
            const void *data, uint16_t length)
{
	ARG_UNUSED(conn);

	if (!data) {
		LOG_INF("[UNSUBSCRIBED]");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	if (length == HID_REPORT_SIZE) {
		hid_report_cb((const uint8_t *)data);
	} else {
		LOG_INF("[NOTIFICATION] data %p length %u", data, length);
	}

	return BT_GATT_ITER_CONTINUE;
}

static uint16_t hids_handle = 0;

static uint8_t
discover_func(struct bt_conn *conn,
              const struct bt_gatt_attr *attr,
              struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		LOG_INF("Discover complete");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_INF("[ATTRIBUTE] handle %u", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HIDS)) {
		hids_handle = attr->handle;
		memcpy(&discover_uuid, BT_UUID_HIDS_BOOT_KB_IN_REPORT,
		       sizeof(discover_uuid));
		discover_params.uuid = &discover_uuid.uuid;
		discover_params.start_handle = hids_handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
	                        BT_UUID_HIDS_BOOT_KB_IN_REPORT)) {
		memcpy(&discover_uuid, BT_UUID_HIDS_BOOT_KB_OUT_REPORT,
		       sizeof(discover_uuid));
		discover_params.uuid = &discover_uuid.uuid;
		discover_params.start_handle = hids_handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
	                        BT_UUID_HIDS_BOOT_KB_OUT_REPORT)) {
		memcpy(&discover_uuid, BT_UUID_HIDS_REPORT,
		       sizeof(discover_uuid));
		discover_params.uuid = &discover_uuid.uuid;
		discover_params.start_handle = hids_handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HIDS_REPORT)) {
		memcpy(&discover_uuid, BT_UUID_GATT_CCC, sizeof(discover_uuid));
		discover_params.uuid = &discover_uuid.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}
	} else {
		subscribe_params.notify = notify_func;
		subscribe_params.value = BT_GATT_CCC_NOTIFY;
		subscribe_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			LOG_ERR("Subscribe failed (err %d)", err);
		} else {
			LOG_ERR("[SUBSCRIBED]");
			if (bt_conn_get_security(conn) >= BT_SECURITY_L2) {
				rgb_led_set(&color_amber);
			} else {
				rgb_led_set(&color_green);
			}
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static bool
eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	int i;

	switch (data->type) {
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		if (data->data_len % sizeof(uint16_t) != 0U) {
			LOG_ERR("AD malformed");
			return true;
		}

		for (i = 0; i < data->data_len; i += sizeof(uint16_t)) {
			struct bt_le_conn_param *param;
			const struct bt_uuid *uuid;
			uint16_t u16;
			int err;

			memcpy(&u16, &data->data[i], sizeof(u16));
			uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
			if (bt_uuid_cmp(uuid, BT_UUID_HIDS)) {
				continue;
			}

			err = bt_le_scan_stop();
			if (err) {
				LOG_ERR("Stop LE scan failed (err %d)", err);
				continue;
			}

			param = BT_LE_CONN_PARAM_DEFAULT;
			err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
						param, &default_conn);
			if (err) {
				LOG_ERR("Create conn failed (err %d)", err);
				start_scan();
			}

			return false;
		}
	}

	return true;
}

static void
device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
             struct net_buf_simple *ad)
{
	ARG_UNUSED(rssi);

	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));

	/* We're only interested in connectable events */
	if (type == BT_GAP_ADV_TYPE_ADV_IND ||
	    type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_data_parse(ad, eir_found, (void *)addr);
	}
}

static void
start_scan(void)
{
	int err;

	/* Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime. */
	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return;
	}

	LOG_INF("Scanning successfully started");

	rgb_led_set(&color_blue);
}

static void
connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_ERR("Failed to connect to %s (%u)", addr, conn_err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	LOG_INF("Connected: %s", addr);

	if (conn == default_conn) {
		bt_conn_set_security(conn, BT_SECURITY_L2);

		memcpy(&discover_uuid, BT_UUID_HIDS, sizeof(discover_uuid));
		discover_params.uuid = &discover_uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
		discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed(err %d)", err);
			return;
		}
	}
}

static void
disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason 0x%02x)", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int
bluetooth_listen(void (*callback)(const uint8_t *))
{
	hid_report_cb = callback;

	if (device_is_ready(strip)) {
		LOG_INF("Found LED strip device %s", strip->name);
	} else {
		LOG_ERR("LED strip device %s is not ready", strip->name);
	}

	/* Not sure why this needs to be called two times at first */
	rgb_led_set(&color_black);
	rgb_led_set(&color_black);

	int err;
	err = bt_enable(NULL);

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	bt_set_bondable(true);

	bt_passkey_set(123456);

	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	bt_conn_auth_info_cb_register(&auth_info_cb);

	LOG_INF("Bluetooth initialized");

	start_scan();

	LOG_INF("Listening");

	return 0;
}
