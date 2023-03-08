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
float get_remaining_capacity(float battery_voltage);
bool is_battery_charging(void);
int get_battery_charging_status(uint8_t *charging, uint8_t *vbus, uint8_t *attached, uint8_t *fault);

void battery_apply_filter(float *bv);
uint8_t battery_millivolts_to_percent(uint32_t millivolts);

#endif
