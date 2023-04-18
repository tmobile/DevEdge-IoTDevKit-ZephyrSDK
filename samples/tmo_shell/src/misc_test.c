/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include "tmo_gnss.h"

#define I2C_0 DT_NODE_FULL_NAME(DT_NODELABEL(i2c0))
#define I2C_1 DT_NODE_FULL_NAME(DT_NODELABEL(i2c1))

#if DT_NODE_EXISTS(DT_NODELABEL(as6212))
#define TEMP_0 DT_NODE_FULL_NAME(DT_NODELABEL(as6212))
#endif /* DT_NODE_EXISTS(DT_NODELABEL(as6212)) */

#if DT_NODE_EXISTS(DT_NODELABEL(tsl2540))
#define TSL2540 DT_NODE_FULL_NAME(DT_NODELABEL(tsl2540))
#endif /* DT_NODE_EXISTS(DT_NODELABEL(tsl2540)) */

#if DT_NODE_EXISTS(DT_NODELABEL(lis2dw12))
#define LIS2DW12 DT_NODE_FULL_NAME(DT_NODELABEL(lis2dw12))
#endif /* DT_NODE_EXISTS(DT_NODELABEL(lis2dw12)) */

#if DT_NODE_EXISTS(DT_NODELABEL(lps22hh))
#define LPS22HH DT_NODE_FULL_NAME(DT_NODELABEL(lps22hh))
#endif /* DT_NODE_EXISTS(DT_NODELABEL(lps22hh)) */

int misc_test()
{
	int ret = 0, rc = 0;

	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "device list")) != 0) {
		rc |= ret;
		printf("device list - shell command failed %d\n", ret);
	} else {
		printf("device list - shell command was successful\n");
	}

#if DT_NODE_EXISTS(DT_NODELABEL(as6212))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get " TEMP_0)) != 0) {
		rc |= ret;
		printf("sensor get " TEMP_0  " - as6212 temp sensor - shell command failed %d \n", ret);
	} else {
		printf("sensor get " TEMP_0  " - as6212 temp sensor - shell command was successful\n");
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(bq24250))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c read_byte " I2C_1 " 0x6a 0x3")) != 0) {
		rc |= ret;
		printf("i2c read_byte " I2C_1 " 0x6a 0x3 - BQ24250 voltage regulator- shell command failed %d \n", ret);
	} else {
		printf("i2c read_byte " I2C_1 " 0x6a 0x3 - BQ24250 voltage regulator- shell command was successful\n");
	}
#else
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c read " I2C_1 " 25 10")) != 0) {
		rc |= ret;
		printf("i2c read " I2C_1 " 25 10 - ACT81460 PMIC - shell command failed %d \n", ret);
	} else {
		printf("i2c read " I2C_1 " 25 10 - ACT81460 PMIC - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c write_byte " I2C_1 " 25 47 98")) != 0) {
		rc |= ret;
		printf("i2c write_byte " I2C_1 " 25 47 98 - ACT81460 PMIC turn on GNSS - shell command failed %d \n", ret);
	} else {
		printf("i2c write_byte " I2C_1 " 25 47 98 - ACT81460 PMIC turn on GNSS - shell command was successful\n");
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(sonycxd5605))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c read " I2C_1 " 24 10")) != 0) {
		rc |= ret;
		printf("i2c read " I2C_1 " 24 10 - Sony cxd5605 GNSS - shell command failed %d \n", ret);
	} else {
		printf("i2c read " I2C_1 " 24 10 - Sony cxd5605 GNSS - shell command was successful\n");
	}

	if ((ret = gnss_version()) != 0) {
		printf("tmo gnssversion failed %d\n", ret);
		rc |= ret;
	} else {
		printf("tmo gnssversion passed\n");
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(tsl2540))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get " TSL2540)) != 0) {
		rc |= ret;
		printf("sensor get " TSL2540 " - tsl25403 ambient light sensor - shell command failed %d \n", ret);
	} else {
		printf("sensor get " TSL2540 " - tsl25403 ambient light sensor - shell command was successful\n");
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(murata_1sc))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "tmo modem 1 imei")) != 0) {
		rc |= ret;
		printf("tmo modem 1 imei - murata 1sc- shell command failed %d \n", ret);
	} else {
		printf("tmo modem 1 imei - murata 1sc - shell command was successful\n");
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(rs9116w))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "tmo wifi status 2")) != 0) {
		rc |= ret;
		printf("tmo wifi status 2 - rs9116w- shell command failed %d \n", ret);
	} else {
		printf("tmo wifi status 2 - rs9116w - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "tmo wifi scan 2")) != 0)
	{
		rc |= ret;
		printf("tmo wifi scan 2 - rs9116w- shell command failed %d \n", ret);
	} else {
		printf("tmo wifi scan 2 - rs9116w - shell command was successful\n");
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(lis2dw12))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get " LIS2DW12)) != 0) {
		rc |= ret;
		printf("sensor get " LIS2DW12 " - accelerometer - shell command failed %d \n", ret);
	} else {
		printf("sensor get " LIS2DW12 " - accelerometer - shell command was successful\n");
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(lps22hh))
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "sensor get " LPS22HH)) != 0) {
		rc |= ret;
		printf("sensor get " LPS22HH " - pressure sensor - shell command failed %d \n", ret);
	} else {
		printf("sensor get " LPS22HH " - pressure sensor - shell command was successful\n");
	}
