/* main.c - Application main entry point */

/*
 * Copyright (c) 2021 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/bas.h>
#include <bluetooth/services/hrs.h>

//5432e12b-34be-4996-aafa-bdc0c7554b89
#define uuid128(...) BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(__VA_ARGS__))
#define BT_UUID_BWT_CHRC uuid128(0x5432e12b, 0x34be, 0x4996, 0xaafa, 0xbdc0c7554b89)
//7abd3505-2a8a-4dc6-b4ea-a4eb12fd6bf8
#define BT_UUID_BWT uuid128(0x7abd3505, 0x2a8a, 0x4dc6, 0xb4ea, 0xa4eb12fd6bf8)


int rcvd_len = 0;

static ssize_t write_test(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	rcvd_len += len;	
	return len;
}

BT_GATT_SERVICE_DEFINE(bw_test_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_BWT),
	BT_GATT_CHARACTERISTIC(BT_UUID_BWT_CHRC, BT_GATT_CHRC_WRITE,
				   BT_GATT_PERM_WRITE, NULL, write_test, NULL),
);

static const struct bt_data ad[] = {
		BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_le_conn_param fast_params = BT_LE_CONN_PARAM_INIT(6, 6, 0, 400);
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
		bt_conn_le_param_update(conn, &fast_params);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(void)
{
	int err;

	printk("Bluetooth initialized\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}


void main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	bt_ready();

	bt_conn_cb_register(&conn_callbacks);

	int last_rcvd_len = 0;
	int cur_rcvd_len;
	while (true) {
		k_msleep(5000);
		cur_rcvd_len = rcvd_len;
		if (last_rcvd_len != cur_rcvd_len) {
			printk("Total Received so far: %d\n", cur_rcvd_len);
			last_rcvd_len = cur_rcvd_len;
		}
	}

}
