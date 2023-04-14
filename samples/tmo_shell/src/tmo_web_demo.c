/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/posix/fcntl.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#if CONFIG_MODEM
#include <zephyr/drivers/modem/murata-1sc.h>
#endif

#include "tmo_adc.h"
#include "tmo_gnss.h"
#include "tmo_ble_demo.h"
#include "tmo_web_demo.h"
#include "tmo_http_request.h"
#include "tmo_shell.h"
#include "tmo_battery_ctrl.h"

static struct web_demo_settings_t web_demo_settings = {false, 0, 2, TRANSMIT_INTERVAL_SECS_WEB};
#define MAX_BASE_URL_SIZE  100
#define MAX_PATH_SIZE      100
static char base_url_s[MAX_BASE_URL_SIZE] = "https://devkitmqtt.devedge.t-mobile.com";
static char path_s[MAX_PATH_SIZE] = "/prd/bridge/";
#define MAX_SIZE_OF_BUFFER  20

static char json_payload[MAX_PAYLOAD_BUFFER_SIZE] = {0};
static inline void strupper(char *p) { while (*p) *p++ &= 0xdf;}
static uint8_t battery_attached = 0 ;
static uint8_t fault = 0 ;
static const char *battery_state_string[] = {
	"charging", "not-charging", "not-attached", "attached",
};

bool get_transmit_flag()
{
	return web_demo_settings.transmit_flag;
}

int get_json_iface_type()
{
	return web_demo_settings.iface_type;
}

bool set_transmit_json_flag( bool user_transmit_setting)
{
	web_demo_settings.transmit_flag = user_transmit_setting;
	return web_demo_settings.transmit_flag;
}

void set_transmit_interval(int secs)
{
	web_demo_settings.transmit_interval = secs;
}

int set_json_iface_type (int iface_type)
{
	web_demo_settings.iface_type = iface_type;
	return web_demo_settings.iface_type;
}

int set_json_base_url(const char *base_url)
{
	snprintf(base_url_s, sizeof(base_url_s), "%s", base_url);
	return 0;
}

char *get_json_base_url()
{
	return base_url_s;
}

int set_json_path(const char *path)
{
	if (path[strlen(path)-1] == '/') {
		snprintf(path_s, sizeof(path_s), "%s", path);
	} else {
		snprintf(path_s, sizeof(path_s), "%s/", path);
	}
	return 0;
}

char *get_json_path()
{
	return path_s;
}

int increment_number_http_requests()
{
	web_demo_settings.number_http_requests++;
	return web_demo_settings.number_http_requests;
}

int get_web_demo_settings(struct web_demo_settings_t *ws)
{
	if (ws != NULL) {
		memcpy(ws, &web_demo_settings, sizeof(struct web_demo_settings_t));
	}
	return 0;
}

int get_cell_strength(int *val)
{
	// What is the idx value to use - find the define and replace the hard-code
	int ret = 0;
	struct net_if *iface = net_if_get_by_index(1);
	if (iface == NULL) {
		return -EINVAL;
	}
	if (!strstr(iface->if_dev->dev->name, "murata")) {
		return -EINVAL;
	}
	int sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);
	if (sd == -1) {
		return -ENOENT;
	}

	char cmd_buf[20];
	strcpy(cmd_buf, "SSI");
	strupper(cmd_buf);
	int res = fcntl(sd, GET_ATCMD_RESP, cmd_buf);
	if (res < 0) {
		ret = -EAGAIN;
	} else if (cmd_buf[0] == 0) {
		ret = -EAGAIN;
	}

	int stat = zsock_close(sd);
	if (stat < 0) {
		// Close failed;
		return stat;
	}

	int sig_strength = 0;
	if (cmd_buf[0] != '\0') {
		sig_strength = atoi(cmd_buf);
	}
	*val = sig_strength;
	return ret;
}

int get_gnss_location_info(double* latitude, double* longitude, double* alt, double* hdop)
{
	*latitude  = gnss_values.lat;

	*longitude = gnss_values.lon;

	*alt = gnss_values.alt;

	*hdop = gnss_values.hdop;

	return 0;
}