#endif

	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "fs write /tmo/access_id.txt 30 31 32 33 0a 0d")) != 0) {
		rc |= ret;
		printf("fs write /tmo/access_id.txt 30 31 32 33 0a 0d - SPI nor flash - shell command failed %d \n", ret);
	} else {
		printf("fs write /tmo/access_id.txt 30 31 32 33 0a 0d - SPI nor flash - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "fs read /tmo/access_id.txt")) != 0) {
		rc |= ret;
		printf("fs read /tmo/access_id.txt - SPI nor flash - shell command failed %d \n", ret);
	} else {
		printf("fs read /tmo/access_id.txt - SPI nor flash - shell command was successful\n");
	}
	if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "fs rm /tmo/access_id.txt")) != 0) {
		rc |= ret;
		printf("fs fs rm /tmo/access_id.txt - SPI nor flash - shell command failed %d \n", ret);
	} else {
		printf("fs rm /tmo/access_id.txt - SPI nor flash - shell command was successful\n");
	}

	if (strcmp(CONFIG_BOARD, "tmo_dev_edge") == 0) {
		/* Check for presence of On Semi LC709204F fuel gauge
		 * (battery must be present for this to pass)
		 */
		if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c read " I2C_1 " 0xb 0x15 2")) != 0) {
			rc |= ret;
			printf("i2c read " I2C_1 "0xb 0x15 2 - LC709204F fuel gauge - shell command failed %d \n", ret);
		} else {
			printf("i2c read " I2C_1 "0xb 0x15 2 - LC709204F fuel gauge - shell command was successful\n");
		}

		/* Scan the secondary I2C bus (I2C_0) */
		if ((ret = shell_execute_cmd(shell_backend_uart_get_ptr(), "i2c scan " I2C_0)) != 0) {
			rc |= ret;
			printf("i2c scan " I2C_0 " - shell command failed %d \n", ret);
		} else {
			printf("i2c scan " I2C_0 " - shell command was successful\n");
		}
	}
	return rc;
}

#include "dfu_rs9116w.h"

/* These bindings are defined both by zephyr and the RS9116W, so to eliminate 
 * warnings, the definitions need to be removed.
 */
#undef AF_INET
#undef AF_INET6
#undef AF_UNSPEC
#undef PF_INET
#undef PF_INET6
#undef TCP_NODELAY
#undef IP_TOS
#undef IPPROTO_IP
#undef IPPROTO_TCP
#undef IPPROTO_UDP
#undef IPPROTO_RAW
#undef SOCK_STREAM
#undef SOCK_DGRAM
#undef SOCK_RAW
#undef htons
#undef htonl
#undef ntohs
#undef ntohl
#undef s6_addr
#undef s6_addr32
#include "dfu_murata_1sc.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/cxd5605.h>

