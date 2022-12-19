/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include "tmo_bq24250.h"

#if DT_NODE_HAS_STATUS(DT_NODELABEL(bq24250), okay)

#define BQ_GPIO_F_NAME               "GPIO_F"
#define BQ_CHARGE_INT                12      /* PF12 - configured here */
#define BQ_CHARGE_CEn                13      /* PF13 - set low in board.c */
#define BQ_ADDRESS                   0x6a

#define ENABLE_HARDWARE              90
#define BQ_INIT                      91
#define BQ_INT_MONITOR               92

#define MAX_BYTES_FOR_REGISTER_INDEX 4
#define MAX_I2C_BYTES                20

#define BQ_CHARGER_FAULT_MODE        3
#define BQ_SLEEP_MODE                3
#define INPUT_FAULT_AND_LDO_LOW_MODE 10

struct drv_data {
	struct gpio_callback gpio_int_cb;
	gpio_flags_t mode;
	int index;
	int aux;
};

static bool restart_setup = false;

/* BQ Battery and Charger Status */
const struct device *gpiof_dev = DEVICE_DT_GET(DT_NODELABEL(gpiof));

static const struct device *i2c1_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

/* BQ Battery Charger interrupt callback */
static struct gpio_callback gpio_batt_chrg_cb;
int bq_battery_charge_int_isr_count = 0;
bool bq_battery_charge_int_state_change = false;
uint8_t i2c_read_data[20];

static int i2c_read_to_buffer(uint8_t dev_addr, int reg_addr, uint8_t *buf, uint8_t buf_length)
{
	const struct device *dev;
	uint8_t reg_addr_buf[MAX_BYTES_FOR_REGISTER_INDEX];
	int reg_addr_bytes;
	int ret;

	dev = i2c1_dev;
	if (!dev) {
		printf("I2C: Device driver I2C_1 not found."); 
		return -ENODEV;
	}

	reg_addr_bytes = 1;
	sys_put_be32(reg_addr, reg_addr_buf);

	ret = i2c_write_read (dev, dev_addr,
			reg_addr_buf +
			MAX_BYTES_FOR_REGISTER_INDEX - reg_addr_bytes,
			reg_addr_bytes, buf, buf_length);
	if (ret < 0) {
		printf("Failed to read from device: 0x%x", dev_addr);
		return -EIO;
	}
	return 0;
}

int get_bq24250_status(uint8_t *charging, uint8_t *vbus, uint8_t *attached, uint8_t *fault)
{
	int ret = 0;
	uint8_t bq_addr = 0x6a;
	uint8_t rx_len = 1;
	uint8_t bq_reg_0x00= 0x00;
	uint8_t bq_reg_0x00_data;

	ret = i2c_read_to_buffer (bq_addr, bq_reg_0x00, &bq_reg_0x00_data, rx_len);
	if (ret) {
		printf("read_bytes function:Error reading from BQ! error code (%d)\n", ret);
	}

	/* parse the BQ Charge Status or Fault Interrupt that got us here. */
	uint8_t charger_status = ((bq_reg_0x00_data & 0x30) >> 4);
	uint8_t charge_fault_status = (bq_reg_0x00_data & 0x0F);

	*vbus = 0;
	*charging = 0;
	*attached = 0;
	*fault = 0;

	if ((charger_status == 3) && (charge_fault_status > 0 )) {
		*vbus = 0;
		*charging = 0;
		if (charge_fault_status == 8) {
			*attached = 0;
		}
		else {
			*attached = 1;
			*fault = 1;
		}
	}
	else {
		if ((charger_status == 1) && (charge_fault_status == 0)) {
			*vbus = 1;
			*charging = 1;
			*attached = 1;
		}
		else {
			*vbus = 1;
			*charging = 0;
			*attached = 1;
		}
	}
	return ret;
}

