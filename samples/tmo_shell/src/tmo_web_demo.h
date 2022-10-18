/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_WEB_DEMO_H
#define TMO_WEB_DEMO_H

#define TRANSMIT_INTERVAL_SECS_WEB   10
#define MAX_PAYLOAD_BUFFER_SIZE      400

typedef struct sensor_value SENSOR_VALUE_STRUCT;
struct web_demo_settings_t {
	bool transmit_flag;
	unsigned int number_http_requests;
	unsigned int iface_type;
	unsigned int transmit_interval;
};

enum battery_state {
	battery_state_charging,
	battery_state_not_charging,
	battery_state_not_attached,
	battery_state_attached
};

void set_battery_charging_status(uint8_t* charging, uint8_t* vbus, uint8_t* attached, uint8_t* fault);
bool get_transmit_flag();
bool set_transmit_json_flag( bool user_transmit_setting);
void set_transmit_interval(int secs);
char* get_json_payload_pointer();
int increment_number_http_requests();
int get_web_demo_settings(struct web_demo_settings_t *ws);
int set_json_iface_type (int iface_type);
int get_json_iface_type();
int set_json_base_url(const char *base_url);
char *get_json_base_url();
int set_json_path(const char *path);
char *get_json_path();
int get_cell_strength(int *val);

int  create_json();
int read_accelerometer( SENSOR_VALUE_STRUCT *acc_sensor_arr);
#ifdef CONFIG_DEBUG_TMO_WEB_DEMO
#define printf_debug printf
#else
#define printf_debug  if(0)printf
#endif

#endif
