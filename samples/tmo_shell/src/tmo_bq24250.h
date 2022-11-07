/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_BATT_CHARGER_H
#define TMO_BATT_CHARGER_H

int get_bq24250_status(uint8_t *charging, uint8_t *vbus, uint8_t *attached, uint8_t *fault);

#endif
