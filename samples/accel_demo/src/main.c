/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
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

#define GECKO_CONSOLE DT_LABEL(DT_CHOSEN(zephyr_console))
#define BUSY_WAIT_DURATION 2U
#define SLEEP_DURATION 2U

#define ACCEL1_GPIO_B_NAME    "GPIO_B"
#define ACCEL1_INT1                 8      /* PB08 - configured here */
#define ACCEL1_INT2                 9      /* PB09 - set low in board.c */
#define ACCEL1_ADDRESS           0x18
#define LIS2DW_ADDRESS           0x18

#define MAX_BYTES_FOR_REGISTER_INDEX 4
#define MAX_I2C_BYTES                20

struct drv_data {
        struct gpio_callback gpio_int_cb;
        gpio_flags_t mode;
        int index;
        int aux;
};

static const struct device *gpiob_dev;

/* ACCEL1 device */
static const struct device *gpiob_dev;
static const struct device *i2c1_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

/* ACCEL1 interrupt callback */
static struct gpio_callback gpio_cb;
int accel1_int1_int_isr_count = 0;
int accel1_int2_int_isr_count = 0;
bool accel1_int_state_change = false;
int8_t temperature = 0;

/* User Push Button (sw0) interrupt callback */
static struct gpio_callback gpio_cb;
int  push_button_isr_count = 0;
bool push_button_isr_state_change = false;

void user_push_button_intr_callback(const struct device *port,
                struct gpio_callback *cb, uint32_t pins)
{
        push_button_isr_count++;
        push_button_isr_state_change = true;
        printf("\nPush Button (sw0) Interrupt detected\n");
}

uint8_t i2c_read_data[20];
static int i2c_read_to_buffer( uint8_t dev_addr, int reg_addr, uint8_t *buf, uint8_t buf_length)
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

int lis2dw_reg_read16( uint8_t reg, uint16_t *val)
{
        const struct device *dev;
        dev = device_get_binding("I2C_1");
        if (!dev) {
                printf("I2C: Device driver I2C_1 not found.");
                return -ENODEV;
        }

        if (i2c_burst_read(dev, LIS2DW_ADDRESS,
                                reg, (uint8_t *) val, 2) < 0) {
                printf("I2C read failed");
                return -EIO;
        }

        // *val = sys_be16_to_cpu(*val);

        return 0;
}

/**
 * Modifies a register within the ACCEL1
 * Returns 0 on success, or errno on error
 */
static int accel1_modify_register(uint8_t reg, uint8_t reg_mask, uint8_t reg_val)
{
        const struct device *i2c_dev;
        uint8_t reg_current;
        int rc;

        i2c_dev = device_get_binding("I2C_1");
        if (!i2c_dev) {
                printf("I2C: Device driver I2C_1 not found.");
                return -ENODEV;
        }

        rc = i2c_reg_read_byte(i2c_dev, ACCEL1_ADDRESS,
                        reg, &reg_current);
        if (rc) {
                return rc;
        }

        reg_current &= ~reg_mask;
        reg_current |= reg_val;

        rc = i2c_reg_write_byte(i2c_dev, ACCEL1_ADDRESS, reg,
                        reg_current);
        if (rc) {
                return rc;
        }

        rc = i2c_reg_read_byte(i2c_dev, ACCEL1_ADDRESS,
                        reg, &reg_current);
        if (rc) {
                return rc;
        }

        printf("\tR-W-R : Reg %x = %x\n", reg, reg_current);

        return rc;
}

#define LIS2DW_REG_TEMP16 0x0d
static int lis2dw_get_temp(void)
{
        uint16_t val;
        if (lis2dw_reg_read16(LIS2DW_REG_TEMP16, &val) < 0) {
                return -EIO;
        }

        int16_t temperature = val;
        temperature = (temperature >> 4);
        // printf("LIS2DW value from reg 0x0d = 0x%04x %d\n", temperature, temperature);
        float floatTemp = (temperature * 0.0625) + 25;
        printf("\tLIS2DW Temperature in degrees C %.2f\n", floatTemp);
        return 0;
}

