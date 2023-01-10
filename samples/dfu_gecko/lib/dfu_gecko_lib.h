/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_GECKO_H
#define DFU_GECKO_H

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

int dfu_mcu_firmware_upgrade(int slot_to_upgrade);

#endif
