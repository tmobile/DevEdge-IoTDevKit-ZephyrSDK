/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/cxd5605.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor/gnss.h>
#include <zephyr/sys/byteorder.h>

#define TMO_GNSS
#include "tmo_gnss.h"

//#define DEBUG

static uint16_t ln_las_flags = 0;
static bool startGetFixTimer = false;
static bool gotFix = false;
static uint32_t fix_time_in_seconds = 0;

void callback_1pps() {
	gnss_values.pps1Count++;
}

uint32_t cold_start_time;

void ln_buf_gen(void)
{
	int32_t latitude = 0;
	int32_t longitude = 0;
	int32_t elevation = 0;
	uint8_t hdop = 0;
	uint8_t beacons = 0;
	uint16_t ttff = 0;
	uint16_t quality_flags = 0;
	bool fix_ok = gnss_values.fix_valid;
	latitude = sys_cpu_to_le32((int)(gnss_values.lat * 10000000.0));
	longitude = sys_cpu_to_le32((int)(gnss_values.lon * 10000000.0));
	elevation = sys_cpu_to_le32((int32_t)(gnss_values.alt * 100));
	/* Clear flags */
	ln_las_flags = 0;
	if (fix_ok) {
		ln_las_flags = BIT(2) | BIT(3) | BIT(13);
		ln_las_flags |= 0b01 << 7;
	}
	memcpy(ln_las_buf, &ln_las_flags, 2);
	memcpy(ln_las_buf + 2, &latitude, 4);
	memcpy(ln_las_buf + 6, &longitude, 4);
	memcpy(ln_las_buf + 10, &elevation, 3);

	if (fix_ok){
		quality_flags = BIT(0) | BIT(2) | BIT(5) | (0b01<<7);
		beacons = gnss_values.sats;
		hdop = (uint8_t) gnss_values.hdop * 5;
		ttff = gnss_values.timeToFix * 10;
		memcpy(ln_quality_buf, &quality_flags, 2);
		ln_quality_buf[2] = beacons;
		memcpy(ln_quality_buf + 3, &ttff, 2);
		ln_quality_buf[5] = hdop;
	} else {
		memset(ln_quality_buf, 0, sizeof(ln_quality_buf));
	}

}

/***Firmware uploading state machines***/
#define ENABLE_HARDWARE 0
#define CHECK_LOCATION 1

int gnss_version(void)
{
	int rc = 0;
	struct sensor_value sens_values = {0,0};
	uint32_t major;
	uint32_t minor;
	uint32_t patch;

	sens_values.val1 = 0;
	sens_values.val2 = 0;
	rc |= sensor_attr_get(cxd5605,GNSS_CHANNEL_POSITION,GNSS_ATTRIBUTE_VER, &sens_values);
	major = sens_values.val2;
	sens_values.val1 = 1;
	sens_values.val2 = 0;
	rc |= sensor_attr_get(cxd5605,GNSS_CHANNEL_POSITION,GNSS_ATTRIBUTE_VER, &sens_values);
	minor = sens_values.val2;
	sens_values.val1 = 2;
	sens_values.val2 = 0;
	rc |= sensor_attr_get(cxd5605,GNSS_CHANNEL_POSITION,GNSS_ATTRIBUTE_VER, &sens_values);
	patch = sens_values.val2;

	if (rc) {
		printf("No GNSS chip FW detected\n");
	} else {
		printf("GNSS chip FW version: 0x%X.0x%X.0x%X\n", major, minor, patch);
		if (major == 0 || minor == 0 || patch == 0) {
			printf("GNSS chip FW not loaded\n");
			rc = -1;
		}
	}
	return rc;
}

static uint8_t current_state = ENABLE_HARDWARE;

void gnss_enable_hardware(void)
{
	/* wait 2 seconds for GNSS to boot*/
	k_msleep(2000);
	current_state = ENABLE_HARDWARE;
}

void gnss_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

#ifdef DEBUG_PRINT
	printf("%s:%d - thread restarted\n", __FUNCTION__, __LINE__);
