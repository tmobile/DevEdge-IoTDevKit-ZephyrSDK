/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr.h>
#include <net/socket.h>
#include <net/wifi_mgmt.h>
#include <net/net_event.h>
#include <shell/shell.h>
#include <shell/shell_help.h>

#include "tmo_shell.h"

#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_SCAN_RESULT | \
		NET_EVENT_WIFI_SCAN_DONE |		\
		NET_EVENT_WIFI_CONNECT_RESULT |		\
		NET_EVENT_WIFI_DISCONNECT_RESULT)

static struct {
	const struct shell *shell;

	union {
		struct {

			uint8_t connecting	: 1;
			uint8_t disconnecting	: 1;
			uint8_t _unused		: 6;
		};
		uint8_t all;
	};
} context;

static uint32_t scan_result;

static struct net_mgmt_event_callback wifi_shell_mgmt_cb;

#define print(shell, level, fmt, ...)					\
	do {								\
		if (shell) {						\
			shell_fprintf(shell, level, fmt, ##__VA_ARGS__); \
		} else {						\
			printk(fmt, ##__VA_ARGS__);			\
		}							\
	} while (false)


static char *net_byte_to_hex(char *ptr, uint8_t byte, char base, bool pad)
{
	int i, val;

	for (i = 0, val = (byte & 0xf0) >> 4; i < 2; i++, val = byte & 0x0f) {
		if (i == 0 && !pad && !val) {
			continue;
		}
		if (val < 10) {
			*ptr++ = (char) (val + '0');
		} else {
			*ptr++ = (char) (val - 10 + base);
		}
	}

	*ptr = '\0';

	return ptr;
}

static char *net_sprint_ll_addr_buf(const uint8_t *ll, uint8_t ll_len,
		char *buf, int buflen)
{
	uint8_t i, len, blen;
	char *ptr = buf;

	if (ll == NULL) {
		return "<unknown>";
	}

	switch (ll_len) {
		case 8:
			len = 8U;
			break;
		case 6:
			len = 6U;
			break;
		case 2:
			len = 2U;
			break;
		default:
			len = 6U;
			break;
	}

	for (i = 0U, blen = buflen; i < len && blen > 0; i++) {
		ptr = net_byte_to_hex(ptr, (char)ll[i], 'A', true);
		*ptr++ = ':';
		blen -= 3U;
	}

	if (!(ptr - buf)) {
		return NULL;
	}

	*(ptr - 1) = '\0';
	return buf;
}

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry =
		(const struct wifi_scan_result *)cb->info;
	uint8_t mac_string_buf[sizeof("xx:xx:xx:xx:xx:xx")];

	scan_result++;

	if (scan_result == 1U) {
		print(context.shell, SHELL_NORMAL,
				"%-4s | %-32s %-5s | %-4s | %-4s | %-5s    | %s\n",
				"Num", "SSID", "(len)", "Chan", "RSSI", "Sec", "MAC");
	}

	print(context.shell, SHELL_NORMAL, "%-4d | %-32s %-5u | %-4u | %-4d | %-5s | %s\n",
			scan_result, entry->ssid, entry->ssid_length, entry->channel, entry->rssi,
			(entry->security == WIFI_SECURITY_TYPE_PSK ? "WPA/WPA2" : "Open    "),
			((entry->mac_length) ?
			 net_sprint_ll_addr_buf(entry->mac, WIFI_MAC_ADDR_LEN, mac_string_buf,
				 sizeof(mac_string_buf)) : ""));
}

static void handle_wifi_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *)cb->info;

	if (status->status) {
		print(context.shell, SHELL_WARNING,
				"Scan request failed (%d)\n", status->status);
	}
	scan_result = 0U;
}

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *) cb->info;

	if (status->status) {
		print(context.shell, SHELL_WARNING,
				"Connection request failed (%d)\n", status->status);
	} else {
		print(context.shell, SHELL_NORMAL, "Connected\n");
	}

	context.connecting = false;
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *) cb->info;

	if (context.disconnecting) {
		print(context.shell,
				status->status ? SHELL_WARNING : SHELL_NORMAL,
				"Disconnection request %s (%d)\n",
				status->status ? "failed" : "done",
				status->status);
		context.disconnecting = false;
	} else {
		print(context.shell, SHELL_NORMAL, "Disconnected\n");
	}
}

