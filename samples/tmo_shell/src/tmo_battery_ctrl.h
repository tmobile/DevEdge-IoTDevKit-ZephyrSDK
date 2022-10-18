/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_BATTERY_CTRL_H
#define TMO_BATTERY_CTRL_H

#include <stdint.h>

/** Uint : Percentage **/
#define FULL_CHARGED_LV       90
#define HIGH_CHARGED_LV       80
#define LOW_CHARGED_LV        30
#define MIN_CHARGED_LV        10

#define HIGH_BATTERY_LV       60
#define LOW_BATTERY_LV        30

#define PROTECTED_CUT_LV      6

#define CHARGER_STAT_READY    0
#define CHARGER_STAT_INPROG   1
#define CHARGER_STAT_FULL     2
#define CHARGER_STAT_FAULT    3

#define CUR_STAT_UNKNOWN      0
#define CUR_STAT_BATTERY      10
#define CUR_STAT_INPROG       20
#define CUR_STAT_FULL         30
#define CUR_STAT_FAULT        40

bool getBatteryPercent(uint8_t *bv);
void apply_filter(float *bv);
float get_remaining_capacity(float battery_voltage);

#endif
