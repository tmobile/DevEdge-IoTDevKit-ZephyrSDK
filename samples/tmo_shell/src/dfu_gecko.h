/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_GECKO_H
#define DFU_GECKO_H

int dfu_mcu_firmware_upgrade(int slot_to_upgrade);
int32_t dfu_gecko_write_image(int slot_to_upgrade);

#endif
