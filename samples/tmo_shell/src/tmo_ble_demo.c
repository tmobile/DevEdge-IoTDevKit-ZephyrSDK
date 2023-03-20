/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/posix/fcntl.h>
#include <zephyr/types.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/random/rand32.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/modem/murata-1sc.h>
#include "modem_context.h"
#include <zephyr/fs/fs.h>
#include <zephyr/shell/shell.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/bluetooth/rs9116w.h>

#include "tmo_buzzer.h"
#include "tmo_web_demo.h"
#include "tmo_ble_demo.h"
#include "tmo_adc.h"
#include "tmo_gnss.h"
#include "tmo_smp.h"
#include "tmo_shell.h"
#include "tmo_battery_ctrl.h"
#include "board.h"

#define ON_CHARGER_POWER 0
#define ON_BATTERY_POWER 1

static inline void strupper(char *p) { while (*p) *p++ &= 0xdf;}
#define uuid128(...) BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(__VA_ARGS__))

extern struct bt_conn *get_acl_conn(int i);
extern int get_active_le_conns();

K_SEM_DEFINE(update_sem, 0, 1);

/**
 * @brief Generic 8-bit integer read callback
 */
static ssize_t i8_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	uint8_t *value = attr->user_data;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 1);
}

ssize_t i32_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	uint8_t *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 4);
}

ssize_t ln_las_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	ln_buf_gen();
	uint8_t *value = attr->user_data;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(ln_las_buf));
}

ssize_t ln_quality_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	ln_buf_gen();
	uint8_t *value = attr->user_data;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 6);
}

void las_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	las_notify = value == BT_GATT_CCC_NOTIFY;
	printf("LAS CCC: %2x\n", value);
}

/** Acceleration and Orientation Service Data */
#define UUID_SERVICE_ACCELERATION_ORIENTATION \
	uuid128(0xa4e649f4, 0x4be5, 0x11e5, 0x885d, 0xfeff819cdc9f)
#define UUID_CHARACTERISTIC_ACCELERATION \
	uuid128(0xc4c1f6e2, 0x4be5, 0x11e5, 0x885d, 0xfeff819cdc9f)
static bool acc_notify = false;

/* Sensors */
static const struct device *acc_sensor = NULL;
static const struct device *temp_sensor = NULL;
static const struct device *press_sensor = NULL;
static const struct device *light_sensor = NULL;
static const struct device *ir_sensor = NULL;

