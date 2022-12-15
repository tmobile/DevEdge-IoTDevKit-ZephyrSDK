/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

int misc_test()
{
	int ret = 0;

#if DT_NODE_EXISTS(DT_NODELABEL(as6212))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get TEMP_0")) != 0)
	{
		printf("sensor get TEMP_0 - as6212 temp sensor - shell command failed %d \n", ret);
	} else {
		printf("sensor get TEMP_0 - as6212 temp sensor - shell command was successful\n");
	}
#else
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get TEMP_0")) != 0)
	{
		printf("sensor get TEMP_0 - tmp108 temp sensor - shell command failed %d \n", ret);
	} else {
		printf("sensor get TEMP_0 - tmp108 temp sensor - shell command was successful\n");
	}
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(bq24250))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c read_byte I2C_1 0x6a 0x3")) != 0)
	{
		printf("i2c read_byte I2C_1 0x6a 0x3 - BQ24250 voltage regulator- shell command failed %d \n", ret);
	} else {
		printf("i2c read_byte I2C_1 0x6a 0x3 - BQ24250 voltage regulator- shell command was successful\n");
	}
#else
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c read I2C_1 25 10")) != 0)
	{
		printf("i2c read I2C_1 25 10 - ACT81460 PMIC - shell command failed %d \n", ret);
	} else {
		printf("i2c read I2C_1 25 10 - ACT81460 PMIC - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c write_byte I2C_1 25 47 98")) != 0)
	{
		printf("i2c write_byte I2C_1 25 47 98 - ACT81460 PMIC turn on GNSS - shell command failed %d \n", ret);
	} else {
		printf("i2c write_byte I2C_1 25 47 98 - ACT81460 PMIC turn on GNSS - shell command was successful\n");
	}
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(sonycxd5605))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c read I2C_1 24 10")) != 0)
	{
		printf("i2c read I2C_1 24 10 - Sony cxd5605 GNSS - shell command failed %d \n", ret);
	} else {
		printf("i2c read I2C_1 24 10 - Sony cxd5605 GNSS - shell command was successful\n");
	}
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(tsl2540))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get TSL2540")) != 0)
	{
		printf("sensor get TSL2540 - tsl25403 ambient light sensor - shell command failed %d \n", ret);
	} else {
		printf("sensor get TSL2540 - tsl25403 ambient light sensor - shell command was successful\n");
	}
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(murata_1sc))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "tmo modem 1 imei")) != 0)
	{
		printf("tmo modem 1 imei - murata 1sc- shell command failed %d \n", ret);
	} else {
		printf("tmo modem 1 imei - murata 1sc - shell command was successful\n");
	}
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(rs9116w))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "tmo wifi status 2")) != 0)
	{
		printf("tmo wifi status 2 - rs9116w- shell command failed %d \n", ret);
	} else {
		printf("tmo wifi status 2 - rs9116w - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "tmo wifi scan 2")) != 0)
	{
		printf("tmo wifi scan 2 - rs9116w- shell command failed %d \n", ret);
	} else {
		printf("tmo wifi scan 2 - rs9116w - shell command was successful\n");
	}
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(lis2dw12))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get LIS2DW12")) != 0)
	{
		printf("sensor get LIS2DW12 - accelerometer - shell command failed %d \n", ret);
	} else {
		printf("sensor get LIS2DW12 - accelerometer - shell command was successful\n");
	}
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(lps22hh))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get LPS22HH")) != 0)
	{
		printf("sensor get LPS22HH - pressure sensor - shell command failed %d \n", ret);
	} else {
		printf("sensor get LPS22HH - pressure sensor - shell command was successful\n");
	}
#endif
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "fs write /tmo/access_id.txt 30 31 32 33 0a 0d")) != 0)
	{
		printf("fs write /tmo/access_id.txt 30 31 32 33 0a 0d - SPI nor flash - shell command failed %d \n", ret);
	} else {
		printf("fs write /tmo/access_id.txt 30 31 32 33 0a 0d - SPI nor flash - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "fs read /tmo/access_id.txt")) != 0)
	{
		printf("fs read /tmo/access_id.txt - SPI nor flash - shell command failed %d \n", ret);
	} else {
		printf("fs read /tmo/access_id.txt - SPI nor flash - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "fs rm /tmo/access_id.txt")) != 0)
	{
		printf("fs fs rm /tmo/access_id.txt - SPI nor flash - shell command failed %d \n", ret);
	} else {
		printf("fs rm /tmo/access_id.txt - SPI nor flash - shell command was successful\n");
	}
	if (strcmp(CONFIG_BOARD, "tmo_dev_edge") == 0) {
		if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c scan I2C_0")) != 0)
		{
			printf("i2c scan I2C_0 - shell command failed %d \n", ret);
		} else {
			printf("i2c scan I2C_0 - shell command was successful\n");
		}
	}
	return (ret);
}
