/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_SHELL_H
#define TMO_SHELL_H

#include <stddef.h>
#include <stdint.h>
#include <net/net_if.h>
#include <errno.h>

#define MAX_MODEM_SOCKS	5
#define MAX_SOCK_REC	16

struct test
{
	int pass;
	int fail;
};

enum sock_rec_flags {
	sock_open = 0,
	sock_udp,
	sock_dtls,
	sock_tcp,
	sock_tls,
	sock_connected,
	sock_bound,
	sock_v6
};

typedef enum {
	MODEM_ID = 1,
	WIFI_ID,
} devId_type;

struct sock_rec_s {
	int sd;
	uint8_t flags;
	struct net_if *dev;
};

static inline void gen_payload(uint8_t *buf, int len)
{
	for (int i=0;i<len;i++)
		buf[i] = 0x20 + (i % 97);
}

#if defined(CONFIG_NET_OFFLOAD) && defined(CONFIG_NET_SOCKETS_OFFLOAD)
static inline int zsock_socket_ext(int family, int type, int proto, struct net_if *iface)
{
  if (iface->if_dev->offload && iface->if_dev->socket_offload != NULL){
    return iface->if_dev->socket_offload(family, type, proto);
  } else {
    errno = EINVAL;
    return -1;
  }
}
#endif

extern const struct shell *shell;

int cmd_wifi_connect(const struct shell *shell, size_t argc, char *argv[]);
int cmd_wifi_disconnect(const struct shell *shell, size_t argc, char *argv[]);
int cmd_wifi_scan(const struct shell *shell, size_t argc, char *argv[]);
int cmd_wifi_status(const struct shell *shell, size_t argc, char *argv[]);

int toggle_keyboard(const struct shell *shell, size_t argc, char *argv[]);
int toggle_confirm(const struct shell *shell, size_t argc, char *argv[]);
int toggle_display(const struct shell *shell, size_t argc, char *argv[]);

int send_passkey(const struct shell *shell, size_t argc, char *argv[]);
int send_confirm(const struct shell *shell, size_t argc, char *argv[]);
int send_cancel(const struct shell *shell, size_t argc, char *argv[]);

int smp_enable(const struct shell *shell, size_t argc, char *argv[]);
int smp_disable(const struct shell *shell, size_t argc, char *argv[]);
int show_enabled(const struct shell *shell, size_t argc, char *argv[]);
void tmo_shell_main(void);
int tmo_offload_init(int devid);

#endif