static ssize_t temp_read(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{//temp = .01degree c sint16
	uint16_t rand_temp = 2221 + (uint8_t)sys_rand32_get() - 127;
	int16_t real_temp;
	uint8_t *value = (uint8_t*)&rand_temp;

	if (temp_sensor) {
		struct sensor_value temp;
		if (sensor_sample_fetch(temp_sensor) >= 0) {
			sensor_channel_get(temp_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			double tvalue = sensor_value_to_double(&temp) * 100;
			real_temp = tvalue;
			value = (uint8_t*)&real_temp;
		}
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 2);
}

static ssize_t press_read(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{//pressure = .1 Pa uint32
	uint32_t rand_press = (100000 + (uint8_t)sys_rand32_get() - 127) * 10;
	uint32_t real_press;
	uint8_t *value = (uint8_t*)&rand_press;

	if (press_sensor) {
		struct sensor_value press;
		if (sensor_sample_fetch(press_sensor) >= 0) {
			sensor_channel_get(press_sensor, SENSOR_CHAN_PRESS, &press);
			double pvalue = sensor_value_to_double(&press) * 10000;
			real_press = pvalue;
			value = (uint8_t*)&real_press;
		}
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 4);
}

static ssize_t ambient_light_read(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	// Default value
	uint32_t real_light = 0;

	if (light_sensor) {
		struct sensor_value light_value;
		if (sensor_sample_fetch(light_sensor) >= 0) {
			sensor_channel_get(light_sensor, SENSOR_CHAN_LIGHT, &light_value);
			double pvalue = sensor_value_to_double(&light_value);
			real_light = 100 * pvalue;
		}
	}
	return bt_gatt_attr_read(conn, attr, buf, len, offset, (uint8_t*) &real_light, 4);
}

static ssize_t ambient_ir_read(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	// Default value
	uint32_t real_ir = 0;

	if (ir_sensor) {
		struct sensor_value ir_value;
		if (sensor_sample_fetch(ir_sensor) >= 0) {
			sensor_channel_get(ir_sensor, SENSOR_CHAN_IR, &ir_value);
			double pvalue = sensor_value_to_double(&ir_value);
			real_ir = 10 * pvalue;
		}
	}
	return bt_gatt_attr_read(conn, attr, buf, len, offset, (uint8_t*) &real_ir, 4);
}

static ssize_t battery_voltage_get(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset)
{
	uint8_t percent = 0;
	uint32_t millivolts = 0;
	uint8_t battery_attached = 0;
	uint8_t charging = 0;
	uint8_t vbus = 0;
	uint8_t fault = 0;

	get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);

	/* flush the fault status out by reading again */
	if (fault) {
		get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
	}
	/* there can be 2 of these to flush */
	if (fault) {
		get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
	}

	millivolts = read_battery_voltage();
	millivolts_to_percent(millivolts, &percent);
	
	return bt_gatt_attr_read(conn, attr, buf, len, offset, (uint8_t*) &percent, 1);
}

static ssize_t battery_power_source_get(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr, void *buf,
                                uint16_t len, uint16_t offset)
{
	uint8_t power_source;
	uint8_t charging, vbus, battery_attached, fault;

	get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
	if (vbus) {
		power_source = ON_CHARGER_POWER;
	} else {
		power_source = ON_BATTERY_POWER;
	}
	return bt_gatt_attr_read(conn, attr, buf, len, offset, (uint8_t*) &power_source, 1);
}

static void dummy_ccc_cfg_changed(const struct bt_gatt_attr *attr,
		uint16_t value)
{
	printf("CCC: %2x\n", value);
}

/**
 * @brief Acceleration CCC callback
 *
 */
static void acceleration_ccc_cfg_changed(const struct bt_gatt_attr *attr,
		uint16_t value)
{
	acc_notify = value == BT_GATT_CCC_NOTIFY;
	printf("Acceleration CCC: %2x\n", value);
}

/**
 * @brief Read Callback for acceleration/orientation data
 *
 */
static ssize_t acc_ori_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	int16_t value[3] = {0};

	if (acc_sensor != NULL) {
		struct sensor_value accel[3];
		int rc = sensor_sample_fetch(acc_sensor);

		if (rc == 0) {
			rc = sensor_channel_get(acc_sensor,
					SENSOR_CHAN_ACCEL_XYZ,
					accel);
		}
		if (rc < 0) {
			printf("ERROR: Update failed: %d\n", rc);
		} else {
			double xacc = sensor_value_to_double(&accel[0]);
			double yacc = sensor_value_to_double(&accel[1]);
			double zacc = sensor_value_to_double(&accel[2]);
			value[0] = (int16_t)(xacc * 100);
			value[1] = (int16_t)(yacc * 100);
			value[2] = (int16_t)(zacc * 100);
		}
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 6);
}

/** Define Location Service */
BT_GATT_SERVICE_DEFINE(ln_svc, //Todo: uuids
		BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0x1819)),
		BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2a6a),
			BT_GATT_CHRC_READ,
			BT_GATT_PERM_READ, i32_rd_cb, NULL, &ln_ln_feature
			),
		BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2a67),
			BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			BT_GATT_PERM_READ, ln_las_rd_cb, NULL, ln_las_buf
			),
		BT_GATT_CCC(las_ccc_cfg_changed, \
			BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
		BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2a69),
			BT_GATT_CHRC_READ,
			BT_GATT_PERM_READ, ln_quality_rd_cb, NULL, ln_quality_buf
			),
		);

#define  UUID_CHARACTERISTIC_AMBIENT_IR\
	uuid128(0xeedc804d, 0xaf50, 0x4488, 0x942e, 0xb4e9043f1687)

/** Define Environmental Sensing Service */
BT_GATT_SERVICE_DEFINE(env_svc,
		BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
		BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE,
			BT_GATT_CHRC_READ,
			BT_GATT_PERM_READ,
			press_read,
			NULL, NULL
			),
		BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE,
			BT_GATT_CHRC_READ,
			BT_GATT_PERM_READ,
			temp_read,
			NULL, NULL
			),
		BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2AFB),
			BT_GATT_CHRC_READ,
			BT_GATT_PERM_READ,
			ambient_light_read,
			NULL, NULL
			),
		BT_GATT_CHARACTERISTIC(UUID_CHARACTERISTIC_AMBIENT_IR,
			BT_GATT_CHRC_READ,
			BT_GATT_PERM_READ,
			ambient_ir_read,
			NULL, NULL
			),
		BT_GATT_CCC(dummy_ccc_cfg_changed, \
				BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
		);

