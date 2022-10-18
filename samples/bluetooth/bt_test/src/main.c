/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2021 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/gatt.h>
#include <random/rand32.h>

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_URI, 'd', 'a', 't', 'a', ':', ',', 'H', 'e', 'l', 'l',
	'o', '%', '2', 'C', '%', '2', '0', 'W', 'o', 'r', 'l', 'd', '%', '2', '1'),
};

#define DEVICE_NAME "Zephyr"
#define DEVICE_NAME_LEN sizeof(DEVICE_NAME)

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

uint8_t rd_test_sz, nt_test_type, nt_test_szmin, nt_test_szmax;
uint16_t nt_test_intmin, nt_test_intmax, nt_test_ccc_val;
uint8_t test_buffer[256];

#define uuid128(...) BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(__VA_ARGS__))

static ssize_t u8read(struct bt_conn *conn,
                const struct bt_gatt_attr *attr, void *buf,
                uint16_t len, uint16_t offset)
{
    uint8_t *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
        1);
}
static ssize_t u8write(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	const uint8_t *in = buf;

	if (!len || len > 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
	*(uint8_t*)attr->user_data = *in;

	return len;
}

static ssize_t u16read(struct bt_conn *conn,
                const struct bt_gatt_attr *attr, void *buf,
                uint16_t len, uint16_t offset)
{
    uint8_t *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
        2);
}
static ssize_t u16write(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	const uint16_t *in = buf;

	if (!len || len > 2) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
	*(uint16_t*)attr->user_data = *in;

	return len;
}

static ssize_t testread(struct bt_conn *conn,
                const struct bt_gatt_attr *attr, void *buf,
                uint16_t len, uint16_t offset)
{
    uint8_t *value = attr->user_data;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
        rd_test_sz);
}

static ssize_t testwrite(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{

	if (!len || len + offset > rd_test_sz) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    
	memcpy(attr->user_data + offset, buf, len);

	return len;
}

static ssize_t errorread(struct bt_conn *conn,
                const struct bt_gatt_attr *attr, void *buf,
                uint16_t len, uint16_t offset)
{
	switch(sys_rand32_get() & 0x07){
		case 0:
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
		case 1:
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
		case 2:
			return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
		case 3:
			return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
		case 4:
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		case 5:
			return BT_GATT_ERR(BT_ATT_ERR_READ_NOT_PERMITTED);
		case 6:
			return BT_GATT_ERR(BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
		case 7:
		default:
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
}
static void notification_ccc_changed(const struct bt_gatt_attr *attr,
        uint16_t value)
{
    nt_test_ccc_val = value;
	printk("CCC set to %d", value);
}

static ssize_t errorwrite(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	switch(sys_rand32_get() & 0x07){
		case 0:
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
		case 1:
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
		case 2:
			return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
		case 3:
			return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
		case 4:
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		case 5:
			return BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED);
		case 6:
			return BT_GATT_ERR(BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
		case 7:
		default:
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
}



BT_GATT_SERVICE_DEFINE(test_svc, 
    BT_GATT_PRIMARY_SERVICE(uuid128(0x12e24ae3, 0xf208, 0x46e5, 0x99cb, 0x4183b9f3a72e)),
	BT_GATT_CHARACTERISTIC(uuid128(0xfaa1cf8a, 0x9cde, 0x424e, 0xb46e, 0x755ae5ece96b), //Notification test param
        BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE,
        BT_GATT_PERM_NONE, 
        NULL, NULL, NULL
    ),
	BT_GATT_CCC(notification_ccc_changed, \
            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(uuid128(0xe1da5d69, 0x5cb2, 0x4f6a, 0x9ec9, 0xa81e09c9e375), //Test Param Read Len
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        u8read, 
        u8write, &rd_test_sz
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0x5483910d, 0x58fe, 0x4520, 0x8f31, 0xf9790e6b9a3f), //Test Read Param
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ, 
        testread, 
        NULL, &test_buffer
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0x88d168be, 0x10f1, 0x43d3, 0x8217, 0x898bfd7815b), //Test Write Param
        BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_WRITE, 
        NULL, 
        testwrite, &test_buffer
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0x4a924c45, 0x9e3c, 0x4ef6, 0x9e75, 0x815ce6638d5f), //Test RW Param
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        testread, 
        testwrite, &test_buffer
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0x2175c40, 0x8284, 0x4845, 0x825d, 0x2c67fa953688), //Notification test type
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        u8read, 
        u8write, &nt_test_type
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0x9c605c97, 0x6e2c, 0x4527, 0xa54f, 0xfb6ff33372b7), //Notification test size min
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        u8read, 
        u8write, &nt_test_szmin
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0xaacd17c3, 0x65ba, 0x4144, 0x9f89, 0x3e525484ef3b), //Notification test size max
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        u8read, 
        u8write, &nt_test_szmax
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0xf8d11eae, 0xf101, 0x4b0e, 0x8374, 0x138d88b34790), //Notification test interval min
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        u16read, 
        u16write, &nt_test_intmin
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0x2a2602a7, 0x5061, 0x45e8, 0x9c69, 0xcde66257bdb5), //Notification test invterval max
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        u16read, 
        u16write, &nt_test_intmax
    ),
	BT_GATT_CHARACTERISTIC(uuid128(0x49cf0fae, 0xc8f1, 0x48a6, 0x9a2f, 0xc6a2259c9859), //Test Error Param
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, 
        errorread, 
        errorwrite, &test_buffer
    ),

); 

void start_adv_test()
{
	char addr_s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr = {0};
	size_t count = 1;
	int err;


	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));

	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}
	printk("Advertising successfully started\n");
}

static const struct bt_data ad_conn[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

void start_conn_test()
{
	int err;
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad_conn, ARRAY_SIZE(ad_conn), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }
	printk("Advertising successfully started\n");
}

void main(void)
{
	//Ad run for 30 seconds
	//then connectable ad
	//RW services: 1 big, 1 small; Run through steps;
	//CCC tests
	int err;
	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	// start_adv_test();

	// //k_msleep(30000);

	// err = bt_le_adv_stop();
	// if (err){
	// 	printk("Advertising stop failed (err%d)\n", err);
	// } else {
	// 	printk("Stopped Advertising\n");
	// }

	start_conn_test();

	printk("Starting connection/GATT test\n");

	for(size_t i = 0; i < 256; i++){
		test_buffer[i] = i;
	}
	uint8_t sz_delta;
	uint16_t tm_delta;
	while (true)
	{
		/* code */
		if (nt_test_ccc_val) {
			if (nt_test_type){
				sz_delta++;
				if (nt_test_szmax - nt_test_szmax){
					sz_delta %= (nt_test_szmax - nt_test_szmax);
				} else {
					sz_delta = 1;
				}
				bt_gatt_notify(NULL, &test_svc.attrs[1], test_buffer, nt_test_szmin + sz_delta);
				k_msleep(1000);
			} else {
				tm_delta += 100;
				if (nt_test_intmax - nt_test_intmin){
					tm_delta %= (nt_test_intmax - nt_test_intmin);
				} else {
					tm_delta = 100;
				}
				bt_gatt_notify(NULL, &test_svc.attrs[1], test_buffer, 32);
				k_msleep(nt_test_intmin + tm_delta);
			}
			
		} else {
			//printk("No notify\n");
			k_msleep(5000);
		}

	}
	printk("Hello World! %s\n", CONFIG_BOARD);
}