static void accel1_init_regs (void)
{
        uint8_t accel1_init_reg_array_num = 13;
        uint8_t accel1_init_reg_array[]       = {0x20, 0x22, 0x23, 0x24, 0x25, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x3f};
        uint8_t accel1_init_reg_value_array[] = {0x24, 0x38, 0xC8, 0x00, 0x34, 0x01, 0x01, 0xe1, 0x75, 0x81, 0x00, 0x0F, 0x20};

        for (int i=0; i < accel1_init_reg_array_num; i++)
        {
                accel1_init_reg_array[i] = accel1_init_reg_array[i];
                accel1_init_reg_value_array[i] = accel1_init_reg_value_array[i];
                accel1_modify_register( accel1_init_reg_array[i], 0xff, accel1_init_reg_value_array[i]);
        }
        printk("\n");
}

void accel1_intr_callback(const struct device *port,
                struct gpio_callback *cb, uint32_t pins)
{
        accel1_int1_int_isr_count++;
        accel1_int_state_change = true;
        //printk("\nACCEL1 INT1\n");
}

void get_wakeup_direction (void){

        int ret = 0;
        uint8_t accel1_addr = ACCEL1_ADDRESS;
        uint8_t rx_len = 1;

        uint8_t wakeup_src = 0x38;
        uint8_t wakeup_data;

        ret = i2c_read_to_buffer (accel1_addr, wakeup_src, &wakeup_data, rx_len);
        if (ret) {
                printf("read_bytes function:Error reading from ACCEL1 ! error code (%d)\n", ret);
        }
        else {

                uint8_t wakeupDetected = wakeup_data & 0x08;
                uint8_t wakeupDirX = wakeup_data & 0x04;
                uint8_t wakeupDirY = wakeup_data & 0x02;
                uint8_t wakeupDirZ = wakeup_data & 0x01;

                //Wake-up event detected
                if(wakeupDetected) {
                        printf("wake-up event happened in\n");
                        //Wake-up motion direction detection
                        if(wakeupDirX){
                                printf("\tx direction");
                        }
                        if(wakeupDirY){
                                printf("\ty direction");
                        }
                        if(wakeupDirZ){
                                printf("\tz direction");
                        }
                        k_msleep(100);
                }
        }
}

void get_status(void) {

        int ret = 0;
        uint8_t accel1_addr = ACCEL1_ADDRESS;
        uint8_t rx_len = 1;

        uint8_t status  = 0x27;
        uint8_t status_data = 0;

        ret = i2c_read_to_buffer (accel1_addr, status, &status_data, rx_len);
        if (ret) {
                printf("read_bytes function:Error reading from ACCEL1 ! error code (%d)\n", ret);
        }
        else {

		uint8_t fifo_ths = status & 0x80;
                uint8_t wakeup_ia  = status & 0x40;
                uint8_t sleep_state = status & 0x20;
                uint8_t double_tap = status & 0x10;
                uint8_t single_tap = status & 0x08;
                uint8_t pos_6D_ia = status & 0x04;
                uint8_t ff_ia = status & 0x02;
                uint8_t data_ready = status & 0x01;

		// read the status reg
                if(status) {
                        printf("\tStatus event detected %x\n", status);
                        // IA detection
			if(fifo_ths){
                                printf("\t\tFIFO ths detection status\n");
                        }
                        if(wakeup_ia){
                                printf("\t\tWakeUp ia detection status\n");
                        }
                        if(sleep_state){
                                printf("\t\tSleep ia detection status\n");
                        }
                        if(double_tap){
                                printf("\t\tdouble tap detection status\n");
                        }
                        if(single_tap){
                                printf("\t\tsingle tap detection status\n");
                        }
                        if(pos_6D_ia){
                                printf("\t\t6D ia detection status\n");
                        }
                        if(ff_ia){
                                printf("\t\tFree Fall detection status\n");
                        }
                        if(data_ready){
                                printf("\t\tData Ready detection status\n");
                        }
                }
	}
}