/** Define Acceleration/Orientation Service */
BT_GATT_SERVICE_DEFINE(acc_ori_svc,
		BT_GATT_PRIMARY_SERVICE(UUID_SERVICE_ACCELERATION_ORIENTATION),
		BT_GATT_CHARACTERISTIC(UUID_CHARACTERISTIC_ACCELERATION,
			BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ,
			BT_GATT_PERM_READ, acc_ori_rd_cb, NULL, NULL
			),
		BT_GATT_CCC(acceleration_ccc_cfg_changed, \
			BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
		);

#define UUID_BAS_BATTERY_POWER_SOURCE \
        uuid128(0xEC61A454, 0xED01, 0xA5E8, 0x88F9, 0xDE9EC026EC51)
BT_GATT_SERVICE_DEFINE(bas,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
	BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL,
				   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_GATT_PERM_READ, battery_voltage_get, NULL,
				   NULL),
	BT_GATT_CCC(dummy_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(UUID_BAS_BATTERY_POWER_SOURCE,
                                   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                   BT_GATT_PERM_READ, battery_power_source_get, NULL,
                                   NULL),
		);

#define UUID_SERVICE_AUTOMATION_IO \
	BT_UUID_DECLARE_16(0x1815)

// Automation IO Service
#define UUID_CHARACTERISTIC_DIGITAL_TO_BOARD \
	BT_UUID_DECLARE_16(0x2a56)

#define UUID_CHARACTERISTIC_DIGITAL_FROM_BOARD \
	BT_UUID_DECLARE_16(0x2a57)

#define UUID_DESCRIPTOR_NUMBER_OF_DIGITALS \
	BT_UUID_DECLARE_16(0x2909)

struct bt_gatt_cpf aio_in_digital_fmt = {
	.format = 0x1B, //Struct
	.exponent = 0,
	.unit = 0x2700, //Unitless
	.name_space = 1,
	.description = 2
};


struct bt_gatt_cpf aio_out_digital_fmt = {
	.format = 0x1B, //Struct
	.exponent = 0,
	.unit = 0x2700, //Unitless
	.name_space = 1,
	.description = 1
};

// static uint8_t aio_btn_format[] = { 0x1B, 0x00, 0x00, 0x27, 0x01, 0x02, 0x00};
// static uint8_t aio_led_format[] = { 0x1B, 0x00, 0x00, 0x27, 0x01, 0x01, 0x00};

uint8_t aio_in_digital_cnt = 1;
uint8_t aio_out_digital_cnt = 5;
uint8_t aio_digital_output_state = 0;
bool aio_btn_notify;



#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>

#define SW0_NODE	DT_ALIAS(sw0)
static struct gpio_callback button_cb_data;

static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
		{0});

uint8_t aio_btn_pushed = 0;

void button_stat_change(const struct device *dev, struct gpio_callback *cb,
		uint32_t pins);

