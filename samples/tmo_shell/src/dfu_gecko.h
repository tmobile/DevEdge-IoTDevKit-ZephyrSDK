/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_GECKO_H
#define DFU_GECKO_H

#include <stdint.h>

int dfu_mcu_firmware_upgrade(int slot_to_upgrade);
int32_t dfu_gecko_write_image(int slot_to_upgrade);
int get_oldest_slot(void);

/*
 * This file along with dfu_gecko.c is being jointly used by DevEdge for example purposes and Syncup SDK
 * for production purposes. The idea is to have a single place for Syncup to get updates and fixed from DevEdge.
 * As such the code is written in a way to function in both environments.
 */
#ifndef CONFIG_SYNCUP_SDK
#define CONFIG_SYNCUP_DFU_IMAGE_HDR_LEN 	32
#define CONFIG_SYNCUP_DFU_SLOT0_FLASH_ADDR 	0x10000
#define CONFIG_SYNCUP_DFU_SLOT1_FLASH_ADDR 	0x80000
#define CONFIG_SYNCUP_DFU_IMAGE_MAGIC 		0x96f3b83d
#define CONFIG_SYNCUP_MCU_NAME 				"Pearl Gecko"
#define CONFIG_SYNCUP_DFU_CHUNK_SIZE 		2048UL

#define CONFIG_SYNCUP_DFU_SLOT0_FILE "/tmo/zephyr.slot0.bin"
#define CONFIG_SYNCUP_DFU_SLOT1_FILE "/tmo/zephyr.slot1.bin"
#define CONFIG_SYNCUP_DFU_SLOT0_SHA1 "/tmo/zephyr.slot0.bin.sha1"
#define CONFIG_SYNCUP_DFU_SLOT1_SHA1 "/tmo/zephyr.slot1.bin.sha1"

#define CONFIG_SYNCUP_FS_READ_BUF_SIZE 4096

#define LOG_INFO printf
#define LOG_DEBUG printf
#define LOG_ERROR printf
#define LOG_WARN printf
#define ENDL "\n"
#else
#define ENDL ""
#endif

#define FILE_FW      0
#define FILE_SHA1    1
#define SHA1_LEN     20

struct image_version {
    uint8_t  iv_major;
    uint8_t  iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

/** Image header. All fields are in little endian byte order. */
struct image_header {
    uint32_t             ih_magic;
    uint32_t             ih_load_addr;
    uint16_t             ih_hdr_size;           /* Size of image header (bytes). */
    uint16_t             ih_protect_tlv_size;   /* Size of protected TLV area (bytes). */
    uint32_t             ih_img_size;           /* Does not include header. */
    uint32_t             ih_flags;              /* IMAGE_F_[...]. */
    struct image_version ih_ver;
};

#endif
