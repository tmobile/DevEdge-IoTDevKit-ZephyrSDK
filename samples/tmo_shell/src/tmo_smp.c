/*
 * Copyright (c) 2015-2016 Intel Corporation
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/shell/shell.h>

bool smp_enabled = false;

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	printf("Pairing Complete\n");
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	printf("Pairing Failed (%d). Disconnecting.\n", reason);
	bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
}

static struct bt_conn_auth_info_cb auth_cb_info = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printf("Failed to connect to %s (%u)\n", addr, err);
		return;
	}

	printf("Connected %s\n", addr);

	if (smp_enabled){
		if (bt_conn_set_security(conn, BT_SECURITY_L4)) {
			printf("Failed to set security\n");
		}
	}	
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printf("Disconnected from %s (reason 0x%02x)\n", addr, reason);
}

static void identity_resolved(struct bt_conn *conn, const bt_addr_le_t *rpa,
		const bt_addr_le_t *identity)
{
	char addr_identity[BT_ADDR_LE_STR_LEN];
	char addr_rpa[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
	bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

	printf("Identity resolved %s -> %s\n", addr_rpa, addr_identity);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
		enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printf("Security changed: %s level %u\n", addr, level);
	} else {
		printf("Security failed: %s level %u err %d\n", addr, level,
				err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.identity_resolved = identity_resolved,
	.security_changed = security_changed,
};

// static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
// {
// 	char addr[BT_ADDR_LE_STR_LEN];

// 	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

// 	printf("Passkey for %s: %06u\n", addr, passkey);
// }

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printf("Pairing cancelled: %s\n", addr);
}

struct bt_conn *conn_passkey = NULL;
struct bt_conn *conn_confirm = NULL;
struct bt_conn *conn_pkd = NULL;

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	conn_pkd = conn;
	printf("Passkey for %s: %06u\n", addr, passkey);
}

static void cb_passkey_entry(struct bt_conn *conn)
{
	printf("Passkey Entry Requested\n");
	conn_passkey = conn;
}

static void cb_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	printf("Confirm key %d\n", passkey);
	conn_confirm = conn;
}

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
	.passkey_confirm = NULL,
};

bool display_enabled = true;
bool kb_enabled = false;
bool confirm_enabled = false;

int send_passkey(const struct shell *shell, int argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing argument");
		return -1;
	}
	int passkey = atoi(argv[1]);
	shell_print(shell, "Sending passkey %d", passkey);
	bt_conn_auth_passkey_entry(conn_passkey, passkey);
	conn_passkey = NULL;
	return 0;
}

int send_confirm(const struct shell *shell, int argc, char **argv)
{
	bt_conn_auth_pairing_confirm(conn_confirm);
	conn_confirm = NULL;
	return 0;
}

int toggle_display(const struct shell *shell, int argc, char **argv)
{
	display_enabled = !display_enabled;
	if (display_enabled){
		auth_cb_display.passkey_display = auth_passkey_display;
		shell_print(shell, "Display is now enabled");
	} else {
		auth_cb_display.passkey_display = NULL;
		shell_print(shell, "Display is now disabled");
	}
	return 0;
}

int toggle_keyboard(const struct shell *shell, int argc, char **argv)
{
	kb_enabled = !kb_enabled;
	if (kb_enabled){
		auth_cb_display.passkey_entry = cb_passkey_entry;
		shell_print(shell, "Keyboard is now enabled");
	} else {
		auth_cb_display.passkey_entry = NULL;
		shell_print(shell, "Keyboard is now disabled");
	}
	return 0;
}

int toggle_confirm(const struct shell *shell, int argc, char **argv)
{
	confirm_enabled = !confirm_enabled;
	if (confirm_enabled){
		auth_cb_display.passkey_confirm = cb_passkey_confirm;
		shell_print(shell, "Confirm is now enabled");
	} else {
		auth_cb_display.passkey_confirm = NULL;
		shell_print(shell, "Confirm is now disabled");
	}
	return 0;
}

int show_enabled(const struct shell *shell, int argc, char **argv)
{
	shell_print(shell, "CONFIRM: %s", confirm_enabled ? "enabled":"disabled");
	shell_print(shell, "DISPLAY: %s", display_enabled ? "enabled":"disabled");
	shell_print(shell, "KEYBOARD: %s", kb_enabled ? "enabled":"disabled");
	return 0;
}

int send_cancel(const struct shell *shell, int argc, char **argv)
{
	if (conn_confirm){
		bt_conn_auth_cancel(conn_confirm);
		conn_confirm = NULL;
	} else if (conn_passkey) {
		bt_conn_auth_cancel(conn_passkey);
		conn_passkey = NULL;
	} else if (conn_pkd) {
		bt_conn_auth_cancel(conn_pkd);
		conn_pkd = NULL;
	}
	return 0;
}

int smp_enable(const struct shell *shell, int argc, char **argv)
{
	smp_enabled = true;
	shell_print(shell, "SMP Enabled.");
	return 0;
};

int smp_disable(const struct shell *shell, int argc, char **argv)
{
	smp_enabled = false;
	shell_print(shell, "SMP disabled.");
	return 0;
};

void tmo_smp_shell_init()
{
	bt_conn_auth_cb_register(&auth_cb_display);
	bt_conn_cb_register(&conn_callbacks);
	// bt_conn_auth_info_cb_register is defined in subsys/bluetooth/host/conn.c
	// However, the host folder is only built if CONFIG_BT_HCI is defined
	bt_conn_auth_info_cb_register(&auth_cb_info);
}