void setup_buttons(void){
	int ret;
	if (!device_is_ready(button0.port)) {
		printf("Error: button device %s is not ready\n",
				button0.port->name);
		return;
	}
	ret = gpio_pin_configure_dt(&button0, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		printf("Error %d: failed to configure %s pin %d\n",
				ret, button0.port->name, button0.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button0,
			GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printf("Error %d: failed to configure interrupt on %s pin %d\n",
				ret, button0.port->name, button0.pin);
		return;
	}
	gpio_init_callback(&button_cb_data, button_stat_change, BIT(button0.pin));
	gpio_add_callback(button0.port, &button_cb_data);
}

#define LED0_NODE DT_ALIAS(led0)

#include "tmo_leds.h"

const struct device *pwm_buzzer = DEVICE_DT_GET(DT_ALIAS(pwm_buzzer));

static ssize_t write_aio_digital_output(struct bt_conn *conn,
		const struct bt_gatt_attr *attr,
		const void *buf, uint16_t len, uint16_t offset,
		uint8_t flags)
{
	const uint8_t *in = buf;
	if (!len) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	aio_digital_output_state = *in;
	const struct device *dev;
	int ret = 0;
	dev = PWMLEDS;
	if (dev == NULL) {
		return len;
	}

	if (ret < 0) {
		return len;
	}
#ifdef LED_PWM_WHITE
	if (aio_digital_output_state & BIT(3)){
		led_on(dev, LED_PWM_WHITE);
	} else {
		led_off(dev, LED_PWM_WHITE);
	}
#endif /* LED_PWM_WHITE */

	dev = PWMLEDS;
	if (dev == NULL) {
		return len;
	}

	if (ret < 0) {
		return len;
	}

	if (aio_digital_output_state & BIT(0)){
		led_on(dev, LED_PWM_RED);
	} else {
		led_off(dev, LED_PWM_RED);
	}
	dev = PWMLEDS;
	if (dev == NULL) {
		return len;
	}

	if (ret < 0) {
		return len;
	}

	if (aio_digital_output_state & BIT(1)){
		led_on(dev, LED_PWM_GREEN);
	} else {
		led_off(dev, LED_PWM_GREEN);
	}
	dev = PWMLEDS;
	if (dev == NULL) {
		return len;
	}

	if (ret < 0) {
		return len;
	}

	if (aio_digital_output_state & BIT(2)){
		led_on(dev, LED_PWM_BLUE);
	} else {
		led_off(dev, LED_PWM_BLUE);
	}
	if (ret < 0) {
		return len;
	}

	if (aio_digital_output_state & BIT(4)){
		pwm_set_frequency(pwm_buzzer, 0, 880);
	} else {
		pwm_set_frequency(pwm_buzzer, 0, 0);
	}
	return len;
}

static void aio_in_digital_ccc_cfg_changed(const struct bt_gatt_attr *attr,
		uint16_t value)
{
	aio_btn_notify = value == BT_GATT_CCC_NOTIFY;
	printf("BTN Notify is %s\n", aio_btn_notify ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(aio_svc,
		BT_GATT_PRIMARY_SERVICE(UUID_SERVICE_AUTOMATION_IO),
		BT_GATT_CHARACTERISTIC(UUID_CHARACTERISTIC_DIGITAL_FROM_BOARD,
			BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			BT_GATT_PERM_READ,
			i8_rd_cb,
			NULL, &aio_btn_pushed
			),
		BT_GATT_CCC(aio_in_digital_ccc_cfg_changed, \
			BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
		BT_GATT_DESCRIPTOR(UUID_DESCRIPTOR_NUMBER_OF_DIGITALS,
			BT_GATT_PERM_READ, i8_rd_cb, NULL,
			&aio_in_digital_cnt
			),
		BT_GATT_CHARACTERISTIC(UUID_CHARACTERISTIC_DIGITAL_TO_BOARD,
			BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			i8_rd_cb,
			write_aio_digital_output,
			&aio_digital_output_state
			),
		BT_GATT_DESCRIPTOR(UUID_DESCRIPTOR_NUMBER_OF_DIGITALS,
				BT_GATT_PERM_READ, i8_rd_cb, NULL,
				&aio_out_digital_cnt
				),
		);

K_SEM_DEFINE(ble_thd_sem, 0, 1);

void button_stat_change(const struct device *dev, struct gpio_callback *cb,
		uint32_t pins)
{
	aio_btn_pushed = (gpio_pin_get(button0.port, button0.pin) > 0)? 1 : 0;
	k_sem_give(&update_sem);
	k_sem_give(&ble_thd_sem);
}

#if CONFIG_MODEM

static uint64_t get_imei()
{
	struct net_if *iface = net_if_get_by_index(1);
	int sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);
	if (sd == -1) {
		return 0;
	}
	uint8_t buf[16] = {0};
	strcpy(buf, "IMEI");
	strupper(buf);
	int res = fcntl(sd, GET_ATCMD_RESP, buf);
	zsock_close(sd);
	if (res >= 0) {
		uint8_t partial[8] = {0};
		memcpy(partial, buf, 7);
		uint64_t val = strtol(partial, NULL, 10);
		val *= 100000000;
		val += strtol(buf + 7, NULL, 10);
		return val;
	} else {
		return 0;
	}
}

static ssize_t imei_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	uint64_t imei = get_imei();
	uint8_t *value = (uint8_t *)&imei;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 8);
}

static ssize_t cell_rssi_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	int rssi_value;
	int8_t rssi_byte_value;
	uint8_t *value = attr->user_data;
	get_cell_strength(&rssi_value);
	rssi_byte_value = (int8_t) rssi_value;
	value = (uint8_t *) &rssi_byte_value;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, 1);
}

// Cellular Service UUID: 2618484c-7465-441d-bc3f-35f1af1c6f16
#define UUID_TMO_CELL_SVC \
	uuid128(0x2618484c, 0x7465, 0x441d, 0xbc3f, 0x35f1af1c6f16)

// Cell strength characteristic: 0e3b403a4-e97d-4401-8d09-87b6af705298 int8t
#define UUID_TMO_CELL_RSSI \
	uuid128(0x0e3b403a4, 0xe97d, 0x4401, 0x8d09, 0x87b6af705298)

