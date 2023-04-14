/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_MURATA_1SC_H
#define DFU_MURATA_1SC_H

#include <stdint.h>

#include "dfu_common.h"

#define DFU_MODEM_FW_VER_SIZE 32
#define UA_HEADER_SIZE	      256

uint32_t murata_1sc_crc32_update(uint32_t crc32, const uint8_t *data, size_t len);
uint32_t murata_1sc_crc32_finish(uint32_t crc32, size_t len);

int dfu_modem_get_version(char *dfu_murata_version_str);
int dfu_modem_firmware_upgrade(const struct dfu_file_t *dfu_file);

#endif