static int check_battery_charger_regs(void)
{
	int ret = 0;

	static bool no_vbus = true;

	uint8_t bq_addr = 0x6a;
	uint8_t rx_len = 1;

	uint8_t bq_reg_0x00= 0x00;
	uint8_t bq_reg_0x00_data = 0;

	uint8_t bq_reg_0x01= 0x01;
	uint8_t bq_reg_0x01_data = 0;

	ret = i2c_read_to_buffer (bq_addr, bq_reg_0x00, &bq_reg_0x00_data, rx_len);
	if (ret) {
		printf("i2c read_bytes function:Error reading from BQ! error code (%d)\n", ret);
	}

	ret = i2c_read_to_buffer (bq_addr, bq_reg_0x01, &bq_reg_0x01_data, rx_len);
	if (ret) {
		printf("i2c read_bytes function:Error reading from BQ! error code (%d)\n", ret);
	}

	k_msleep(100);

	ret = i2c_read_to_buffer (bq_addr, bq_reg_0x00, &bq_reg_0x00_data, rx_len);
	if (ret) {
		printf("i2c read_bytes function:Error reading from BQ! error code (%d)\n", ret);
	}

	ret = i2c_read_to_buffer (bq_addr, bq_reg_0x01, &bq_reg_0x01_data, rx_len);
	if (ret) {
		printf("i2c read_bytes function:Error reading from BQ! error code (%d)\n", ret);
	}

	printf("\nReceived a Charge Status or Fault Interrupt callback\n");

	/* parse the BQ Charge Status or Fault Interrupt that got us here. */
	uint8_t charger_status = ((bq_reg_0x00_data & 0x30) >> 4);
	uint8_t charge_fault_status = (bq_reg_0x00_data & 0x0F);

	if ((charger_status == BQ_CHARGER_FAULT_MODE) && 
			((charge_fault_status == BQ_SLEEP_MODE) || (charge_fault_status == INPUT_FAULT_AND_LDO_LOW_MODE)))
	{
		if (!no_vbus) { 
			printf("\tCharger VBUS has just been removed!\n");
		}
		else {
			printf("\tCharger VBUS is missing!\n");
		}
		no_vbus = true;
	}
	else
	{
		no_vbus = false;
		switch(charger_status)
		{
			case 0:
				printf("\tCharger is ready to charge!\n");
				break;
			case 1:
				printf("\tCharger is currently charging!\n");
				break;
			case 2:
				printf("\tCharger is done charging!\n");
				break;
			case 3:
				printf("\tCharger is in Fault mode!\n");
				break;
			default:
				break;
		}

		switch(charge_fault_status) 
		{
			case 0:
				printf("\tCharger is in normal mode!\n");
				break;
			case 1:
				printf("\tCharger is in Input OVP mode!\n");
				break;
			case 2:
				printf("\tCharger is in Input UVLO mode!\n");
				break;
			case 3:
				printf("\tCharger is in Sleep mode!\n");
				break;
			case 4:
				printf("\tBattery is in Temperature Fault mode!\n");
				break;
			case 5:
				printf("\tBattery is in OVP mode!\n");
				break;
			case 6:
				printf("\tBattery is in Thermal Shutdown mode!\n");
				break;
			case 7:
				printf("\tBattery is in Timer Fault!\n");
				break;
			case 8:
				printf("\tBattery is in No Battery Connected mode - Try reseating the connector!\n");
				break;
			case 9:
				printf("\tBattery is in ISET short mode!\n");
				break;
			case 10:
				printf("\tBattery is in Input Fault and LDO low mode!\n");
				break;
			default:
				break;
		}
	}
	return ret;
}

static void bq_intr_callback(const struct device *port,
		struct gpio_callback *cb, uint32_t pins)
{
	bq_battery_charge_int_isr_count++;
	bq_battery_charge_int_state_change = true;
}

static void bq_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	k_msleep(4000);

	printf("%s - thread started\n", __FUNCTION__);
	uint8_t current_state = BQ_INIT;
	int ret;

	memset(i2c_read_data, 0, 20);

	while (1) {
		if (restart_setup) {
			printf("%s - restart_setup\n", __FUNCTION__);
			current_state = ENABLE_HARDWARE;
			restart_setup = false;
		}

		switch (current_state) {
			case BQ_INIT:

				/* Setup and configure the interrupt for BQ_INT */

				if (!gpiof_dev) {
					printf("GPIOF driver error\n");
					return;
				}

				/* Setup GPIO input, and triggers on falling edge. */
				ret = gpio_pin_configure(gpiof_dev, BQ_CHARGE_INT, GPIO_INPUT | GPIO_INT_EDGE_BOTH);
				if (ret) {
					printf("Error configuring GPIO_F %d!\n", BQ_CHARGE_INT);
				}

				gpio_init_callback(&gpio_batt_chrg_cb, bq_intr_callback, BIT(BQ_CHARGE_INT));

				ret = gpio_add_callback(gpiof_dev, &gpio_batt_chrg_cb);
				if (ret) {
					printf("Cannot setup callback!\n");
				}

				ret = gpio_pin_interrupt_configure(gpiof_dev, BQ_CHARGE_INT, GPIO_INPUT | GPIO_INT_EDGE_BOTH);

				if (!device_is_ready(i2c1_dev)) {
					printf("I2C: device is not ready\n");
				}

				k_msleep(1000);

				bq_battery_charge_int_isr_count = 0;
				bq_battery_charge_int_state_change = false;
				memset(i2c_read_data, 0, 10);

				printf("Checking battery and charger state\n");
				check_battery_charger_regs();
				current_state = BQ_INT_MONITOR;

				break;

			case BQ_INT_MONITOR:
				if (bq_battery_charge_int_state_change) 
				{
					check_battery_charger_regs();
					bq_battery_charge_int_state_change = false;
				}
				break;
			default:
				break;
		}
		k_msleep(250);
	}
}

#define BQ_NOTIF_THREAD_STACK_SIZE 2048
#define BQ_NOTIF_THREAD_PRIORITY CONFIG_MAIN_THREAD_PRIORITY

K_THREAD_DEFINE(bq_tid, BQ_NOTIF_THREAD_STACK_SIZE,
		bq_thread, NULL, NULL, NULL,
		BQ_NOTIF_THREAD_PRIORITY, 0, 0);
#endif