// Imei Characteristic: 02d93bc0-46da-4444-9527-a063f082023b uint64t
#define UUID_TMO_CELL_IMEI \
	uuid128(0x02d93bc0, 0x46da, 0x4444, 0x9527, 0xa063f082023b)

#endif /* CONFIG_MODEM */
K_SEM_DEFINE(wifi_status_refresh_sem, 0, 1);
uint8_t conn_ssid[WIFI_SSID_MAX_LEN] = {0};
uint8_t ssid_len = 0;
int8_t rssi = 0;

/**
 * @brief Read Callback SSID characteristic
 *
 */
static ssize_t ssid_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	k_sem_give(&wifi_status_refresh_sem);

	uint8_t *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
			ssid_len);
}

/**
 * @brief Read Callback RSSI characteristic
 *
 */
static ssize_t rssi_rd_cb(struct bt_conn *conn,
		const struct bt_gatt_attr *attr, void *buf,
		uint16_t len, uint16_t offset)
{
	k_sem_give(&wifi_status_refresh_sem);

	return i8_rd_cb(conn, attr, buf, len, offset);
}


// Connectivity Service UUID: 75c7e8df-376a-4171-a096-41d486bb3d72
#define UUID_TMO_CONN_SVC \
	uuid128(0x75c7e8df, 0x376, 0x4171, 0xa096, 0x41d486bb3d72)

// Wifi name Characteristic: 2618484d-7465-bc3f-b8f9-35f1af1c6f16 str
#define UUID_TMO_WIFI_SSID \
	uuid128(0x2618484d, 0x7465, 0xbc3f, 0xb8f9, 0x35f1af1c6f16)

// Wifi Strength Characteristic: 5ed074fa-7205-4395-a95c-2223928bdc64 int8t
#define UUID_TMO_WIFI_RSSI \
	uuid128(0x5ed074fa, 0x7205, 0x4395, 0xa95c, 0x2223928bdc64)


BT_GATT_SERVICE_DEFINE(wifi_svc,
		BT_GATT_PRIMARY_SERVICE(UUID_TMO_CONN_SVC),
		BT_GATT_CHARACTERISTIC(UUID_TMO_WIFI_SSID,
			BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			ssid_rd_cb, NULL, conn_ssid
			),
		BT_GATT_CHARACTERISTIC(UUID_TMO_WIFI_RSSI,
			BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			rssi_rd_cb, NULL, &rssi
			),
#ifdef CONFIG_MODEM
		BT_GATT_CHARACTERISTIC(UUID_TMO_CELL_IMEI,
			BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			imei_rd_cb, NULL, NULL
			),
		BT_GATT_CHARACTERISTIC(UUID_TMO_CELL_RSSI,
			BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			cell_rssi_rd_cb, NULL, NULL
			),
#endif /* CONFIG_MODEM */
		);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_data ebeacon_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
			0xaa, 0xfe, /* Eddystone UUID */
			0x10, /* Eddystone-URL frame type */
			0x00, /* Calibrated Tx power at 0m */
			0x03, /* URL Scheme Prefix https:// */
			'd', 'e', 'v', 'e', 'd', 'g',
			'e', '.', 't', '-', 'm', 'o', 'b',
			'i', 'l', 'e',
			0x07) /* .com */
};

static const struct bt_data ibeacon_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
			0x4c, 0x00, /* Apple */
			0x02, 0x15, /* iBeacon */
			0x54, 0x2d, 0x4d, 0x4f, /* UUID[15..12] */
			0x42, 0x49, /* UUID[11..10] */
			0x4c, 0x45, /* UUID[9..8] */
			0xb4, 0x44, /* UUID[7..6] */
			0x45, 0x56, 0x45, 0x44, 0x47, 0x45, /* UUID[5..0] */
			0x00, 0x00, /* Major */
			0x00, 0x00, /* Minor */
			0xBD) /* Calibrated RSSI @ 1m */
};

#define IBEACON_PAYLOAD_SIZE 25

static uint8_t ibeacon_custom_data[25];

static struct bt_data ibeacon_custom[2] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	{
		.type = BT_DATA_MANUFACTURER_DATA,
		.data_len = 25,
		.data = ibeacon_custom_data
	}
};

#if CONFIG_WIFI

static struct net_mgmt_event_callback ble_demo_mgmt_cb;

