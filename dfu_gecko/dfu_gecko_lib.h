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

#ifndef __DFU_FILE__
#define __DFU_FILE__

#define DFU_DESC_LEN 64
#define DFU_FILE_LEN 64
#define DFU_SHA1_LEN 20
#define DFU_URL_LEN  256

struct dfu_file_t {
	char desc[DFU_DESC_LEN];
	char lfile[DFU_FILE_LEN];
	char rfile[DFU_FILE_LEN];
	char sha1[DFU_SHA1_LEN];
};
#endif /* __DFU_FILE__ */

#ifdef BOOT_SLOT
int is_bootloader_running(void);
int erase_image_slot(int slot);
int get_gecko_fw_version(int boot_slot, char *version, int max_len);
int print_gecko_slot_info(void);
int get_current_slot(void);
int get_unused_slot(void);
int dfu_mcu_firmware_upgrade(int slot_to_upgrade, char *bin_file, char *sha_file);
bool slot_is_safe_to_erase(int slot);
#endif
#endif
