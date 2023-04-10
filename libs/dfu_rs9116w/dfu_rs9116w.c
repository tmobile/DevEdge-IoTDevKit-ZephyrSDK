/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Device Firmware Update (DFU) support for SiLabs RS9116W
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/fs/fs.h>
#include <zephyr/drivers/gpio.h>
#include <rsi_wlan_apis.h>
#include <rsi_common_apis.h>
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

#include "dfu_rs9116w.h"
#include "dfu_common.h"

struct dfu_file_t dfu_files_rs9116w[] = {
	{"SiLabs RS9116W",
	 "/tmo/rs9116_file.rps",
	 // "RS9116W.2.4.0.36.rps",
	 // { 0x81, 0x7d, 0x22, 0x14, 0x2b, 0xcb, 0x03, 0x84, 0x29, 0x5d, 0x4f, 0xb6, 0x95, 0x3d,
	 // 0xb9, 0x2e, 0xa7, 0xbd, 0xd6, 0xb6 } "RS9116W.2.5.2.0.4.rps", { 0x4b, 0xb8, 0xf8, 0x48,
	 // 0x04, 0x36, 0xa5, 0xa8, 0xed, 0x66, 0x50, 0x30, 0x87, 0xfe, 0xe5, 0xef, 0x99, 0x0d,
	 // 0x81, 0xa2 }
	 "",
	 {0xfb, 0x3b, 0xc5, 0x71, 0xe5, 0xa3, 0x9a, 0xc6, 0x14, 0x5e,
	  0xda, 0x90, 0xd3, 0xc0, 0xff, 0xf9, 0x5b, 0x06, 0x9c, 0xe8}},
	{"", "", "", ""}};

#define RS9116_GPIO_NAME    "GPIO_A"
#define RS9116_RST_GPIO_PIN 9

/***************************************
 *  RS9116 FW Update
 */

#define RSI_CHUNK_SIZE	    4096UL
#define RSI_IN_BETWEEN_FILE 0UL
#define RSI_START_OF_FILE   1UL
#define RSI_END_OF_FILE	    2UL
#define RSI_FW_VER_SIZE	    20UL

#define RSI_SUCCESS 0

typedef enum rs9116w_app_state_e {
	RS9116W_INITIAL_STATE = 0,
	RS9116W_RADIO_INIT_STATE,
	RS9116W_FW_UPGRADE,
	RS9116W_FW_UPGRADE_DONE
} rs9116w_app_state_t;

// wlan application control block
typedef struct rs9116w_app_cb_s {
	// wlan application state
	rs9116w_app_state_t state;
} rs9116w_app_cb_t;

// application control block
rs9116w_app_cb_t rs9116w_app_cb;

// FW send variable , buffer
uint32_t chunk_cnt = 0u, chunk_check = 0u, offset = 0u, fw_image_size = 0u;
int32_t status = RSI_SUCCESS;
uint8_t recv_buffer[RSI_CHUNK_SIZE] = {0}, fw_version[RSI_FW_VER_SIZE] = {0};

// uint8_t fw_array[4096];

// extern functions
extern int16_t rsi_bl_upgrade_firmware(uint8_t *firmware_image, uint32_t fw_image_size,
				       uint8_t flags);
extern int32_t rsi_wireless_init(uint16_t opermode, uint16_t coex_mode);
extern int32_t rsi_wireless_deinit(void);

// Firmware update request structure
typedef struct fwupeq_s {
	uint16_t control_flags;
	uint16_t sha_type;
	uint32_t magic_no;
	uint32_t image_size;
	uint32_t fw_version;
	uint32_t flash_loc;
	uint32_t crc;
} fwreq_t;

#define FS_XFER_SIZE	    4096
#define RS9116_FILE_SIZE    1718768
#define RS9116_FLASH_SECTOR 0x200000
extern int read_image_from_external_flash(uint8_t *flash_read_buffer, int readBytes,
					  uint32_t flashStartSector, int ImageFileNum);
extern int32_t rsi_device_init(uint8_t select_option);
extern int32_t rsi_device_deinit(void);
extern int16_t rsi_bl_waitfor_boardready(void);

const struct device *rs_dev; /* RS9116 Gpio Device */

static struct fs_file_t rs9116file = {0};
static char *rs9116_name = "/tmo/rs9116_file.rps";
static int readbytes = 0;
static int totalreadbytes = 0;

// This function gets the size of the RS9116 firmware
static uint32_t get_rs9116_fw_size(char *buffer)
{
	fwreq_t *fw = (fwreq_t *)buffer;
	printf("\nRS9116W RPS image size = %d\n", (uint32_t)fw->image_size);
	return fw->image_size;
}

static int file_read_flash(uint32_t offset)
{
	readbytes = fs_read(&rs9116file, recv_buffer, FS_XFER_SIZE);
	if (readbytes < 0) {
		printf("Could not read file /tmo/rs9116_file.rps\n");
		return -1;
	}

	totalreadbytes += readbytes;
	return 0;
}