static void ble_sample_wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
		uint32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event ==  NET_EVENT_WIFI_STATUS_RESULT){
		const struct wifi_status_result *res =
			(const struct wifi_status_result *)cb->info;
		memset(conn_ssid, 0, sizeof(conn_ssid));
		memcpy(conn_ssid, res->ssid, res->ssid_length);
		ssid_len = res->ssid_length;
		rssi = res->rssi;
	}
}

void ble_wifi_status_retrieve(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	while (1) {
		k_sem_take(&wifi_status_refresh_sem, K_FOREVER);
		struct net_if *iface = net_if_get_by_index(2); //Hardcoded for now

		net_mgmt(NET_REQUEST_WIFI_STATUS, iface, NULL, 0);
	}
}

#define WIFI_THREAD_STACK_SIZE 2048
#define WIFI_THREAD_PRIORITY CONFIG_MAIN_THREAD_PRIORITY

K_THREAD_DEFINE(wifi_tid, WIFI_THREAD_STACK_SIZE,
		ble_wifi_status_retrieve, NULL, NULL, NULL,
		WIFI_THREAD_PRIORITY, 0, 0);


#endif

void ble_notif_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	uint8_t button_last_state = 0;
	uint8_t battery_last_percent = 0;

	while (1) {
		if (!get_active_le_conns()) {
			k_sem_take(&ble_thd_sem, K_FOREVER);
		}
		k_sem_take(&update_sem, K_MSEC(200));
		if (acc_notify) {
			int16_t acc_value[3] = {0};

			if (acc_sensor != NULL) {
				struct sensor_value accel[3];
				int rc = sensor_sample_fetch(acc_sensor);

				if (rc == 0) {
					rc = sensor_channel_get(acc_sensor,
							SENSOR_CHAN_ACCEL_XYZ,
							accel);
				}
				if (rc < 0) {
					printf("ERROR: Update failed: %d\n", rc);
				} else {
					double xacc = sensor_value_to_double(&accel[0]);
					double yacc = sensor_value_to_double(&accel[1]);
					double zacc = sensor_value_to_double(&accel[2]);
					acc_value[0] = (int16_t)(xacc * 100);
					acc_value[1] = (int16_t)(yacc * 100);
					acc_value[2] = (int16_t)(zacc * 100);
				}
			}
			bt_gatt_notify(NULL, &acc_ori_svc.attrs[1], &acc_value, 6);
		}
		if (get_active_le_conns() && aio_btn_notify && aio_btn_pushed != button_last_state){
			button_last_state = aio_btn_pushed;
			bt_gatt_notify(NULL, &aio_svc.attrs[1], &aio_btn_pushed, 1);
		} else if (aio_btn_pushed != button_last_state && aio_btn_pushed) {
			button_last_state = aio_btn_pushed;
			tmo_play_jingle();
		} else if (aio_btn_pushed != button_last_state) {
			button_last_state = aio_btn_pushed;
		}
		if (las_notify){
			ln_buf_gen();
			bt_gatt_notify(NULL, &ln_svc.attrs[2], ln_las_buf, sizeof(ln_las_buf));
		}
		uint8_t charging, vbus, battery_attached, fault, percent;
	
		get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
		uint32_t millivolts = read_battery_voltage();
		millivolts_to_percent(millivolts, &percent);
		if (battery_last_percent != percent) {
			bt_gatt_notify(NULL, &bas.attrs[1], &percent, sizeof(percent));
		}
		battery_last_percent = percent;
	}
}

#define BLE_NOTIF_THREAD_STACK_SIZE 2048
#define BLE_NOTIF_THREAD_PRIORITY CONFIG_MAIN_THREAD_PRIORITY

K_THREAD_DEFINE(ble_notif_tid, BLE_NOTIF_THREAD_STACK_SIZE,
		ble_notif_thread, NULL, NULL, NULL,
		BLE_NOTIF_THREAD_PRIORITY, 0, 0);

static void ble_connected(struct bt_conn *conn, uint8_t err)
{
	const struct device *dev = PWMLEDS;
	ARG_UNUSED(conn);
	ARG_UNUSED(err);
	k_sem_give(&ble_thd_sem);
	
	led_off(dev, LED_PWM_RED);
	led_off(dev, LED_PWM_GREEN);
	led_off(dev, LED_PWM_BLUE);
	#ifdef LED_PWM_WHITE
	led_off(dev, LED_PWM_WHITE);
	#endif /* LED_PWM_WHITE */
}

static struct bt_conn_cb conn_callbacks = {
	.connected = ble_connected,
};