bool status_requested = false;

static void handle_wifi_status_result(struct net_mgmt_event_callback *cb)
{
	if (!status_requested) {
		return;
	} else {
		status_requested = false;
	}
	const struct wifi_status_result *entry =
		(const struct wifi_status_result *)cb->info;

	uint8_t ip4_string_buf[sizeof("255.255.255.255")];
	uint8_t ip6_string_buf[sizeof("0000:0000:0000:0000:0000:0000:0000:0000")];

	print(context.shell, SHELL_NORMAL,
			"\n%-4s | %-12s | %-32s %-5s | %-4s | %-4s | %-5s    | %-15s | %s\n",
			"Mode", "State", "SSID", "(len)", "Chan", "RSSI", "Sec", "IPv4", "IPv6");
	bool has_ip6 = entry->ip6.s6_addr32[0] || entry->ip6.s6_addr32[1] 
		|| entry->ip6.s6_addr32[2] || entry->ip6.s6_addr32[3];
	print(context.shell, SHELL_NORMAL, "%-4s | %-12s | %-32s %-5u | %-4u | %-4d | %-5s | %-15s | %s\n",
			(entry->ap_mode ? " AP" : " ST"),
			(entry->connected ? "Connected" : "Disconnected"),
			(entry->ssid ? (char *)entry->ssid : ""),
			entry->ssid_length, entry->channel, entry->rssi,
			(entry->security == WIFI_SECURITY_TYPE_PSK ? "WPA/WPA2" : "Open    "),
			((entry->ip4.s_addr) ?
			 net_addr_ntop(AF_INET, &entry->ip4, ip4_string_buf,
				 sizeof(ip4_string_buf)) : ""),
			((has_ip6) ?
			 net_addr_ntop(AF_INET6, &entry->ip6, ip6_string_buf,
				 sizeof(ip6_string_buf)) : ""));
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
		uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
		case NET_EVENT_WIFI_SCAN_RESULT:
			handle_wifi_scan_result(cb);
			break;
		case NET_EVENT_WIFI_SCAN_DONE:
			handle_wifi_scan_done(cb);
			break;
		case NET_EVENT_WIFI_CONNECT_RESULT:
			handle_wifi_connect_result(cb);
			break;
		case NET_EVENT_WIFI_DISCONNECT_RESULT:
			handle_wifi_disconnect_result(cb);
			break;
		case NET_EVENT_WIFI_STATUS_RESULT:
			handle_wifi_status_result(cb);
			break;
		default:
			break;
	}
}

static int __wifi_args_to_params(size_t argc, char *argv[],
		struct wifi_connect_req_params *params)
{
	char *endptr;
	int idx = 1;

	if (argc < 1) {
		return -EINVAL;
	}

	/* SSID */
	params->ssid = argv[0];
	params->ssid_length = strlen(params->ssid);

	/* Channel (optional) */
	if ((idx < argc) && (strlen(argv[idx]) <= 2)) {
		params->channel = strtol(argv[idx], &endptr, 10);
		if (*endptr != '\0') {
			return -EINVAL;
		}

		if (params->channel == 0U) {
			params->channel = WIFI_CHANNEL_ANY;
		}

		idx++;
	} else {
		params->channel = WIFI_CHANNEL_ANY;
	}

	/* PSK (optional) */
	if (idx < argc) {
		params->psk = argv[idx];
		params->psk_length = strlen(argv[idx]);
		params->security = WIFI_SECURITY_TYPE_PSK;
	} else {
		params->security = WIFI_SECURITY_TYPE_NONE;
	}

	return 0;
}


int cmd_wifi_connect(const struct shell *shell, size_t argc,
		char *argv[])
{
	// struct net_if *iface = net_if_get_default();

	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		z_shell_help_subcmd_print_selitem(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", idx);
		return -EINVAL;
	} else if (!strstr(iface->if_dev->dev->name,"RS9116")) { //Stop-Gap solution
		shell_error(shell, "Operation not supported on non-WiFi interfaces");
		return -EINVAL;
	}

	static struct wifi_connect_req_params cnx_params;

	if (__wifi_args_to_params(argc - 2, &argv[2], &cnx_params)) {
		shell_help(shell);
		return -ENOEXEC;
	}

	context.connecting = true;
	context.shell = shell;

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
				&cnx_params, sizeof(struct wifi_connect_req_params))) {
		shell_fprintf(shell, SHELL_WARNING,
				"Connection request failed\n");
		context.connecting = false;

		return -ENOEXEC;
	}
	return 0;
}