int  create_json()
{
	int val;
	double lat, lon, alt, hdop;
	int ret_val;
	struct sensor_value sensor_value_arr[3];
	int buffer_size = MAX_PAYLOAD_BUFFER_SIZE;
	memset(json_payload, 0, MAX_PAYLOAD_BUFFER_SIZE);

	// test blank payload
	// json_payload[0] = '\0';
	// return strlen(json_payload);

	// test simple payload
	// snprintf(json_payload, sizeof(json_payload), "{ \"x\":\"hello\" }");
	// return strlen(json_payload);

	// Initial bracket
	int total_bytes_written = ret_val = snprintf(json_payload, buffer_size, "{\n");
	int num_bytes_avail_buffer = buffer_size - total_bytes_written;

	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		read_accelerometer(sensor_value_arr);
		double val0 = sensor_value_to_double(sensor_value_arr);
		double val1 = sensor_value_to_double(&sensor_value_arr[1]);
		double val2 = sensor_value_to_double(&sensor_value_arr[2]);
		ret_val = snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
				"\"accelerometer\":{\n\"x\":%.2lf,\n\"y\":%.2lf,\n\"z\":%.2lf\n},\n",
				val0, val1, val2);
	} else {
		return ret_val;
	}

	num_bytes_avail_buffer -= ret_val;
	total_bytes_written += ret_val;

	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		uint8_t percent = 0;
		uint32_t millivolts = 0;
		enum battery_state e_bat_state = battery_state_not_attached;
		if (battery_attached !=0) {
			millivolts = read_battery_voltage();
			millivolts_to_percent(millivolts, &percent);
			if (is_battery_charging()) {
				e_bat_state = battery_state_charging;
			} else {
				e_bat_state = battery_state_not_charging;
			}
		} else {
			e_bat_state = battery_state_not_attached;
		}
		ret_val = snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
				"\"battery\":{\n\"voltage\":%d.%03d,\n\"percent\":%d,\n\"state\":\"%s\"\n},\n",
				millivolts/1000, millivolts%1000, percent,
				battery_state_string[e_bat_state]);
	} else {
		return ret_val;
	}
	num_bytes_avail_buffer -= ret_val;
	total_bytes_written += ret_val;

	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		if (get_cell_strength(&val) == 0) {
			ret_val = snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
				"\"cellSignalStrength\":{\n\"dbm\":%d\n},\n", val);
		} else {
			ret_val = snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
				"\"cellSignalStrength\":{\n\"dbm\":null\n},\n");
		}
	} else {
		return ret_val;
	}
	num_bytes_avail_buffer -= ret_val;
	total_bytes_written += ret_val;

	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		if (fetch_temperature((struct sensor_value*) &sensor_value_arr[0])) {
			double val = sensor_value_to_double(sensor_value_arr);
			ret_val =  snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
					"\"temperature\":{\n\"temperatureCelsius\":%.1lf\n},\n", val);
		}
	} else {
		return ret_val;
	}
	num_bytes_avail_buffer -=  ret_val;
	total_bytes_written += ret_val;

	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		if ((fetch_light((struct sensor_value*) &sensor_value_arr[0])) &&
				(fetch_ir((struct sensor_value*) &sensor_value_arr[1]))) {
			double val0 = sensor_value_to_double(sensor_value_arr);
			double val1 = sensor_value_to_double(&sensor_value_arr[1]);
			ret_val =  snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
					"\"ambientLight\":{\n\"visibleLux\":%.2lf,\n\"irLux\":%.2lf\n},\n",
					val0, val1);
		}
	} else {
		return ret_val;
	}
	num_bytes_avail_buffer -=  ret_val;
	total_bytes_written += ret_val;

#if CONFIG_LPS22HH
	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		if (fetch_pressure((struct sensor_value*) &sensor_value_arr[0])) {
			double val = sensor_value_to_double(sensor_value_arr);
			ret_val =  snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
					"\"pressure\":{\n\"kPa\":%.2lf\n},\n", val);
		}
	} else {
		return ret_val;
	}
	num_bytes_avail_buffer -=  ret_val;
	total_bytes_written += ret_val;
#endif

	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		get_gnss_location_info(&lat, &lon, &alt, &hdop);
		ret_val = snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer,
				"\"map\":{\n\"lat\":%.6lf,\n\"lng\":%.6lf,\n\"alt\":%.2lf,\n\"hdop\":%.2lf\n}\n", lat, lon, alt, hdop);
	} else {
		return ret_val;
	}
	num_bytes_avail_buffer -=  ret_val;
	total_bytes_written += ret_val;

	if (ret_val >= 0 && total_bytes_written < buffer_size) {
		// Final bracket
		ret_val = snprintf(json_payload+total_bytes_written, num_bytes_avail_buffer, "}\n");
	} else {
		return ret_val;
	}
	total_bytes_written += ret_val;

#ifdef CONFIG_DEBUG_JSON_GENERATION
	printf("\n total_bytes_written %d ", total_bytes_written);
	printf("\n%s\n", json_payload);
#endif
	return total_bytes_written;
}

char* get_json_payload_pointer()
{
	return json_payload;
}

static void tmo_web_demo_notif_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	k_sleep(K_SECONDS(TRANSMIT_INTERVAL_SECS_WEB));

	while (1) {
		k_sleep(K_SECONDS(web_demo_settings.transmit_interval));
		uint8_t charging = 0;
		uint8_t vbus = 0;
		if (get_transmit_flag()) {
			get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
			create_json();
			increment_number_http_requests();
			tmo_http_json();
		}
	}
}

#define TMO_WEB_DEMO_NOTIF_THREAD_STACK_SIZE 2048
#define TMO_WEB_DEMO_NOTIF_THREAD_PRIORITY CONFIG_MAIN_THREAD_PRIORITY

K_THREAD_DEFINE(tmo_web_demo_notif_tid, TMO_WEB_DEMO_NOTIF_THREAD_STACK_SIZE,
		tmo_web_demo_notif_thread, NULL, NULL, NULL,
		TMO_WEB_DEMO_NOTIF_THREAD_PRIORITY, 0, 0);