/* Unfortunately, this has to be done again */
#undef AF_INET
#undef AF_INET6
#undef AF_UNSPEC
#undef PF_INET
#undef PF_INET6
#undef TCP_NODELAY
#undef IP_TOS
#undef IPPROTO_IP
#undef IPPROTO_TCP
#undef IPPROTO_UDP
#undef IPPROTO_RAW
#undef SOCK_STREAM
#undef SOCK_DGRAM
#undef SOCK_RAW
#undef htons
#undef htonl
#undef ntohs
#undef ntohl
#undef s6_addr
#undef s6_addr32
#include <rsi_driver.h>
extern const struct device *cxd5605;

int fw_test()
{
	char buf[MAX(DFU_RS9116W_FW_VER_SIZE, DFU_MODEM_FW_VER_SIZE)];
	int major, minor, rev, rc;
	char *v_ptr;

	dfu_modem_get_version(buf);
	v_ptr = strchr(buf, '_') + 1;
	
	major = strtol(v_ptr, NULL, 10);
	v_ptr = strchr(v_ptr, '_') + 1;
	minor = strtol(v_ptr, NULL, 10);
	v_ptr = strchr(v_ptr, '_') + 1;
	v_ptr = strchr(v_ptr, '_') + 1;
	v_ptr = strchr(v_ptr, '_') + 1;
	rev = strtol(v_ptr, NULL, 10);

	if (major < 3 || minor < 2 || rev < 20161) {
		printk("Murata firmware < RK_03_02_00_00_20161\n");
		return 1;
	}
	if (rsi_driver_cb->common_cb->state < RSI_COMMON_CARDREADY) {
		printk("RS9116 not in ready state!\n");
		return 2;
	}
	dfu_wifi_get_version(buf);
	v_ptr = strchr(buf, '.') + 1;
	
	major = strtol(v_ptr, NULL, 10);
	v_ptr = strchr(v_ptr, '.') + 1;
	minor = strtol(v_ptr, NULL, 10);
	v_ptr = strchr(v_ptr, '.') + 1;
	rev = strtol(v_ptr, NULL, 10);
	
	if (major < 2 || minor < 6) {
		printk("RS9116 firmware < 2.6\n");
		return 2;
	}

	struct sensor_value sens_values = {0,0};

	sens_values.val1 = 0;
	sens_values.val2 = 0;
	rc = sensor_attr_get(cxd5605,GNSS_CHANNEL_POSITION,GNSS_ATTRIBUTE_VER, &sens_values);
	if (rc || !sens_values.val2 || sens_values.val2 < 0x20047) {
		printk("GNSS firmware read failure\n");
		return 3;
	}

	return 0;
}

#ifdef CONFIG_TMO_TEST_MFG_CHECK_ACCESS_CODE
#include <zephyr/fs/fs.h>
#endif /* CONFIG_TMO_TEST_MFG_CHECK_ACCESS_CODE */

int ac_test()
{
#ifdef CONFIG_TMO_TEST_MFG_CHECK_ACCESS_CODE
	int ret, bytes_read;
	char tmp_buf[64];
	struct fs_file_t zfp = {0};

	fs_file_t_init(&zfp);
	ret = fs_open(&zfp, "/tmo/aws_session.txt", FS_O_READ);
	if (ret) {
		printk("Failed to read /tmo/aws_session.txt\n");
		return 1;
	}
	bytes_read = fs_read(&zfp, tmp_buf, sizeof(tmp_buf));
	if (bytes_read < 8) {
		printk("Invalid access code\n");
		return 2;
	}

	fs_close(&zfp);
#endif /* CONFIG_TMO_TEST_MFG_CHECK_ACCESS_CODE */
	return 0;
}