void get_tap_and_int_src(void) {

        int ret = 0;
        uint8_t accel1_addr = ACCEL1_ADDRESS;
        uint8_t rx_len = 3;

        uint8_t tap_src = 0x39;
        uint8_t tap_plus_interrupt_data[4] = {0};

        ret = i2c_read_to_buffer (accel1_addr, tap_src, tap_plus_interrupt_data, rx_len);
        if (ret) {
                printf("read_bytes function:Error reading from ACCEL1 ! error code (%d)\n", ret);
        }
        else {

		uint8_t tap_data = tap_plus_interrupt_data[0];
                uint8_t tap_detected = tap_data & 0x40;
                uint8_t single_tap = tap_data & 0x20;
                uint8_t double_tap = tap_data & 0x10;
                // uint8_t tap_sign = tap_data & 0x08;
		uint8_t x_tap = tap_data & 0x04;
                uint8_t y_tap = tap_data & 0x02;
                uint8_t z_tap = tap_data & 0x01;

                uint8_t all_intp_data = tap_plus_interrupt_data[2];
		uint8_t sleep_chg = all_intp_data & 0x20;
                uint8_t pos_6D_chg = all_intp_data & 0x10;
                uint8_t double_tap_intp = all_intp_data & 0x08;
                uint8_t single_tap_intp = all_intp_data & 0x04;
                uint8_t wakeup_intp = all_intp_data & 0x02;
                uint8_t ff_intp = all_intp_data & 0x01;

                // Tap event detected
                if(tap_detected) {
                        printf("\tTap event Detected %x\n", tap_detected);
			// Tap detection
                        if(single_tap){
                                printf("\t\tsingleTap detected");
                        }
                        if(double_tap){
                                printf("\t\tdoubleTap detected");
                        }
                        if(x_tap){
                                printf("\t\tx-axis detected");
                        }
                        if(y_tap){
                                printf("\t\ty-axis detected");
                        }
                        if(z_tap){
                                printf("\t\tz-axis detected");
                        }
                }

                if(all_intp_data) {
                        printf("\tInterrupt detected was %x\n", all_intp_data);
                        // Interrupt Source detection
			if(sleep_chg){
                                printf("\t\tSleep status change detected");
                        }
                        if(pos_6D_chg){
                                printf("\t\t6D Position change detected");
                        }
                        if(double_tap_intp){
                                printf("\t\tdouble tap interrupt detected");
                        }
                        if(single_tap_intp){
                                printf("\t\tsingle tap interrupt detected");
                        }
                        if(wakeup_intp){
                                printf("\t\tWake Up Interrupt detected");
                        }
                        if(ff_intp){
                                printf("\t\tFree Fall Interrupt detected");
                        }
                }
        }
}

void main(void)
{
	int ret;

	/* Wait for zephyr kernel init to complete */
	k_msleep(2000);

	printf("\n\tWelcome to the DevEdge LIS2DW Accelerometer Demo\n\n");

	gpiob_dev = device_get_binding(ACCEL1_GPIO_B_NAME);
        if (!gpiob_dev) {
                printf("GPIOB driver error\n");
        }

        /* Setup GPIO input, and triggers on falling edge. */

        ret = 0;
        ret = gpio_pin_configure(gpiob_dev,
                                 ACCEL1_INT1,
                                 GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
        if (ret) {
                printf("Error configuring GPIO_B %d!\n", ACCEL1_INT1);
        }

        gpio_init_callback(&gpio_cb, accel1_intr_callback, BIT(ACCEL1_INT1));

        ret = gpio_add_callback(gpiob_dev, &gpio_cb);
        if (ret) {
                printf("Cannot setup callback!\n");
        }

        ret = gpio_pin_interrupt_configure(gpiob_dev,
                                           ACCEL1_INT1,
                                           GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
        if (!device_is_ready(i2c1_dev)) {
                printf("I2C: device is not ready\n");
        }

        k_msleep(1000);

        accel1_int1_int_isr_count = 0;
        accel1_int_state_change = false;
        memset(i2c_read_data, 0, 10);

	printf("\n\tThe DevEdge LIS2DW Accelerometer Demo register init values are as follows:\n\n");
	accel1_init_regs();
	printf("\n\tDevEdge LIS2DW Accelerometer Demo sensor dashboard:\n");
	printf("\n\tNote 1: Move the DevEdge n the X and Y axis with enough force to generate a Single and Double Tap.\n");
	printf("\n\tNote 2: A single snap action will cause a single tap, back to back snaps generate a double tap.\n\n");
	printf("\n\tNote 3: Hit enter to flush out any console taps that may have occured in the demo init.\n\n");

	lis2dw_get_temp();

	/* TODO:
	 * printk("\nTesting Gecko EM mode Sleep Wakeup - create a motion based wake up event\n");
         * pm_state_force(0u, &(struct pm_state_info){ PM_STATE_SUSPEND_TO_IDLE, 0, 0});
         * k_sleep(K_SECONDS(SLEEP_DURATION));
	 */

	while (1)
	{ 
	    if (accel1_int_state_change)
            {
		    //get_status();
		    get_tap_and_int_src();
                    //printf("\n\t... Interrupt:  \n");
		    accel1_int_state_change = false;
            }		    

            k_msleep(1000);
        }
}