uint8_t fwUpgradeDone = 0;
int32_t dfu_wifi_write_image(void)
{
	rsi_device_deinit();

	int32_t err = rsi_wlan_disconnect();
	if (!err) {
		printf("RS9116 disconnect was successful %d \n", err);
	} else {
		printf("RS9116 disconnect failed! %d\n", err);
		// return -ENODEV;
	}

	rs_dev = DEVICE_DT_GET(DT_NODELABEL(gpioa));
	if (!rs_dev) {
		printf("RS9116 gpio port was not found!\n");
		return -ENODEV;
	} else {
		printf("RS9116 RESET Pin ready\n");
	}

	err = rsi_bl_waitfor_boardready();
	if (!err) {
		printf("RS9116 boardready was successful %d \n", err);
	} else {
		printf("RS9116 boardready failed! %d\n", err);
		// return -ENODEV;
	}

	int status = rsi_device_init(BURN_NWP_FW);
	if (status != RSI_SUCCESS) {
		printf("RS9116 not ready for FW update %d \n", status);
		// return (status);
	} else {
		printf("RS9116 init for FW update %d\n", status);
	}

	printf("\nChecking for /tmo/rs9116_file.rps to be present\n");
	if (fs_open(&rs9116file, rs9116_name, FS_O_READ) != 0) {
		printf("The file %s is missing - please run the sample/dfu_https_download to add "
		       "it\n",
		       "/tmo/rs9116_file.rps");
		return 1;
	} else {
		printf("The required file %s is present\n", "/tmo/rs9116_file.rps");
	}

	readbytes = 0;
	totalreadbytes = 0;

	while (!fwUpgradeDone) {

		switch (rs9116w_app_cb.state) {
		case RS9116W_INITIAL_STATE: {
			printf("\nRS9116W FW update started\n");
			/* update wlan application state */
			rs9116w_app_cb.state = RS9116W_FW_UPGRADE;
		}

			/* no break */

		case RS9116W_FW_UPGRADE: {
			if (file_read_flash(offset) != 0) {
				printf("file system flash read failed\n");
				return (-1);
			}

			/* Send the first chunk to extract header */
			fw_image_size = get_rs9116_fw_size((char *)recv_buffer);

			/* Calculate the total number of chunks */
			chunk_check = (fw_image_size / RSI_CHUNK_SIZE);
			if (fw_image_size % RSI_CHUNK_SIZE) {
				chunk_check += 1;
			}

			/* Loop until all the chunks are read and written */
			while (offset <= fw_image_size) {
				if (chunk_cnt == chunk_check) {
					/* printf("RSs9116 FW upgrade started\n"); */
					// break;
				}
				if (chunk_cnt != 0) {
					if (file_read_flash(offset) != 0) {
						printf("file system flash read failed\n");
						return (-1);
					}
					// printf("chunk_cnt: %d\n", chunk_cnt);
				}
				if (chunk_cnt == 0) {
					printf("RS9116W FW update - starts here with - 1st "
					       "Chunk\n");
					status = rsi_bl_upgrade_firmware((uint8_t *)recv_buffer,
									 RSI_CHUNK_SIZE,
									 RSI_START_OF_FILE);
					if (status != RSI_SUCCESS) {
						printf("1st Chunk RSI_ERROR: %d\n", status);
						return (-1);
					}
					printk(".");
				} else if (chunk_cnt == (chunk_check - 1)) {
					printf("\nplease wait for 2 minutes, finalizing with last "
					       "chunk\n");
					status = rsi_bl_upgrade_firmware((uint8_t *)recv_buffer,
									 RSI_CHUNK_SIZE,
									 RSI_END_OF_FILE);
					if (status != RSI_SUCCESS) {
						printf("last Chunk RSI_ERROR: %d\n", status);
						break;
					}
					printf("\r\nRS9116W FW update success\n");
					rs9116w_app_cb.state = RS9116W_FW_UPGRADE_DONE;
				} else {
					printk(".");
					// printf("\nRS9116W FW update - continues with in-between
					// Chunks\n");
					status = rsi_bl_upgrade_firmware((uint8_t *)recv_buffer,
									 RSI_CHUNK_SIZE,
									 RSI_IN_BETWEEN_FILE);
					if (status != RSI_SUCCESS) {
						printf("in-between Chunks RSI_ERROR: %d\n", status);
						break;
					}
				}
				offset += RSI_CHUNK_SIZE;
				memset(recv_buffer, 0, sizeof(recv_buffer));
				chunk_cnt++;
			} /* end While Loop */
		}	  /* End case of  */
		break;

		case RS9116W_FW_UPGRADE_DONE: {
			fwUpgradeDone = 1;
			printf("total bytes read       = %d bytes\n", totalreadbytes);
			printf("Sleeping for 40 seconds to finish the update\n");
			k_sleep(K_SECONDS(40));
			printf("RS9116W FW update was successful - rebooting now\n");
			k_sleep(K_SECONDS(2));
			sys_reboot(SYS_REBOOT_COLD);
		} break;

		default:
			printf("\nerror: dfu_rsi_write_image: default case\n");
			break;
		} /* end of switch */
	}
	return status;
} /* end of routine */

int dfu_wifi_get_version(char *wifi_fw_version)
{
	memset(wifi_fw_version, 0, DFU_RS9116W_FW_VER_SIZE);
	status = rsi_wlan_get(RSI_FW_VERSION, wifi_fw_version, DFU_RS9116W_FW_VER_SIZE);
	if (status != RSI_SUCCESS) {
		printf("reading the RS9116W FW version failed\n");
		return (-1);
	} else {
		printf("The current RS9116W FW version is: %s\n", wifi_fw_version);
	}
	return (0);
}

int dfu_wifi_firmware_upgrade(void)
{
	int ret = 0;
	printf("*** Performing the Silabs RS9116W FW update ***\n");
	dfu_wifi_write_image();
	return ret;
}