#endif
	int rc;
	struct sensor_value sens_values = {0,0};
	struct sensor_value temp_flags;
	int32_t integral;
	int32_t frac;
	double temp;

	current_state = ENABLE_HARDWARE;

	//printf("Starting cxd5605 thread\n");

	cxd5605 = DEVICE_DT_GET(DT_NODELABEL(sonycxd5605));

	if (!cxd5605) {
		printf("cxd5605 driver error\n");
		return;
	}


	while (1) {
#ifdef DEBUG_PRINT
		printf("current_state = %d\n", current_state);
#endif
		if (restart_setup) {
#ifdef DEBUG_PRINT
			printf("%s:%d - restart_setup\n", __FUNCTION__, __LINE__);
#endif
			current_state = ENABLE_HARDWARE;
			restart_setup = false;
		}

		switch (current_state) {
		case ENABLE_HARDWARE:
			rc = sensor_attr_set(cxd5605,SENSOR_CHAN_ALL,SENSOR_ATTR_CXD5605_CALLBACK, &sens_values);
			sens_values.val1 = 1;
			sens_values.val2 = (int32_t)callback_1pps;
			rc = sensor_attr_set(cxd5605,SENSOR_CHAN_ALL,SENSOR_ATTR_CXD5605_PULSE, &sens_values);

			/* wait 2 seconds for GNSS to boot*/
			k_msleep(2000);
			sens_values.val1 = 1;
			sens_values.val2 = 0;
			rc = sensor_attr_set(cxd5605,SENSOR_CHAN_ALL,SENSOR_ATTR_CXD5605_VERSION, &sens_values);
			k_msleep(250);
			struct sensor_value gsop[3] = {{1,3000},{0,0}};
			rc = sensor_attr_set(cxd5605,SENSOR_CHAN_ALL,SENSOR_ATTR_CXD5605_OPERATING_MODE, gsop);
			k_msleep(250);
			sens_values.val1 = 0x01;
			rc = sensor_attr_set(cxd5605,SENSOR_CHAN_ALL,SENSOR_ATTR_CXD5605_SENTENCE_SELECT, &sens_values);
			k_msleep(250);
			rc = sensor_attr_set(cxd5605,SENSOR_CHAN_ALL,SENSOR_ATTR_CXD5605_COLD_START, &sens_values);
			startGetFixTimer = true;
			fix_time_in_seconds = 0;
			cold_start_time = k_uptime_get_32();
			k_msleep(250);
			rc = sensor_attr_set(cxd5605,SENSOR_CHAN_ALL,SENSOR_ATTR_CXD5605_WAKE_UP, &sens_values);
			k_msleep(250);

			current_state = CHECK_LOCATION;
			gnss_values.pps1Count = 0;
			break;

		case CHECK_LOCATION: 
			sensor_attr_get(cxd5605,
					GNSS_CHANNEL_POSITION,
					GNSS_ATTRIBUTE_LATITUDE,
					&temp_flags);
			integral = temp_flags.val1/10000000;
			frac = temp_flags.val1-(integral*10000000);
			frac = (frac / 60.0) * 100.0;
			frac = (frac < 0) ? (frac * -1) : frac;
			gnss_values.lat = (double)integral + ((double)frac/10000000.0);
			sensor_attr_get(cxd5605,
					GNSS_CHANNEL_POSITION,
					GNSS_ATTRIBUTE_LONGITUDE,
					&temp_flags);
			integral = (int)temp_flags.val1/10000000;
			frac = temp_flags.val1-(integral*10000000);
			frac = (frac / 60.0) * 100.0;
			gnss_values.lon = (double)integral + ((double)frac/10000000.0);
			sensor_attr_get(cxd5605,
					GNSS_CHANNEL_POSITION,
					GNSS_ATTRIBUTE_HDOP,
					&temp_flags);
					temp = (double)temp_flags.val2;
			gnss_values.hdop = (double)temp_flags.val1 + ((double)temp_flags.val2/10);
			sensor_attr_get(cxd5605,
					GNSS_CHANNEL_POSITION,
					GNSS_ATTRIBUTE_SIV,
					&temp_flags);
			gnss_values.sats = temp_flags.val1;
			sensor_attr_get(cxd5605,
					GNSS_CHANNEL_POSITION,
					GNSS_ATTRIBUTE_VER,
					&temp_flags);
			strcpy(gnss_values.version,(char *)temp_flags.val1);
			sensor_attr_get(cxd5605,
						GNSS_CHANNEL_POSITION,
						GNSS_ATTRIBUTE_FIXTYPE,
						&temp_flags);
			gnss_values.fix_valid = (temp_flags.val1 > 0);
			sensor_attr_get(cxd5605,
					GNSS_CHANNEL_POSITION,
					GNSS_ATTRIBUTE_ALTITUDE_MSL,
					&temp_flags);
			while (temp_flags.val2 > 0) temp_flags.val2 /= 10.0;
			gnss_values.alt = (double)temp_flags.val1 + (double)temp_flags.val2;
			
			if (startGetFixTimer)
			{
				startGetFixTimer = false;
				gotFix = false;
			}
			if (!gotFix)
			{
				sensor_attr_get(cxd5605,
						GNSS_CHANNEL_POSITION,
						GNSS_ATTRIBUTE_FIXTYPE,
						&temp_flags);
				if (temp_flags.val1 > 0)
				{
					gotFix = true;
					gnss_values.timeToFix = (k_uptime_get_32() - cold_start_time) / 1000;
					/* Based on GATT_Specification_Supplement_v6.pdf
					 * Bit 13 is unused and is set to indicate hdop present
					 * Bit 3 is set to indicate elevation present
					 */
					ln_las_flags |= BIT(2) | BIT(3) | BIT(13) | BIT(7);
				}
			}
		default:
			break;

		}
		k_msleep(250);
	}
}

#define GNSS_GPIO_DEV DEVICE_DT_GET(DT_NODELABEL(gpiof))
static const struct device *cxd5605_dev;
int cxd5605_init(void)
{
	cxd5605_dev = GNSS_GPIO_DEV;

	if (!cxd5605_dev) {
		printf("CXD5605 gpio port was not found!\n");
		return -ENODEV;
	}

	/* this starts the CXD5605 setup in tmo_gnss thread */
	restart_setup = true;

	return 0;
}

#define GNSS_NOTIF_THREAD_STACK_SIZE 2048
#define GNSS_NOTIF_THREAD_PRIORITY CONFIG_MAIN_THREAD_PRIORITY

K_THREAD_DEFINE(gnss_tid, GNSS_NOTIF_THREAD_STACK_SIZE,
		gnss_thread, NULL, NULL, NULL,
		GNSS_NOTIF_THREAD_PRIORITY, 0, 3000);

