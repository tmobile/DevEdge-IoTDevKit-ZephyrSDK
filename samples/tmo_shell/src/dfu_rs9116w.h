/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_RS9116W_H
#define DFU_RS9116W_H

#define DFU_RS9116W_FW_VER_SIZE 20

int dfu_wifi_firmware_upgrade(void);
int32_t dfu_wifi_write_image(void);
int dfu_wifi_get_version(char *rsi_fw_version);

#endif
