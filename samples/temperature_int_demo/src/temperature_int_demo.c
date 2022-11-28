/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <soc.h>
#include <drivers/gpio.h>

#include <drivers/i2c.h>
#include <drivers/gpio.h>
#include <sys/byteorder.h>
#include <drivers/sensor.h>

#define AS6212_GPIO_F_NAME       "GPIO_F"
#define AS6212_INT1                   14   /* PF14 - configured here */
#define AS6212_ADDRESS              0x48

#define SLEEP_DURATION                2U

#define MAX_BYTES_FOR_REGISTER_INDEX   4
#define MAX_I2C_BYTES                 20

struct drv_data {
        struct gpio_callback gpio_int_cb;
        gpio_flags_t mode;
        int index;
        int aux;
};

/* AS6212 device */
static const struct device *gpiob_dev;
static const struct device *i2c1_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

/* AS6212 interrupt callback */
static struct gpio_callback gpio_cb;
int as6212_int1_int_isr_count = 0;
uint8_t i2c_read_data[20];

static int i2c_burst_write_as6212 (const struct device *dev,
                                   uint16_t dev_addr,
                                   uint8_t start_addr,
                                   const uint8_t *buf,
                                   uint32_t num_bytes)
{
        struct i2c_msg msg[2];

        msg[0].buf = &start_addr;
        msg[0].len = 1U;
        msg[0].flags = I2C_MSG_WRITE;

        msg[1].buf = (uint8_t *)buf;
        msg[1].len = num_bytes;
        msg[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        return i2c_transfer(dev, msg, 2, dev_addr);
}

static int i2c_write_from_buffer( uint8_t dev_addr, 
		                  uint8_t reg_addr,
				  uint8_t *data,
				  uint8_t data_length)
{
        const struct device *dev;
        int ret;

	dev = device_get_binding("I2C_1");
        if (!dev) {
                printf("I2C: Device driver I2C_1 not found.");
                return -ENODEV;
        }

	ret = i2c_burst_write_as6212(dev, AS6212_ADDRESS, reg_addr, data, data_length);
        if (ret < 0) {
                printf("Failed to read from AS6212\n"); 
                return -EIO;
        }
        return 0;
}

static int i2c_read_to_buffer( uint8_t dev_addr,
	       	               int reg_addr,
			       uint8_t *buf,
			       uint8_t buf_length)
{
        const struct device *dev;
        uint8_t reg_addr_buf[MAX_BYTES_FOR_REGISTER_INDEX];
        int reg_addr_bytes;
        int ret;

        dev = device_get_binding("I2C_1");
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

void set_as6212_temperatureLo (void)
{
        int ret = 0;
        uint8_t as6212_addr = AS6212_ADDRESS;
        uint8_t tx_len = 2;
        uint8_t as6212_RegTempLo = 0x02;
        uint8_t writedata[2];

        writedata[0] = 0x13;
        writedata[1] = 0x40;
        ret = i2c_write_from_buffer(as6212_addr, as6212_RegTempLo, writedata, tx_len );
        if (ret) {
                printf("read_bytes function:Error reading from AS6212 ! error code (%d)\n", ret);
        }
}

void set_as6212_temperatureHi (void)
{
        int ret = 0;
        uint8_t as6212_addr = AS6212_ADDRESS;
        uint8_t tx_len = 2;
        uint8_t as6212_RegTempHi = 0x03;
	uint8_t writedata[2];

	writedata[0] = 0x16;
        writedata[1] = 0x00;
	ret = i2c_write_from_buffer(as6212_addr, as6212_RegTempHi, writedata, tx_len );
        if (ret) {
                printf("read_bytes function:Error reading from AS6212 ! error code (%d)\n", ret);
        }
}

void get_as6212_temperatureConfig (void)
{
        int ret = 0;
        uint8_t as6212_addr = AS6212_ADDRESS;
        uint8_t rx_len = 2;
        uint8_t as6212_CONFIG = 0x01;

	ret = i2c_read_to_buffer (as6212_addr, as6212_CONFIG, i2c_read_data, rx_len);
        if (ret) {
                printf("read_bytes function:Error reading from AS6212 ! error code (%d)\n", ret);
        }
	else {
	        printf("\tAS6212 CONFIG reg 0x01 = (%x %x)\n", i2c_read_data[0], i2c_read_data[1]);
	}
}

void get_as6212_temperatureLo (void)
{
        int ret = 0;
        uint8_t as6212_addr = AS6212_ADDRESS;
        uint8_t rx_len = 2;

        ret = i2c_read_to_buffer (as6212_addr, 0x02, i2c_read_data, rx_len);
        if (ret) {
                printf("read_bytes function:Error reading from AS6212 ! error code (%d)\n", ret);
        }
        else{
		printf("\tAS6212 reg 0x02 = (%x %x)\n", i2c_read_data[0], i2c_read_data[1]);
                int intTempLo = ((i2c_read_data[0] << 8) + i2c_read_data[1]);
                float floatTempLo = (intTempLo * 0.0078125);
                printf("\tAS6212 Alert Temperature Lo in degrees C %.2f\n", floatTempLo);
        }
}

void get_as6212_temperatureHi (void)
{
        int ret = 0;
        uint8_t as6212_addr = AS6212_ADDRESS;
        uint8_t rx_len = 2;

        ret = i2c_read_to_buffer (as6212_addr, 0x03, i2c_read_data, rx_len);
        if (ret) {
                printf("read_bytes function:Error reading from AS6212 ! error code (%d)\n", ret);
        }
        else{
                printf("\tAS6212 reg 0x03 = (%x %x)\n", i2c_read_data[0], i2c_read_data[1]);
                int intTempHi = ((i2c_read_data[0] << 8) + i2c_read_data[1]);
                float floatTempHi = (intTempHi * 0.0078125);
                printf("\tAS6212 Alert Temperature Hi in degrees C %.2f\n", floatTempHi);
        }
}

void get_as6212_temperature (void)
{
        int ret = 0;
        uint8_t as6212_addr = AS6212_ADDRESS;
        uint8_t rx_len = 2;
        uint8_t as6212_reg_0x00= 0x00;

        ret = i2c_read_to_buffer (as6212_addr, as6212_reg_0x00, i2c_read_data, rx_len);
        if (ret) {
                printf("read_bytes function:Error reading from AS6212 ! error code (%d)\n", ret);
        }
        else{
                int intTemp = ((i2c_read_data[0] << 8) + i2c_read_data[1]);
                float floatTemp = (intTemp * 0.0078125);
                printf("\tAS2612 Temperature in degrees C %.2f\n", floatTemp);
        }
}

void as6212_intr_callback(const struct device *port,
                struct gpio_callback *cb, uint32_t pins)
{
        as6212_int1_int_isr_count++;
        printf("\nReceived AS6212 Temperature Sensor ALERT Interrupt (%d)\n", as6212_int1_int_isr_count);
	get_as6212_temperature();
}

void main (void) {

	int ret;

	k_msleep(2000);
	printf("\nThis Sample Demo App implements a Gecko Sleep-then-Wake Up,\n");
       	printf("based on the AS6212 Temperture Alert Interrupt using the %s board\n", CONFIG_BOARD);

	gpiob_dev = device_get_binding(AS6212_GPIO_F_NAME);
        if (!gpiob_dev) {
               printf("GPIOB driver error\n");
        }

        /* Setup GPIO input, and triggers on falling edge. */
        ret = gpio_pin_configure(gpiob_dev,
                                 AS6212_INT1,
                                 GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
        if (ret) {
                printf("Error configuring GPIO_B %d!\n", AS6212_INT1);
        }

        gpio_init_callback(&gpio_cb, as6212_intr_callback, BIT(AS6212_INT1));

        ret = gpio_add_callback(gpiob_dev, &gpio_cb);
        if (ret) {
                printf("Cannot setup callback!\n");
        }

        ret = gpio_pin_interrupt_configure(gpiob_dev,
                                           AS6212_INT1,
                                           GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
        if (!device_is_ready(i2c1_dev)) {
                printf("I2C: device is not ready\n");
        }

        k_msleep(1000);

        as6212_int1_int_isr_count = 0;
	memset(i2c_read_data, 0, 20);

	printf("\n1.) Checking the AS6212 Temperature Sensor regs\n");
        get_as6212_temperatureConfig ();
        get_as6212_temperatureLo();
        get_as6212_temperatureHi();

        set_as6212_temperatureLo();
        get_as6212_temperatureLo();

        set_as6212_temperatureHi();
        get_as6212_temperatureHi();

	get_as6212_temperature();

        k_msleep(5000);
	printf("\n2.) Starting Sleep Wake-up for the AS6212\n");
	printf("\n\tUse a hair dryer to force hot air over the dev edge board,\n");
	printf("\tuntil the as6212 Temperature Hi threshold (Interrupt) is generated.\n\n");

	printf("\n\tthen remove the heat source and wait until the as6212 Temperature\n");
        printf("\tlo threshold (Interrupt) gets generated from it cooling down.\n\n");

	while(true) {
	        k_sleep(K_SECONDS(1));

		/* Try EM2 mode sleep */
                pm_state_force(0u, &(struct pm_state_info){ PM_STATE_SUSPEND_TO_IDLE, 0, 0});
                /*
                 * This will let the idle thread run and let the pm subsystem run in forced state.
                 */
                k_sleep(K_SECONDS(SLEEP_DURATION));
        }

	printf("\nError: Exit  - Wake up occurred unexpectedly\n");
}
