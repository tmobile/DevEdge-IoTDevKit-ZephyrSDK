/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_GECKO_H
#define DFU_GECKO_H

#include "dfu_common.h"

#define DFU_IMAGE_HDR_LEN    32
#define DFU_SLOT0_FLASH_ADDR 0x10000
#define DFU_SLOT1_FLASH_ADDR 0x80000
#define DFU_IMAGE_MAGIC	     0x96f3b83d

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
