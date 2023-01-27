/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_GECKO_H
#define DFU_GECKO_H

#define DFU_IMAGE_HDR_LEN 32
#define DFU_SLOT0_FLASH_ADDR 0x10000
#define DFU_SLOT1_FLASH_ADDR 0x80000
#define DFU_IMAGE_MAGIC 0x96f3b83d
#define DFU_DESC_LEN 64
#define DFU_FILE_LEN 64
#define DFU_SHA1_LEN 20
#define DFU_URL_LEN  256

#define CONFIG_MCU_NAME "Pearl Gecko"

#define LOG_INFO printf
#define LOG_DEBUG printf
#define LOG_ERROR printf
#define LOG_WARN printf
#define ENDL ""

struct dfu_file_t {
	char desc[DFU_DESC_LEN];
	char lfile[DFU_FILE_LEN];
	char rfile[DFU_FILE_LEN];
	char sha1[DFU_SHA1_LEN];
};

//const struct dfu_file_t *dfu_file;

int get_gecko_fw_version (void);
int get_oldest_slot();
int dfu_mcu_firmware_upgrade(int slot_to_upgrade, char *bin_file, char *sha_file);

#endif