static int tmo_ble_demo_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	int err;

	err = bt_enable(NULL);
	if (err) {
		printf("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printf("Bluetooth initialized\n");

#ifdef CONFIG_MODEM
	struct modem_context *mctx = modem_context_from_id(0);
	char *imei = mctx->data_imei;
	char devname[17] = {0};
	char l4_imei[5] = {0};
	strncpy(l4_imei, imei + strlen(imei) - 4, 4);
	strcpy(devname, bt_get_name());
	strcat(devname, ":");
	strcat(devname, l4_imei);
	bt_set_name(devname);
#endif

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printf("Advertising failed to start (err %d)\n", err);
		return 0;
	}

	printf("Advertising successfully started\n");
#if CONFIG_BT_SMP
	tmo_smp_shell_init();
#endif
	bt_conn_cb_register(&conn_callbacks);
	acc_sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dw12));
	temp_sensor = DEVICE_DT_GET(DT_NODELABEL(as6212));
#if CONFIG_LPS22HH
	press_sensor = DEVICE_DT_GET(DT_NODELABEL(lps22hh));
#else
	press_sensor = NULL;
#endif
	ir_sensor = light_sensor = DEVICE_DT_GET(DT_NODELABEL(tsl2540));  // idx = 17, idx = 18

	if (press_sensor) {
		struct sensor_value attr = {
			.val1 = 1,
		};
		if (sensor_attr_set(press_sensor, SENSOR_CHAN_ALL,
					SENSOR_ATTR_SAMPLING_FREQUENCY, &attr) < 0) {
			printf("Cannot configure sampling rate\n");
		}
	}

#ifdef CONFIG_WIFI
	net_mgmt_init_event_callback(&ble_demo_mgmt_cb,
			ble_sample_wifi_mgmt_event_handler,
			NET_EVENT_WIFI_STATUS_RESULT);

	net_mgmt_add_event_callback(&ble_demo_mgmt_cb);
#endif
	setup_buttons();

	return 0;
}

bool fetch_temperature(struct sensor_value *temp)
{
	if (temp_sensor && temp) {
		if (sensor_sample_fetch(temp_sensor) >= 0) {
			sensor_channel_get(temp_sensor, SENSOR_CHAN_AMBIENT_TEMP, temp);
		}
		return true;
	}
	return false;
}

/* acc_sensor is a pointer to a 3 element array of sensor_value struct  */
int read_accelerometer(SENSOR_VALUE_STRUCT* acc_sensor_val_arr)
{
	int rc = 0;
	if (acc_sensor != NULL) {
		rc = sensor_sample_fetch(acc_sensor);

		if (rc == 0) {
			rc = sensor_channel_get(acc_sensor,
					SENSOR_CHAN_ACCEL_XYZ,
					acc_sensor_val_arr);
		}
	} else {
		return -EINVAL;
	}
	return rc;
}

bool fetch_light(struct sensor_value *light)
{
	if (light_sensor && light) {
		if (sensor_sample_fetch(light_sensor) >= 0) {
			sensor_channel_get(light_sensor, SENSOR_CHAN_LIGHT, light);
		}
		return true;
	}
	return false;
}

bool fetch_ir(struct sensor_value *ir_value)
{
	if (ir_sensor && ir_value) {
		if (sensor_sample_fetch(ir_sensor) >= 0) {
			sensor_channel_get(ir_sensor, SENSOR_CHAN_IR, ir_value);
		}
		return true;
	}
	return false;
}

bool fetch_pressure(struct sensor_value *press)
{
	if (press_sensor && press) {
		if (sensor_sample_fetch(press_sensor) >= 0) {
			sensor_channel_get(press_sensor, SENSOR_CHAN_PRESS, press);
		}
		return true;
	}
	return false;
}
SYS_INIT(tmo_ble_demo_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);


int cmd_ble_adv_conn(const struct shell *shell, size_t argc, char **argv)
{
	int err;
	bt_le_adv_stop();
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		shell_error(shell, "Advertising failed to start (err %d)\n", err);
		return -EIO;
	}

	shell_print(shell, "Advertising successfully started\n");
	return 0;
}

int cmd_ble_adv_ebeacon(const struct shell *shell, size_t argc, char **argv)
{
	int err;
	bt_le_adv_stop();
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ebeacon_ad, ARRAY_SIZE(ebeacon_ad), NULL, 0);
	if (err) {
		shell_error(shell, "Advertising failed to start (err %d)\n", err);
		return -EIO;
	}

	shell_print(shell, "Advertising successfully started\n");
	return 0;
}

