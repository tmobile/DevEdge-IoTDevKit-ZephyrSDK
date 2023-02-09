/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_ADC_H
#define TMO_ADC_H

int read_battery_voltage(void);
int read_hwid(void);
bool millivolts_to_percent(uint32_t millivolts, uint8_t *bv);
void initADC();

#endif
