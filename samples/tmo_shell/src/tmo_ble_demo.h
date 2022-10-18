/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_BLE_DEMO_H
#define TMO_BLE_DEMO_H

#include <shell/shell.h>

typedef struct sensor_value SENSOR_VALUE_STRUCT;
int read_accelerometer( SENSOR_VALUE_STRUCT* acc_sensor_arr);
bool fetch_temperature(SENSOR_VALUE_STRUCT* temp);
bool fetch_light(SENSOR_VALUE_STRUCT* light);
bool fetch_ir(SENSOR_VALUE_STRUCT* ir);
bool fetch_pressure(SENSOR_VALUE_STRUCT* temp);

int cmd_ble_adv_ebeacon(const struct shell *shell, size_t argc, char **argv);
int cmd_ble_adv_ibeacon(const struct shell *shell, size_t argc, char **argv);
int cmd_ble_adv_conn(const struct shell *shell, size_t argc, char **argv);
int cmd_ble_conn_rssi(const struct shell *shell, size_t argc, char **argv);

#endif