int cmd_wifi_disconnect(const struct shell *shell, size_t argc,
		char *argv[])
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		z_shell_help_subcmd_print_selitem(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", idx);
		return -EINVAL;
	} else if (!strstr(iface->if_dev->dev->name,"RS9116")) { //Stop-Gap solution
		shell_error(shell, "Operation not supported on non-WiFi interfaces");
		return -EINVAL;
	}

	// struct net_if *iface = net_if_get_default();
	int status;

	context.disconnecting = true;
	context.shell = shell;

	status = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);

	if (status) {
		context.disconnecting = false;

		if (status == -EALREADY) {
			shell_fprintf(shell, SHELL_INFO,
					"Already disconnected\n");
		} else {
			shell_fprintf(shell, SHELL_WARNING,
					"Disconnect request failed\n");
			return -ENOEXEC;
		}
	}
	return 0;
}

int cmd_wifi_scan(const struct shell *shell, size_t argc, char *argv[])
{
	// struct net_if *iface = net_if_get_default();
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		z_shell_help_subcmd_print_selitem(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", idx);
		return -EINVAL;
	} else if (!strstr(iface->if_dev->dev->name,"RS9116")) { //Stop-Gap solution
		shell_error(shell, "Operation not supported on non-WiFi interfaces");
		return -EINVAL;
	}

	context.shell = shell;

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0)) {
		shell_fprintf(shell, SHELL_WARNING, "Scan request failed\n");

		return -ENOEXEC;
	}
	return 0;
}

int cmd_wifi_status(const struct shell *shell, size_t argc, char *argv[])
{
	status_requested = true;
	// struct net_if *iface = net_if_get_default();
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		z_shell_help_subcmd_print_selitem(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", idx);
		return -EINVAL;
	} else if (!strstr(iface->if_dev->dev->name,"RS9116")) { //Stop-Gap solution
		shell_error(shell, "Operation not supported on non-WiFi interfaces");
		return -EINVAL;
	}

	context.shell = shell;

	if (net_mgmt(NET_REQUEST_WIFI_STATUS, iface, NULL, 0)) {
		shell_fprintf(shell, SHELL_WARNING, "Status request failed\n");

		return -ENOEXEC;
	}
	return 0;
}

int tmo_wifi_connect()
{
	int ret = -1;

	if(strlen(CONFIG_TMO_SHELL_SSID)){
		printf("Connecting to WiFi SSID %s\n", CONFIG_TMO_SHELL_SSID);
		struct wifi_connect_req_params cnx_params;
		cnx_params.channel = WIFI_CHANNEL_ANY;
		cnx_params.ssid = CONFIG_TMO_SHELL_SSID;
		cnx_params.ssid_length = strlen(CONFIG_TMO_SHELL_SSID);
		cnx_params.security = strlen(CONFIG_TMO_SHELL_PSK) ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE;
		cnx_params.psk = CONFIG_TMO_SHELL_PSK;
		cnx_params.psk_length = strlen(CONFIG_TMO_SHELL_PSK);

		struct net_if *iface = net_if_get_by_index(2);
		if (iface == NULL) {
			printf("Interface %d not found", 2);
			return -EINVAL;
		} else if (!strstr(iface->if_dev->dev->name,"RS9116")) { //Stop-Gap solution
			printf("Operation not supported on non-WiFi interfaces");
			return -EINVAL;
		}

		ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
				&cnx_params, sizeof(struct wifi_connect_req_params));
	}
	return ret;
}

static int tmo_wifi_shell_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	context.shell = NULL;
	context.all = 0U;
	scan_result = 0U;

	net_mgmt_init_event_callback(&wifi_shell_mgmt_cb,
			wifi_mgmt_event_handler,
			WIFI_SHELL_MGMT_EVENTS);

	net_mgmt_add_event_callback(&wifi_shell_mgmt_cb);

	return 0;
}

SYS_INIT(tmo_wifi_shell_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