static char hex_byte_to_data(const char * hex_byte)
{
	char out;

	if (isalpha(hex_byte[0])) {
		out = (10 + (toupper(hex_byte[0]) & ~0x20) - 'A') << 4;
	} else {
		out = (hex_byte[0] - '0') << 4;
	}
	if (isalpha(hex_byte[1])) {
		out += (10 + (toupper(hex_byte[1]) & ~0x20) - 'A');
	} else {
		out += (hex_byte[1] - '0');
	}
	return out;
}

static size_t hex_str_to_data(const char *input_buf, uint8_t *output_buf, size_t output_len)
{
	size_t str_len = strlen(input_buf);
	size_t i = 0;

	for (i = 0; (i < output_len) && (i * 2 < str_len); i++) {
		output_buf[i] = hex_byte_to_data(&input_buf[i * 2]);
	}
	return i;
}

int cmd_ble_adv_ibeacon(const struct shell *shell, size_t argc, char **argv)
{
	int err;
	bt_le_adv_stop();
	if (argc < 2) {
		err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ibeacon_ad, 
			  ARRAY_SIZE(ibeacon_ad), NULL, 0);
		goto end;
	}

	memcpy(ibeacon_custom_data, ibeacon_ad[1].data, sizeof(ibeacon_custom_data));

	if (argc > 5) {
		//Usage
		shell_help(shell);
		return -EINVAL;
	}

	uint8_t *uuid = &ibeacon_custom_data[4];
	uint8_t *major = &ibeacon_custom_data[20];
	uint8_t *minor = &ibeacon_custom_data[22];
	int8_t *rssi_at_1m = (int8_t *)&ibeacon_custom_data[24];

	if (argc >= 2) {
		errno = 0;
		char *group = argv[1];
		for (int i = 0; i < 5; i++) {
			if (group == NULL) {
				shell_help(shell);
				return -EINVAL;
			}
			switch (i) {
			case 0:
				if (hex_str_to_data(group, uuid, 4) != 4){
					shell_help(shell);
					return -EINVAL;
				}
				uuid += 4;
				break;
			case 1:
			case 2:
			case 3:
				if (hex_str_to_data(group, uuid, 2) != 2){
					shell_help(shell);
					return -EINVAL;
				}
				uuid += 2;
				break;
			case 4:
				if (hex_str_to_data(group, uuid, 6) != 6){
					shell_help(shell);
					return -EINVAL;
				}
				break;
			}
			group = strchr(group, '-');
			group = group ? group + 1 : NULL;
		}
	}

	if (argc >= 3) {
		errno = 0;
		uint16_t major_val = sys_cpu_to_be16(strtol(argv[2], NULL, 10));
		if (errno) {
			shell_help(shell);
			return -EINVAL;
		}
		memcpy(major, &major_val, 2);
	} else {
		memset(major, 0, 2);
	}

	if (argc >= 4) {
		errno = 0;
		uint16_t minor_val = sys_cpu_to_be16(strtol(argv[3], NULL, 10));
		if (errno) {
			shell_help(shell);
			return -EINVAL;
		}
		memcpy(minor, &minor_val, 2);
	} else {
		memset(minor, 0, 2);
	}

	if (argc == 5) {
		errno = 0;
		*rssi_at_1m = strtol(argv[4], NULL, 10);
		if (errno) {
			shell_help(shell);
			return -EINVAL;
		}
	} else {
		*rssi_at_1m = -60;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ibeacon_custom, 
			  ARRAY_SIZE(ibeacon_custom), NULL, 0);
end:
	if (err) {
		shell_error(shell, "Advertising failed to start (err %d)\n", err);
		return -EIO;
	}

	shell_print(shell, "Advertising successfully started\n");
	return 0;
}

int cmd_ble_conn_rssi(const struct shell *shell, size_t argc, char** argv)
{
	if (!get_active_le_conns()) {
		shell_error(shell, "No active connection(s)");
		return 1;
	}
	const struct bt_conn *conn = get_acl_conn(0);
	int rssi = bt_conn_le_get_rssi(conn);
	if (rssi < 0) {
		shell_error(shell, "bt_conn_le_get_rssi returned %d", -rssi);
		return 1;
	} else {
		const bt_addr_le_t *dst_addr = bt_conn_get_dst(conn);
		char addr_str[64];
		bt_addr_le_to_str(dst_addr, addr_str, sizeof(addr_str));
		shell_print(shell, "Connection %s: RSSI = %d", addr_str, -rssi);
	}
	return 0;
}
