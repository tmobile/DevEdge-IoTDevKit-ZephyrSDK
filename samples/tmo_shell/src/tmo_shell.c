/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief tmo shell module
 *
 * Provides a shell with functionality that may be useful to applications
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tmo_shell, LOG_LEVEL_INF);

#include <stdio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/posix/fcntl.h>
#include <zephyr/net/socket.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <shell/shell_help.h>
#include <zephyr/net/http/client.h>
#include <zephyr/drivers/led.h>
#include <zephyr/sys/reboot.h>
#include <rsi_wlan_apis.h>

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include "tls_internal.h"
#include <zephyr/net/tls_credentials.h>
#include "ca_certificate.h"
typedef int sec_tag_t;
#endif

#include "tmo_modem_psm.h"
#include "tmo_modem_edrx.h"

#if CONFIG_MODEM
#include <zephyr/drivers/modem/murata-1sc.h>
#include "modem_sms.h"
#endif

#include "tmo_http_request.h"
#include "tmo_buzzer.h"
#include "tmo_gnss.h"
#include "tmo_web_demo.h"
#include "tmo_wifi.h"
#include "tmo_dfu_download.h"
#include "tmo_file.h"
#include "tmo_certs.h"
#include "tmo_adc.h"
#include "tmo_battery_ctrl.h"
#include "tmo_shell.h"
#include "tmo_sntp.h"
#include "tmo_modem.h"
#include "board.h"
#include "dfu_gecko_lib.h"

#if CONFIG_TMO_SHELL_BUILD_EK
#include "ek18/src/kermit_cmd.h"
#endif

#if CONFIG_PING
#include "tmo_ping.h"
#endif

#if CONFIG_PM_DEVICE
#include "tmo_pm.h"
#endif


#if CONFIG_PM
#include "tmo_pm_sys.h"
#endif

const struct device *ext_flash_dev = NULL;
const struct device *gecko_flash_dev = NULL;

#if (CONFIG_SPI_NOR - 0) || \
	DT_NODE_HAS_STATUS(DT_INST(0, jedec_spi_nor), okay)
#define FLASH_DEVICE DT_NODE_FULL_NAME(DT_INST(0, jedec_spi_nor))
#define FLASH_NAME "JEDEC SPI-NOR"
#elif (CONFIG_NORDIC_QSPI_NOR - 0) || \
	DT_NODE_HAS_STATUS(DT_INST(0, nordic_qspi_nor), okay)
#define FLASH_DEVICE DT_NODE_FULL_NAME(DT_INST(0, nordic_qspi_nor))
#define FLASH_NAME "JEDEC QSPI-NOR"
#elif DT_NODE_HAS_STATUS(DT_INST(0, st_stm32_qspi_nor), okay)
#define FLASH_DEVICE DT_NODE_FULL_NAME(DT_INST(0, st_stm32_qspi_nor))
#define FLASH_NAME "JEDEC QSPI-NOR"
#else
#error Unsupported flash driver
#endif

#define GECKO_FLASH_DEVICE DT_NODE_FULL_NAME(DT_INST(0, silabs_gecko_flash_controller))

extern const struct device *ext_flash_dev;
extern const struct device *gecko_flash_dev;
extern uint32_t getData;
extern int buzzer_test();
extern int led_test();
extern int misc_test();
extern int fw_test();
extern int ac_test();

const struct shell *shell = NULL;

#define READ_4K   4096
#define XFER_SIZE 5000
uint8_t mxfer_buf[XFER_SIZE+1];
int max_fragment = 1000;
int num_ifaces = 0;
bool board_has_gnss = false;

int murata_socket_offload_init(void);
int rs9116w_socket_offload_init(void);
void dump_addrinfo(const struct shell *shell, const struct addrinfo *ai);
int process_cli_cmd_modem_psm(const struct shell *shell, size_t argc, char **argv, int sd);
int process_cli_cmd_modem_edrx(const struct shell *shell, size_t argc, char **argv, int sd);
int process_cli_cmd_modem_edrx_ptw(const struct shell *shell, size_t argc, char **argv, int sd);

struct sock_rec_s socks[MAX_SOCK_REC] = {0};

#if defined(CONFIG_NET_SOCKETS_ENABLE_DTLS)
// int udp_cert_dtls(const struct shell *shell, size_t argc, char **argv);
// int udp_key_dtls(const struct shell *shell, size_t argc, char **argv);
int udp_cred_dtls(const struct shell *shell, size_t argc, char **argv);
int udp_profile_dtls(const struct shell *shell, size_t argc, char **argv);
#endif

int tmo_set_modem(enum murata_1sc_io_ctl cmd, union params_cmd* params, int sd)
{
	int res = -1;
	res = fcntl_ptr(sd, cmd, params);
	return res;
}

int tmo_offload_init(int devid)
{
	int ret = 0;

#if CONFIG_MODEM_MURATA_1SC
	if (MODEM_ID == devid) {
		murata_socket_offload_init();
	} else
#endif
#if CONFIG_WIFI_RS9116W
		if (WIFI_ID == devid) {
			rs9116w_socket_offload_init();
		} else
#endif
		{
			shell_error(shell, "Wrong device ID");
			return -EINVAL;
		}
	return ret;
}

int tcp_create_core(const struct shell *shell, size_t argc, char **argv, int family)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	
	if (idx > num_ifaces) {
		shell_error(shell, "Excessive interface number passed");
		return -EINVAL;
	}
	struct net_if *iface = net_if_get_by_index(idx);

	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", idx);
		return -EINVAL;
	}
	int sd = zsock_socket_ext(family, SOCK_STREAM, IPPROTO_TCP, iface);
	if (sd == -1) {
		shell_error(shell, "Socket creation failed, errno = %d", errno);
		return 0;
	}
	shell_print(shell, "Created socket %d", sd);
	for (int i = 0; i < MAX_SOCK_REC; i++) {
		if (!(socks[i].flags & BIT(sock_open))) {
			socks[i].dev = iface;
			socks[i].sd = sd;
			socks[i].flags = (BIT(sock_tcp) | BIT(sock_open)
					| ((family == AF_INET ? 0 : BIT(sock_v6))));
			break;
		}
	}
	return 0;
}

int tcp_create(const struct shell *shell, size_t argc, char **argv)
{
	return tcp_create_core(shell, argc, argv, AF_INET);
}

int tcp_createv6(const struct shell *shell, size_t argc, char **argv)
{
	return tcp_create_core(shell, argc, argv, AF_INET6);
}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
int tcp_create_tls_core(const struct shell *shell, size_t argc, char **argv, int family)
{
	int ret = 0;
	shell_print(shell, "Creating secure TCP (TLS) socket");

	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	int idx = strtol(argv[1], NULL, 10);
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", idx);
		return -EINVAL;
	}
	int sd = zsock_socket_ext(family, SOCK_STREAM, IPPROTO_TLS_1_2, iface);
	if (sd == -1) {
		shell_error(shell, "Socket creation failed, errno = %d", errno);
		return 0;
	}

	shell_info(shell, "Created socket %d", sd);

	for (int i = 0; i < MAX_SOCK_REC; i++) {
		if (!(socks[i].flags & BIT(sock_open))) {
			socks[i].dev = iface;
			socks[i].sd = sd;
			socks[i].flags = (BIT(sock_tls) | BIT(sock_open)
					| ((family == AF_INET ? 0 : BIT(sock_v6))));
			idx = i;	//for later tls socket
			break;
		}
	}

	sec_tag_t sec_tag_list[] = {
		CLIENT_CERTIFICATE_TAG,
	};

	ret = setsockopt(sd, SOL_TLS, TLS_SEC_TAG_LIST,
			sec_tag_list, sizeof(sec_tag_list));
	if (ret < 0) {
		shell_error(shell, "Failed to set TLS_SEC_TAG_LIST option: %d",
				errno);
		ret = -errno;
	}

	if (strlen(TLS_PEER_HOSTNAME)) {
		ret = setsockopt(sd, SOL_TLS, TLS_HOSTNAME,
				TLS_PEER_HOSTNAME, sizeof(TLS_PEER_HOSTNAME));
		if (ret < 0) {
			shell_error(shell, "Failed to set TLS_HOSTNAME option: %d",
					errno);
			ret = -errno;
		}
	}
	socks[idx].flags |= BIT(sock_tls);

	return ret;
}

int tcp_create_tls(const struct shell *shell, size_t argc, char **argv)
{
	return tcp_create_tls_core(shell, argc, argv, AF_INET);
}

int tcp_create_tlsv6(const struct shell *shell, size_t argc, char **argv)
{
	return tcp_create_tls_core(shell, argc, argv, AF_INET6);
}

#endif

int udp_create_core(const struct shell *shell, size_t argc, char **argv, int family)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);

	if (idx > num_ifaces) {
		shell_error(shell, "Excessive interface number passed");
		return -EINVAL;
	}
	struct net_if *iface = net_if_get_by_index(idx);
	
	if (iface == NULL) {
		shell_error(shell, "Interface %p not found", iface);
		return -EINVAL;
	}
	int sd = zsock_socket_ext(family, SOCK_DGRAM, IPPROTO_UDP, iface);
	if (sd == -1) {
		shell_error(shell, "Socket creation failed, errno = %d", errno);
		return 0;
	}
	shell_print(shell, "Created socket %d", sd);
	for (int i = 0; i < MAX_SOCK_REC; i++) {
		if (!(socks[i].flags & BIT(sock_open))) {
			socks[i].dev = iface;
			socks[i].sd = sd;
			socks[i].flags = (BIT(sock_udp) | BIT(sock_open)
					| ((family == AF_INET ? 0 : BIT(sock_v6))));
			break;
		}
	}
	return 0;
}

int udp_create(const struct shell *shell, size_t argc, char **argv)
{
	return udp_create_core(shell, argc, argv, AF_INET);
}

int udp_createv6(const struct shell *shell, size_t argc, char **argv)
{
	return udp_create_core(shell, argc, argv, AF_INET6);
}


#if defined(CONFIG_NET_SOCKETS_ENABLE_DTLS)
#define DTLS_ECHO_SERVER_CLIENT_PROFILE_ID 1
static bool cert_credential_added = false;
static bool key_credential_added = false;
static bool create_profile_done = false;
int udp_create_dtls_core(const struct shell *shell, size_t argc, char **argv, int family)
{
	int ret = 0;
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	if (!cert_credential_added) {
		ret = tls_credential_add(CLIENT_CERTIFICATE_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE,
				dev_certificate,
				sizeof(dev_certificate));
		if (ret < 0) {
			shell_error(shell, "<<< Failed to register public certificate: %d >>>\n", ret);
		}
		else {
			shell_info(shell, "===== Added DTLS ca_cert! =========\n");
			cert_credential_added = true;
		}
	}

	if (!key_credential_added) {
		ret = tls_credential_add(CLIENT_KEY_TAG, TLS_CREDENTIAL_PRIVATE_KEY,
				dev_private_key,
				sizeof(dev_private_key));
		if (ret < 0) {
			shell_error(shell, "<<< Failed to register dev private key  %d >>>\n", ret);
		}
		else {
			shell_info(shell, "===== Added DTLS dev private key credential! =========\n");
			key_credential_added = true;
		}
	}

	int idx = strtol(argv[1], NULL, 10);
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		shell_error(shell, "Interface %p not found", iface);
		return -EINVAL;
	}
	int sd = zsock_socket_ext(family, SOCK_DGRAM, IPPROTO_DTLS_1_2, iface);
	if (sd == -1) {
		shell_error(shell, "Socket creation failed, errno = %d", errno);
		return 0;
	}
	shell_print(shell, "Created socket %d", sd);
	for (int i = 0; i < MAX_SOCK_REC; i++) {
		if (!(socks[i].flags & BIT(sock_open))) {
			socks[i].dev = iface;
			socks[i].sd = sd;
			socks[i].flags = (BIT(sock_dtls) | BIT(sock_open)
					| ((family == AF_INET ? 0 : BIT(sock_v6))));
			idx = i;
			break;
		}
	}
	sec_tag_t sec_tag_list[] = {
		CLIENT_CERTIFICATE_TAG,
		CLIENT_KEY_TAG,
	};

	ret = setsockopt(sd, SOL_TLS, TLS_SEC_TAG_LIST,
			sec_tag_list, sizeof(sec_tag_list));
	if (ret < 0) {
		shell_error(shell, "Failed to set TLS_SEC_TAG_LIST option: %d",
				errno);
		ret = -errno;
	}

	if (strlen(TLS_PEER_HOSTNAME)) {
		ret = setsockopt(sd, SOL_TLS, TLS_HOSTNAME,
				TLS_PEER_HOSTNAME, sizeof(TLS_PEER_HOSTNAME));
		if (ret < 0) {
			shell_error(shell, "Failed to set TLS_HOSTNAME option: %d",
			errno);
			ret = -errno;
			}
	}
	socks[idx].flags |= BIT(sock_dtls);

	return ret;
}

int tmo_dtls_cred(const struct shell *shell, char* cred_type, char* operation, int sd)
{
	int ret = 0;
	struct tls_credential cred_s;
	size_t credlen;
	union params_cmd  params_cmd_u;
	union params_cmd* u = &params_cmd_u;
	const char *cred;
	enum tls_credential_type ct_val;
	char filename[32] = "dtls_cred_";
	if (!strcmp(cred_type, "cert")) {
		ct_val = TLS_CREDENTIAL_SERVER_CERTIFICATE;
		cred = dev_certificate;
		credlen = sizeof(dev_certificate);
		strcat(filename, "devcert");
	} else if (!strcmp(cred_type, "ca")) {
		ct_val = TLS_CREDENTIAL_CA_CERTIFICATE;
		cred = dtls_server_cert;
		credlen = sizeof(dtls_server_cert);
		strcat(filename, "cacert");
	} else if (!strcmp(cred_type, "key")) {
		ct_val = TLS_CREDENTIAL_PRIVATE_KEY;
		cred = dev_private_key;
		credlen = sizeof(dev_private_key);
		strcat(filename, "devkey");
	} else {
		shell_error(shell, "Invalid credential type: %s", cred_type);
		return 1;
	}
	strcat(filename, ".pem");
	if (((strcmp(operation,"w") !=  0) && (strcmp(operation,"d") != 0)) || sd < 0 || sd >= MAX_SOCK_REC ) {
		shell_error(shell, "\n Invalid Inputs, operation %s sd %d ", operation, sd);
	}


	/* Here do AT%CERTCMD  to write key cert to modem NVRAM */
	memset(u, 0, sizeof(params_cmd_u));
	memset(&cred_s, 0, sizeof(struct tls_credential));
	cred_s.tag = 0;
	cred_s.type = ct_val;
	cred_s.buf = cred;
	cred_s.len = credlen;
	u->cparams.cert = &cred_s;
	u->cparams.filename = filename;
	if (strcmp(operation,"w") == 0) {
		if ((ret = tmo_set_modem(STORE_CERT, (union params_cmd*)u, sd)) == 0) {
			shell_info(shell, "Stored %s using sd: %d ", cred_type, sd);
		} else {
			shell_error(shell, "Failed to store %s - sd:%d errno: %d", cred_type, sd, errno);
		}
	} else {
		if ((ret = tmo_set_modem(DEL_CERT, (union params_cmd*)filename, sd)) == 0) {
			shell_info(shell, "Deleted %s using sd: %d ", cred_type, sd);
		} else {
			shell_error(shell, "Failed to delete %s using sd:%d  error:%d", cred_type, sd, errno);
		}
	}
	return ret;
}


int tmo_profile_dtls(const struct shell *shell, char* operation, int sd)
{
	int ret = 0;
		union params_cmd  params_cmd_u;
		union params_cmd* u = &params_cmd_u;

	if (((strcmp(operation,"a") !=  0) && (strcmp(operation,"d") != 0)) || sd < 0 || sd >= MAX_SOCK_REC) {
		shell_error(shell, "\n Invalid Inputs, operation %s sd %d ", operation, sd);
	}

	memset(u, 0, sizeof(struct murata_tls_profile_params));
		u->profile.profile_id_num = DTLS_ECHO_SERVER_CLIENT_PROFILE_ID;
	u->profile.ca_file = NULL;
	u->profile.ca_path = NULL;
	u->profile.dev_cert = "dtls_cred_devcert.pem";
	u->profile.dev_key =  "dtls_cred_devkey.pem";
	u->profile.psk_id = NULL;
	u->profile.psk_key = NULL;

	if (strcmp(operation, "a") == 0) {
		if (tmo_set_modem(CREATE_CERT_PROFILE, (union params_cmd*)u, sd) == 0) {
			create_profile_done = true;
			shell_info(shell, "Created cert profile: errno %d  sd %d", errno, sd);
		} else {
			shell_error(shell, "Failed to create cert profile, errno: %d sd %d", errno, sd);
			ret = -errno;
			return ret;
		}

		int profile_id, client_verify, peer_verify;
		profile_id = DTLS_ECHO_SERVER_CLIENT_PROFILE_ID;
		ret = setsockopt(sd, SOL_TLS, TLS_MURATA_USE_PROFILE,
		&profile_id, sizeof(int) );
		client_verify = 1;
		ret = setsockopt(sd, SOL_TLS, TLS_MURATA_CLIENT_VERIFY,
		&client_verify, sizeof(int) );
		peer_verify = 0;
		ret = setsockopt(sd, SOL_TLS, TLS_PEER_VERIFY,
		&peer_verify, sizeof(int) );
		if (ret < 0) {
			shell_error(shell, "Failed to set TLS_MURATA_USE_PROFILE errno: %d sd %d",
			errno, sd);
			ret = -errno;
		} else {
			shell_info(shell, " set TLS_MURATA_USE_PROFILE sd %d", sd);
		}
	} else if (strcmp(operation, "d") == 0) {
		if (tmo_set_modem(DELETE_CERT_PROFILE, (union params_cmd*)u, sd) == 0) {
			create_profile_done = false;
			shell_info(shell, "Deleted cert profile: errno %d  sd %d", errno, sd);
		} else {
			shell_error(shell, "Failed to delete cert profile: errno %d sd %d", errno, sd);
			ret = -errno;
		}
	}
	return ret;
}

int udp_create_dtls(const struct shell *shell, size_t argc, char **argv)
{
	return udp_create_dtls_core(shell, argc, argv, AF_INET);
}

int udp_create_dtlsv6(const struct shell *shell, size_t argc, char **argv)
{
	return udp_create_dtls_core(shell, argc, argv, AF_INET6);
}
#endif

int sock_cmd_parent_bm;

int sock_connect(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 4){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int ret;
	char *host;
	struct sockaddr target;
	int sd = strtol(argv[1], NULL, 10);
	char *port_st = argv[3];
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	if (!(socks[sock_idx].flags & sock_cmd_parent_bm)) {
		shell_warn(shell, "Warning: Socket %d is a %s socket",
				sd, (socks[sock_idx].flags & (BIT(sock_udp) | BIT(sock_dtls))) ? "UDP": "TCP");
	}

	host = argv[2];
	int ai_family = (socks[sock_idx].flags & BIT(sock_v6)) ? AF_INET6 : AF_INET;
	// ret = net_addr_pton(AF_INET, host, &target.sin_addr);
	/* Workaround for now, will implement more advanced logic later */
	if (socks[sock_idx].flags & (BIT(sock_tls) | BIT(sock_dtls))){
		int peer_verify_val = TLS_PEER_VERIFY_NONE;
		ret = setsockopt(sd, SOL_TLS, TLS_PEER_VERIFY,
				&peer_verify_val, sizeof(peer_verify_val));
	}
	ret = net_ipaddr_parse(host, strlen(host), &target);
	if (!ret) {	//try dns
		/*  dns stuff */
		static struct addrinfo hints;
		struct zsock_addrinfo *res;

		hints.ai_family = ai_family;
		hints.ai_socktype = SOCK_STREAM;
		// shell_warn(shell, "NOT IP, might be URL");

		int devid = 0;
			// SD: iface=%d proto=<TLS/TCP/UDP> <CONNECTED> <BOUND>
		if (socks[sock_idx].flags & BIT(sock_open)) {
			struct net_if *iface = socks[sock_idx].dev;
			if (strncmp(iface->if_dev->dev->name, "murata", 6) == 0) {
				devid = MODEM_ID;
			} else if (strncmp(iface->if_dev->dev->name, "rs9116", 6) == 0) {
				devid = WIFI_ID;
			} else {
				shell_error(shell, "Unknown interface: %s", iface->if_dev->dev->name);
				return -EINVAL;
			}
		}
		if (devid == 0) {
			shell_error(shell, "Device not found");
			return -EINVAL;
		}

		ret = tmo_offload_init(devid);
		if (ret != 0) {
			shell_error(shell, "Could not init device");
			return ret;
		}
		ret = zsock_getaddrinfo(host, port_st, &hints, &res);
		if (ret != 0) {
			shell_error(shell, "Unable to resolve address (%s), quitting", host);
			return ret;
		}
		dump_addrinfo(shell, res);
		{
			char tmp1[NET_IPV6_ADDR_LEN];
			void * src;
			if (ai_family == AF_INET){
				src = &net_sin(res->ai_addr)->sin_addr;
			} else {
				src = &net_sin6(res->ai_addr)->sin6_addr;
			}

			zsock_inet_ntop(ai_family, src, tmp1, sizeof(tmp1));
			shell_print(shell, "DNS to conn, addr: %s\n", tmp1);
		}
		if (socks[sock_idx].flags & (BIT(sock_tls) | BIT(sock_dtls))){
			ret = setsockopt(sd, SOL_TLS, TLS_HOSTNAME,
					host, strlen(host));
		}

		ret = zsock_connect(sd, res->ai_addr, res->ai_addrlen);
		zsock_freeaddrinfo(res);
	} else {
		if (target.sa_family != ai_family) {
			shell_error(shell, "Socket %d is a %s socket, got %s address",
					sock_idx,
					(ai_family == AF_INET ? "IPV4" : "IPV6"),
					(ai_family == AF_INET ? "IPV6" : "IPV4")
				   );
			return -EINVAL;
		}
		uint16_t port = (uint16_t)strtol(port_st, NULL, 10);
		if (port) {
			if (target.sa_family == AF_INET) {
				net_sin(&target)->sin_port = htons(port);	
			}
#if IS_ENABLED(CONFIG_NET_IPV6)
			else {
				net_sin6(&target)->sin6_port = htons(port);	
			}
#endif
		}
		ret = zsock_connect(sd, &target,
				target.sa_family == AF_INET6 ? sizeof(struct sockaddr_in6)
				: sizeof(struct sockaddr_in));
	}

	if (ret == -1) {
		shell_error(shell, "Connection failed, errno = %d", errno);
		return ret;
	}
	socks[sock_idx].flags |= BIT(sock_connected);
	shell_print(shell, "Connected socket %d", sd);
	return 0;
}

int sock_bind(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 4){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int sd = strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	struct sockaddr_in target;
	net_addr_pton(AF_INET, argv[2], &target.sin_addr);
	target.sin_family = AF_INET;
	uint16_t port = (uint16_t)strtol(argv[3], NULL, 10);
	target.sin_port = htons(port);
	int cstat = zsock_bind(sd, (struct sockaddr *)&target, sizeof(target));
	if (cstat == -1) {
		shell_error(shell, "Bind failed, errno = %d", errno);
		return 0;
	}
	socks[sock_idx].flags |= BIT(sock_bound);
	return 0;
}

int sock_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	int sd = strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	if (!(socks[sock_idx].flags & sock_cmd_parent_bm)) {
		shell_warn(shell, "Warning: Socket %d is a %s socket",
				sd, (socks[sock_idx].flags & (BIT(sock_udp) | BIT(sock_dtls))) ? "UDP": "TCP");
	}
	int stat = zsock_send(sd, argv[2], strlen(argv[2]), 0);
	if (stat == -1) {
		shell_error(shell, "Send failed, errno = %d", errno);
		return -EINVAL;
	}
	return 0;
}

int sock_sendto(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 5){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	int sd = strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	struct sockaddr target;
	char *host = argv[2];
	char *port_st = argv[3];
	int ai_family = (socks[sock_idx].flags & BIT(sock_v6)) ? AF_INET6 : AF_INET;
	int ret = net_ipaddr_parse(host, strlen(host), &target);
	if (ret) {
		if (target.sa_family != ai_family) {
			shell_error(shell, "Socket %d is a %s socket, got %s address",
					sock_idx,
					(ai_family == AF_INET ? "IPV4" : "IPV6"),
					(ai_family == AF_INET ? "IPV6" : "IPV4")
				   );
			return -EINVAL;
		}
		uint16_t port = (uint16_t)strtol(port_st, NULL, 10);
		if (ai_family == AF_INET) {
			net_sin(&target)->sin_family = AF_INET;
			net_sin(&target)->sin_port = htons(port);
		}
#if IS_ENABLED(CONFIG_NET_IPV6)
		else {
			net_sin6(&target)->sin6_family = AF_INET6;
			net_sin6(&target)->sin6_port = htons(port);
		}
#endif
	} else {
		static struct addrinfo hints;
		struct zsock_addrinfo *res;

		hints.ai_family = ai_family;
		hints.ai_socktype = SOCK_DGRAM;
		int devid = 0;
		struct net_if *iface = socks[sock_idx].dev;
		if (strstr(iface->if_dev->dev->name, "murata")) {
			devid = MODEM_ID;
		} else if (strstr(iface->if_dev->dev->name, "9116")) {
			devid = WIFI_ID;
		} else {
			shell_error(shell, "Unknown interface: %s", iface->if_dev->dev->name);
			return -EINVAL;
		}
		if (devid == 0) {
			shell_error(shell, "Device not found");
			return -EINVAL;
		}

		ret = tmo_offload_init(devid);
		if (ret != 0) {
			shell_error(shell, "Could not init device");
			return ret;
		}
		ret = zsock_getaddrinfo(host, port_st, &hints, &res);
		if (ret != 0) {
			shell_error(shell, "Unable to resolve address (%s), quitting", host);
			return ret;
		}
		dump_addrinfo(shell, res);
		memcpy(&target, res->ai_addr, res->ai_addrlen);
		zsock_freeaddrinfo(res);
	}
	int stat = zsock_sendto(sd, argv[4], strlen(argv[4]), 0, &target, ai_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
	if (stat == -1) {
		shell_error(shell, "Send failed, errno = %d", errno);
	}
	return 0;
}

/**
 * send auto-generated bulk data
 */
int sock_sendb(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	int sd = strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	if (!(socks[sock_idx].flags & sock_cmd_parent_bm)) {
		shell_warn(shell, "Warning: Socket %d is a %s socket",
				sd, (socks[sock_idx].flags & (BIT(sock_udp) | BIT(sock_dtls))) ? "UDP": "TCP");
	}
	int sendsize = strtol(argv[2], NULL, 10);
	sendsize = MIN(sendsize, XFER_SIZE);

	gen_payload(mxfer_buf, sendsize);
	mxfer_buf[sendsize] = 0;
	int total = 0;
	int stat = 0;
	while (total < sendsize || sendsize == 0) {
		stat = zsock_send(sd, mxfer_buf + total, MIN(sendsize - total, max_fragment), 0);
		if (stat == -1) {
			if (errno == EMSGSIZE) {
				shell_warn(shell, "Note: EMSGSIZE (errno=%d) may be cause by a fragment being larger than network MTU.", EMSGSIZE);
			}
			shell_error(shell, "send failed, errno = %d", errno);
			break;
		}
		total += stat;
	}
	shell_info(shell, "sent %d", total);
	return (stat < 0) ? stat : total;
}

int sock_mxfragment(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		max_fragment = MAX(1, MIN(1500, strtol(argv[1], NULL, 10)));
	}
	shell_print(shell, "Max xfer fragment size set to %d bytes", max_fragment);
	return 0;
}

static int cmp_payload(const struct shell *shell, uint8_t *buf, int len)
{
#define MARGIN 5	//print partial left/right adjustment
	int stat = 0;
	int left_os, right_os;
	char hexdump[50] = {0};
	char chardump[10];

	for (int i=0;i<len;i++) {
		if (buf[i] != (0x20 + (i % 97))) {
			left_os = MAX(0, i - MARGIN);
			right_os = MIN(len - 1, i + MARGIN);

			for (int j = left_os; j <= right_os; j++) {
				snprintf(chardump, sizeof(chardump), "%02x ", buf[j]);
				strncat(hexdump, chardump, sizeof(hexdump)-1);
			}

			shell_error(shell, "buffer mismatch: buf[%d] = 0x%x, should be 0x%x, buf from [%d]: %s",
					i, buf[i], (0x20+i%97), left_os, hexdump);

			stat = -1;
			break;
		}
	}
	if (stat == 0) {
		shell_info(shell, "buffer matched with send buffer");
	}

	return stat;
}

/**
 * recv bulk data
 */
int sock_recvb(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	int sd = strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	if (!(socks[sock_idx].flags & sock_cmd_parent_bm)) {
		shell_warn(shell, "Warning: Socket %d is a %s socket",
				sd, (socks[sock_idx].flags & (BIT(sock_udp) | BIT(sock_dtls))) ? "UDP": "TCP");
	}
	int recvsize = strtol(argv[2], NULL, 10);
	recvsize = MIN(recvsize, XFER_SIZE);

	memset(mxfer_buf, 0, recvsize+1);
	int total = 0;
	int stat = 0;
	bool again = true;
	while (total < recvsize || recvsize == 0) {
		stat = zsock_recv(sd, mxfer_buf + total, MIN(recvsize - total, max_fragment), ZSOCK_MSG_DONTWAIT);
		if (stat == -1) {
			if ((total == 0) || (errno != EAGAIN)) {
				shell_error(shell, "recv failed, errno = %d", errno);
			} else if (total < recvsize && again) {
				k_msleep(500);
				again = false;
				continue;
			}
			break;
		}
		again = true;
		total += stat;
	}
	shell_info(shell, "received %d", total);

	if (total > 0) {
		cmp_payload(shell, mxfer_buf, total);
	}
	return (stat < 0) ? stat : total;
}

int sock_rcv(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int sd = (int)strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	if (!(socks[sock_idx].flags & sock_cmd_parent_bm)) {
		shell_warn(shell, "Warning: Socket %d is a %s socket",
				sd, (socks[sock_idx].flags & (BIT(sock_udp) | BIT(sock_dtls))) ? "UDP": "TCP");
	}
	int stat = 0;
	memset(mxfer_buf, 0, XFER_SIZE);
	stat = zsock_recv(sd, mxfer_buf, XFER_SIZE, ZSOCK_MSG_DONTWAIT);
	if (stat > 0){
		shell_print(shell, "RECEIVED:\n%s ", (char*)mxfer_buf);
	} else if (stat == -1 && errno == EWOULDBLOCK) {
		shell_print(shell, "No data available!");
		return stat;
	}
	while (stat == XFER_SIZE) {
		memset(mxfer_buf,0,XFER_SIZE);
		stat = zsock_recv(sd, mxfer_buf, XFER_SIZE, ZSOCK_MSG_DONTWAIT);
		shell_print(shell, "%s", (char*)mxfer_buf);
	}
	if (stat == -1) {
		shell_error(shell, "Receive failed, errno = %d", errno);
	} else if (stat == 0) {
		shell_error(shell, "Receive failed, socket closed!");
	}
	return stat;
}

int sock_rcvfrom(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int sd = (int)strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	struct sockaddr target;
	int addrLen;
	int stat = 0;
	int ai_family = (socks[sock_idx].flags & BIT(sock_v6)) ? AF_INET6 : AF_INET;
	addrLen = (ai_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
	memset(mxfer_buf, 0, XFER_SIZE);
	char addrbuf[NET_IPV6_ADDR_LEN];
	stat = zsock_recvfrom(sd, mxfer_buf, XFER_SIZE, ZSOCK_MSG_DONTWAIT, (struct sockaddr*)&target, &addrLen);
#if IS_ENABLED(CONFIG_NET_IPV6)
	void *addr = (ai_family == AF_INET6) ? (void*)&net_sin6(&target)->sin6_addr : (void*)&net_sin(&target)->sin_addr;
	uint16_t port = ntohs((ai_family == AF_INET6) ? net_sin6(&target)->sin6_port : net_sin(&target)->sin_port);
#else
	void *addr = (void*)&net_sin(&target)->sin_addr;
	uint16_t port = ntohs(net_sin(&target)->sin_port);
#endif
	net_addr_ntop(ai_family, addr, addrbuf, sizeof(addrbuf));
	if (stat > 0){
		shell_print(shell, "RECEIVED from %s:%d:\n%s ",  addrbuf, port, (char*)mxfer_buf);
	}  else if (stat == -1 && errno == EWOULDBLOCK) {
		shell_print(shell, "No data available!");
		return stat;
	}
	while (stat == XFER_SIZE) {
		memset(mxfer_buf,0,XFER_SIZE);
		stat = zsock_recv(sd, mxfer_buf, XFER_SIZE, ZSOCK_MSG_DONTWAIT);
		shell_print(shell, "%s", (char*)mxfer_buf);
	}
	if (stat == -1) {
		shell_error(shell, "Receive failed, errno = %d", errno);
	}
	return stat;
}

int sock_close(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int sd = strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC) {
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	if (!(socks[sock_idx].flags & sock_cmd_parent_bm)) {
		shell_warn(shell, "Warning: Socket %d is a %s socket",
				sd, (socks[sock_idx].flags & (BIT(sock_udp) | BIT(sock_dtls)) ? "UDP": "TCP"));
	}
	int stat = zsock_close(sd);
	if (stat < 0) {
		shell_error(shell, "Close failed, errno = %d", errno);
		return stat;
	}
	socks[sock_idx].flags &= ~BIT(sock_open);
	return stat;
}

#if CONFIG_MODEM
int sock_sendsms(const struct shell *shell, size_t argc, char **argv)
{
	int ret;
	struct sms_out sms;

	if (argc != 4){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	int sd = strtol(argv[1], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	// shell_print(shell, "About to call fcntl to send sms, phone: %s, msg: %s\n", argv[2], argv[3]);
	snprintf(sms.phone, SMS_PHONE_MAX_LEN, "%s", argv[2]);
	snprintf(sms.msg, CONFIG_MODEM_SMS_OUT_MSG_MAX_LEN + 1, "%s", argv[3]);
	ret = fcntl_ptr(sock_idx, SMS_SEND, &sms);
	// printf("returned from fcntl, ret = %d\n", ret);
	return ret;
}

int sock_recvsms(const struct shell *shell, size_t argc, char **argv)
{
	int ret;
	struct sms_in sms;


	if (argc != 3){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}

	int sd = strtol(argv[1], NULL, 10);

	int wait = strtol(argv[2], NULL, 10);
	int sock_idx;
	for (sock_idx = 0; sock_idx < MAX_SOCK_REC; sock_idx++) {
		if (socks[sock_idx].sd == sd && socks[sock_idx].flags & BIT(sock_open)) {
			break;
		}
	}
	if (sock_idx == MAX_SOCK_REC){
		shell_error(shell, "Socket %d not found", sd);
		return -EINVAL;
	}
	sms.timeout = K_SECONDS(wait);
	ret = fcntl_ptr(sock_idx, SMS_RECV, &sms);
	if (ret > 0)
		shell_print(shell, "Received SMS from %s at %s: %s\n", sms.phone, sms.time, sms.msg);
	else
		shell_print(shell, "No SMS received!");
	return ret;
}
#endif /* CONFIG_MODEM */
int cmd_list_socks(const struct shell *shell, size_t argc, char **argv)
{
	enum proto_idx {
		proto_idx_tcp = 0,
		proto_idx_udp,
		proto_idx_tls,
		proto_idx_dtls,
		proto_idx_unknown
	};
	char *protos[5] = {"TCP", "UDP", "TLS", "DTLS", "?"};
	int proto_x = 0;
	shell_print(shell, "Open sockets: ");
	for (int i = 0; i < MAX_SOCK_REC; i++) {
		// SD: iface=%d proto=<TLS/TCP/UDP> <CONNECTED> <BOUND>
		if (socks[i].flags & BIT(sock_open)) {
			if (socks[i].flags & BIT(sock_tls)) {
				proto_x = proto_idx_tls;
			} else if (socks[i].flags & BIT(sock_dtls)) {
				proto_x = proto_idx_dtls;
			} else if (socks[i].flags & BIT(sock_tcp)) {
				proto_x = proto_idx_tcp;
			} else if (socks[i].flags & BIT(sock_udp)) {
				proto_x = proto_idx_udp;
			} else {
				proto_x = proto_idx_unknown;
			}
			struct net_if *iface = socks[i].dev;
			shell_print(shell, "%d: iface=%s proto=%s %s%s%s",
					socks[i].sd,
					iface->if_dev->dev->name,
					protos[proto_x],
					(socks[i].flags & BIT(sock_connected)) ? "CONNECTED, " : "",
					(socks[i].flags & BIT(sock_bound)) ? "BOUND" : "",
					(socks[i].flags & BIT(sock_v6)) ? "V6" : ""
				   );
		}
	}
	return 0;
}

int ifcount;
void iface_cb(struct net_if *iface, void *user_data)
{
	const struct shell *shell = (struct shell*)user_data;
	shell_print(shell, "%d: %s", ifcount++, iface->if_dev->dev->name);
}

int cmd_list_ifaces(const struct shell *shell, size_t argc, char **argv)
{
	ifcount = 1;
	net_if_foreach(iface_cb, (void *)shell);
	return 0;
}

#if CONFIG_MODEM
void shell_help_modem(const struct shell *shell)
{
	shell_print(shell, "tmo modem <iface> <cmd_str>\n"
			"                  <cmd_str>: apn | awake | conn_sts | edrx | golden | iccid | imei | imsi |\n"
			"                             ip | ip6 | msisdn | psm | ptw | sim | sleep | ssi | version | wake"
			);
}

#define MAX_CMD_BUF_SIZE	256
char cmd_buf[MAX_CMD_BUF_SIZE];

static inline void strupper(char *p)
{
	while (*p)
		if (*p <= 'z' && *p >= 'a')
			*p++ &= 0xdf;
		else
			p++;
}

int cmd_modem(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3){
		shell_error(shell, "Missing required arguments");
		shell_help_modem(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", idx);
		return -EINVAL;
	}
	if (!strstr(iface->if_dev->dev->name, "murata")) {
		shell_error(shell, "dev - %s Not Supported; only Murata 1SC is supported", iface->if_dev->dev->name);
		return -EINVAL;
	}
	int sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);
	if (sd == -1) {
		shell_error(shell, "No sockets available, errno = %d", errno);
		return 0;
	}

	if (!strcmp(argv[2], "edrx")) {
		process_cli_cmd_modem_edrx(shell, argc, argv, sd);
	} else if (!strcmp(argv[2], "ptw")) {
		process_cli_cmd_modem_edrx_ptw(shell, argc, argv, sd);
	} else if (!strcmp(argv[2], "psm")) {
		process_cli_cmd_modem_psm(shell, argc, argv, sd);
	} else {
		strcpy(cmd_buf, argv[2]);
		strupper(cmd_buf);
		int res = fcntl_ptr(sd, GET_ATCMD_RESP, cmd_buf);
		if (res < 0) {
			shell_error(shell, "request: %s failed, error: %d\n", argv[2], res);
		} else if (cmd_buf[0] == 0) {
			shell_error(shell, "request: %s, response: <none>\n", argv[2]);
		} else {
			shell_print(shell, "request: %s, response: %s\n", argv[2], cmd_buf);
		}
	}

	int stat = zsock_close(sd);
	if (stat < 0) {
		shell_error(shell, "Close failed, errno = %d", errno);
		return stat;
	}
	return (cmd_buf[0] ? 0 : -1 );
}

#endif /* CONFIG_MODEM */

int tcp_connect(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_tcp) | BIT(sock_tls);
	return sock_connect(shell, argc, argv);
}

int tcp_send(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_tcp) | BIT(sock_tls);
	return sock_send(shell, argc, argv);
}

int tcp_rcv(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_tcp) | BIT(sock_tls);
	return sock_rcv(shell, argc, argv);
}

int tcp_sendb(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_tcp) | BIT(sock_tls);
	return sock_sendb(shell, argc, argv);
}

int tcp_recvb(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_tcp) | BIT(sock_tls);
	return sock_recvb(shell, argc, argv);
}

int tcp_close(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_tcp) | BIT(sock_tls);
	return sock_close(shell, argc, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_tcp_sub,
		SHELL_CMD(close, NULL, "<socket>", tcp_close),
		SHELL_CMD(connect, NULL, "<socket> <ip> <port>", tcp_connect),
		SHELL_CMD(create, NULL, "<iface>", tcp_create),
#if IS_ENABLED(CONFIG_NET_IPV6)
		SHELL_CMD(createv6, NULL, "<iface>", tcp_createv6),
#endif
		SHELL_CMD(recv, NULL, "<socket>", tcp_rcv),
		SHELL_CMD(recvb, NULL,  "<socket> <size>", tcp_recvb),
#if CONFIG_MODEM
		SHELL_CMD(recvsms, NULL, "<socket> <wait time (seconds)>", sock_recvsms),
#endif /* CONFIG_MODEM */
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
		SHELL_CMD(secure_create, NULL, "<iface>", tcp_create_tls),
#if IS_ENABLED(CONFIG_NET_IPV6)
		SHELL_CMD(secure_createv6, NULL, "<iface>", tcp_create_tlsv6),
#endif
#endif
		SHELL_CMD(send, NULL, "<socket> <payload>", tcp_send),
		SHELL_CMD(sendb, NULL,  "<socket> <size>", tcp_sendb),
#if CONFIG_MODEM
		SHELL_CMD(sendsms, NULL, "<socket> <phone number> <message>", sock_sendsms),
#endif /* CONFIG_MODEM */
		SHELL_CMD(xfersz, NULL,  "[size]", sock_mxfragment),
		SHELL_SUBCMD_SET_END
		);

int udp_connect(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_udp) | BIT(sock_dtls);
	return sock_connect(shell, argc, argv);
}

int udp_send(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_udp) | BIT(sock_dtls);
	return sock_send(shell, argc, argv);
}

int udp_rcv(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_udp) | BIT(sock_dtls);
	return sock_rcv(shell, argc, argv);
}

int udp_sendb(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_udp) | BIT(sock_dtls);
	return sock_sendb(shell, argc, argv);
}

int udp_recvb(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_udp) | BIT(sock_dtls);
	return sock_recvb(shell, argc, argv);
}

int udp_close(const struct shell *shell, size_t argc, char **argv)
{
	sock_cmd_parent_bm = BIT(sock_udp) | BIT(sock_dtls);
	return sock_close(shell, argc, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_udp_sub,

#if defined(CONFIG_NET_SOCKETS_ENABLE_DTLS)
		SHELL_CMD(cert, NULL, "<w or d>  <socket>", udp_cred_dtls),
		SHELL_CMD(ca, NULL, "<w or d>  <socket>",  udp_cred_dtls),
#endif
		SHELL_CMD(close, NULL, "<socket>", udp_close),
		SHELL_CMD(connect, NULL, "<socket> <ip> <port>", udp_connect),
		SHELL_CMD(create, NULL, "<iface>", udp_create),
#if IS_ENABLED(CONFIG_NET_IPV6)
		SHELL_CMD(createv6, NULL, "<iface>", udp_createv6),
#endif
		// SHELL_CMD(bind, NULL, "<socket> <ip> <port>", sock_bind),
#if defined(CONFIG_NET_SOCKETS_ENABLE_DTLS)
		SHELL_CMD(key, NULL, "<w or d>  <socket>",  udp_cred_dtls),
		SHELL_CMD(profile, NULL, "<a or d>  <socket>",  udp_profile_dtls),
		SHELL_CMD(secure_create, NULL, "<iface>", udp_create_dtls),
#if IS_ENABLED(CONFIG_NET_IPV6)
		SHELL_CMD(secure_createv6, NULL, "<iface>", udp_create_dtlsv6),
#endif
#endif
		SHELL_CMD(recv, NULL, "<socket>", udp_rcv),
		SHELL_CMD(recvb, NULL,  "<socket> <size>", udp_recvb),
		SHELL_CMD(recvfrom, NULL, "<socket> <ip> <port>", sock_rcvfrom),
#if CONFIG_MODEM
		SHELL_CMD(recvsms, NULL, "<socket> <wait time (seconds)>", sock_recvsms),
#endif /* CONFIG_MODEM */
		SHELL_CMD(send, NULL, "<socket> <payload>", udp_send),
		SHELL_CMD(sendb, NULL,  "<socket> <size>", udp_sendb),
#if CONFIG_MODEM
		SHELL_CMD(sendsms, NULL, "<socket> <phone number> <message>", sock_sendsms),
#endif /* CONFIG_MODEM */
		SHELL_CMD(sendto, NULL,  "<socket> <ip> <port> <payload>", sock_sendto),
		SHELL_CMD(xfersz, NULL,  "<size>", sock_mxfragment),
		SHELL_SUBCMD_SET_END
		);

void dump_addrinfo(const struct shell *shell, const struct addrinfo *ai)
{
	char mxfer[128];

	memset(mxfer, 0, sizeof(mxfer));

	if (ai->ai_family == AF_INET6){
		zsock_inet_ntop(ai->ai_family, &net_sin6(ai->ai_addr)->sin6_addr, mxfer, sizeof(mxfer));
	} else {
		zsock_inet_ntop(ai->ai_family, &net_sin(ai->ai_addr)->sin_addr, mxfer, sizeof(mxfer));
	}
	shell_print(shell, "addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
			"sa_family=%d, ntohs(sin_port)=%d, addr=%s",
			ai, ai->ai_family, ai->ai_socktype, ai->ai_protocol,
			ai->ai_addr->sa_family,
			ntohs(((struct sockaddr_in *)ai->ai_addr)->sin_port), (char*)mxfer);
}

int cmd_dnslookup(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3){
		shell_error(shell, "Missing required argument");
		shell_print(shell, "Usage: tmo dns <devid> <hostname> [service]\n"
				"       devid: 1 for modem, 2 for wifi\n");
		return -EINVAL;
	}

	uint8_t devid = strtol(argv[1], NULL, 10);
	int ret = tmo_offload_init(devid);
	if (ret != 0) {
		shell_error(shell, "Could not init device");
		return ret;
	}

	uint8_t *service = NULL;
	if (argc > 3) {
		service = argv[3];
	}

	struct zsock_addrinfo *res, *ai;
	int retval = zsock_getaddrinfo(argv[2], service, NULL, &res);
	if (retval != 0){
		shell_error(shell, "zsock_getaddrinfo failed (%d)", retval);
		return retval;
	}
	ai = res;

	while (ai) {
		memset(mxfer_buf, 0, 128);
		if (ai->ai_family == AF_INET6){
			zsock_inet_ntop(ai->ai_family, &net_sin6(ai->ai_addr)->sin6_addr, mxfer_buf, 128);
		} else {
			zsock_inet_ntop(ai->ai_family, &net_sin(ai->ai_addr)->sin_addr, mxfer_buf, 128);
		}
		dump_addrinfo(shell, ai);
		ai = ai->ai_next;
	}
	zsock_freeaddrinfo(res);
	return 0;
}

int cmd_http(const struct shell *shell, size_t argc, char **argv)
{
	int ret = -1;

	if ((argc < 3) || (argc > 4)) {
		shell_error(shell, "Missing required argument");
		shell_print(shell, "Usage: tmo http <devid> <URL> <file (optional)>\n"
				"       devid: 1 for modem, 2 for wifi\n");
		shell_help(shell);
		return -EINVAL;
	}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	if (!ca_cert_sz) {
		tls_credential_delete(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE);
		tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				dev_certificate, sizeof(dev_certificate));
	}
	// else {
	// 	tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
	// 			ca_cert, ca_cert_sz);
	// }
#endif

	/*
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
tls_credential_delete(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE);
tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
digicert_ca, sizeof(digicert_ca));
#endif
*/
	int devid = strtol(argv[1], NULL, 10);
	ret = tmo_http_download(devid, argv[2], (argc == 4) ? argv[3] : NULL, NULL);
	if (ret < 0) {
		shell_error(shell, "tmo_http_download returned %d", ret);
	}
	return ret;
}

#ifdef CONFIG_WIFI
SHELL_STATIC_SUBCMD_SET_CREATE(tmo_wifi_commands,
		SHELL_CMD(connect, NULL,
			"<iface> \"<SSID>\"\n<channel number (optional), "
			"0 means all>\n"
			"<PSK (optional: valid only for secured SSIDs)>",
			cmd_wifi_connect),
		SHELL_CMD(disconnect, NULL, "\"<iface>\"",
			cmd_wifi_disconnect),
		SHELL_CMD(mac, NULL, "\"<iface>\"",
			cmd_wifi_mac),
		SHELL_CMD(scan, NULL, "\"<iface>\"", cmd_wifi_scan),
		SHELL_CMD(status, NULL, "\"<iface>\"", cmd_wifi_status),
		SHELL_SUBCMD_SET_END
		);
#endif

int cmd_sntp(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo sntp <iface> <server>");
		return -EINVAL;
	}

	int status = -1;

	for (int i = 0; i < 3; i++) {
		status = tmo_update_time(shell, argv[2], strtol(argv[1], NULL, 10));
		if (!status || errno != -EAGAIN) {
			return status;
		}
	}
	return -1;
}
int cmd_gnss(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell,"Fix valid: %s", gnss_values.fix_valid ? "YES" : "NO");
	shell_print(shell,"Latitude: %.7lf",gnss_values.lat);
	shell_print(shell,"Longitude: %.7lf",gnss_values.lon);
	shell_print(shell,"Sats: %d",gnss_values.sats);
	shell_print(shell,"HDOP: %.2lf",gnss_values.hdop);
	shell_print(shell,"TTFF: %d seconds",gnss_values.timeToFix);
	shell_print(shell,"1PPS: %d",gnss_values.pps1Count);

	return 0;
}

int cmd_gnss_version(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = gnss_version();
	if (ret) {
		printf("%s:%d - Error reading GNSS version (%d)\n",__FUNCTION__, __LINE__, ret);
		return 1;
	}

	return 0;
}

void print_set_modem_edrx_usage(const struct shell *shell)
{
	shell_print(shell, "tmo modem <iface> edrx <mode> <Act-type> <edrx value>");
	shell_print(shell, "mode: 0 - off, 1 - on, 2 - unsolicited messages enabled");
	shell_print(shell, "Act-Type: 4- LTE,LTE-M,  5 - NB-IoT");
	shell_print(shell, "edrx values: 1 to 15");
}

void print_set_modem_psm_usage(const struct shell *shell)
{
	shell_print(shell, "tmo modem <iface> psm <mode> <T3412_Unit> <T3412_val> "
		"<T3324_Unit> <T3324_Val>");
	shell_print(shell, "mode: 0 - off, 1 - on, T3412_Unit: 0-7 T3412_Val: 0-31"
		" T3324_Unit: 0-7 T3324_Val: 0-31");
}

int process_cli_cmd_modem_psm(const struct shell *shell, size_t argc, char **argv, int sd)
{
	union params_cmd params_cmd_u;
	union params_cmd* u = &params_cmd_u;
	if (argc == 8) {
		// This is setting the PSM timer
		int mode = strtol(argv[3], NULL, 10);
		int t3412_mul = strtol(argv[4], NULL, 10);
		int t3412 = strtol(argv[5], NULL, 10);
		int t3324_mul = strtol(argv[6], NULL, 10);
		int t3324 = strtol(argv[7], NULL, 10);
		if ( (mode >= 0 && mode <=1) &&  (t3412_mul >= 0 && t3412_mul <= 7) &&
				((t3324_mul >= 0 && t3324_mul <= 2) || t3324_mul == 7)  &&
				(t3412 >=0 && t3412 <= 31) && (t3324 >=0 && t3324 <= 31) ) {
			u->psm.mode = mode;
			u->psm.t3412 = t3412 + (t3412_mul << 5);
			u->psm.t3324 = t3324 + (t3324_mul << 5);

			shell_print(shell, "Set PSM mode: %d,\n"
					"\tT3412 Unit: %d, T3412 Value: %d, T3324 Unit: %d, T3324 Value: %d",
					mode, t3412_mul, t3412, t3324_mul, t3324);
			tmo_set_modem(AT_MODEM_PSM_SET,(union params_cmd*) u, sd);
		} else {
			shell_print(shell, "Invalid inputs for PSM timer");
			print_set_modem_psm_usage(shell);
		}
	} else if (argc == 3) {
		tmo_set_modem(AT_MODEM_PSM_GET, (union params_cmd*) u,  sd);
		shell_print(shell, "%s", (char *)u);
	} else {
		// Invalid PSM command input
		print_set_modem_psm_usage(shell);
	}
	return 0;
}

int process_cli_cmd_modem_edrx(const struct shell *shell, size_t argc, char **argv, int sd)
{
	union params_cmd  params_cmd_u;
	union params_cmd* u = &params_cmd_u;
	if (argc == 6) {
		// This is setting the edrx timer
		u->edrx.mode = strtol(argv[3], NULL, 10);
		u->edrx.act_type = strtol(argv[4], NULL, 10);
		u->edrx.time_mask = strtol(argv[5], NULL, 10);
		if ( (u->edrx.mode >= 0 && u->edrx.mode <=2) && (u->edrx.act_type >=1 && u->edrx.act_type <=5) &&
				((u->edrx.time_mask >= 1 && u->edrx.time_mask <= 15))) {
			shell_print(shell, "Set eDRX mode: %d, act_type: %d, value: %d",
					u->edrx.mode, u->edrx.act_type, u->edrx.time_mask);
			tmo_set_modem(AT_MODEM_EDRX_SET, (union params_cmd*) u, sd);
		} else {
			shell_print(shell, "Invalid inputs for edrx timer");
			print_set_modem_edrx_usage(shell);
		}
	} else if (argc == 3) {
		tmo_set_modem(AT_MODEM_EDRX_GET, &params_cmd_u, sd);
		shell_print(shell, "%s", (char *)u);
	} else {
		// Invalid edrx command input
		print_set_modem_edrx_usage(shell);
	}
	return 0;
}

int process_cli_cmd_modem_edrx_ptw(const struct shell *shell, size_t argc, char **argv, int sd)
{
	int ptw = 0;
	if (argc == 4) {
		ptw = strtol(argv[3], NULL, 10);
		if (ptw >= 0 && ptw <= 15) {
			shell_print(shell, "Set eDRX PTW: %d", ptw);
			fcntl_ptr(sd, AT_MODEM_EDRX_PTW_SET, (const void*)&ptw);
		} else {
			shell_print(shell, "Invalid eDRX PTW value");
			shell_print(shell, "tmo modem <iface> ptw [ptw_value]");
		}
	} else if (argc == 3) {
		fcntl_ptr(sd, AT_MODEM_EDRX_PTW_GET, (const void*)&ptw);
		shell_print(shell, "PTW: %d", ptw);
	} else {
		shell_print(shell, "tmo modem <iface> ptw [ptw_value]");
	}
	return 0;
}

#if IS_ENABLED(CONFIG_BT_SMP)
SHELL_STATIC_SUBCMD_SET_CREATE(ble_smp_9116_toggles,
		SHELL_CMD(keyboard,   NULL, "Toggle Keyboard.", toggle_keyboard),
		SHELL_CMD(confirm,   NULL, "Toggle Confirm.", toggle_confirm),
		SHELL_CMD(display,   NULL, "Toggle Display.", toggle_display),
		SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(ble_smp_9116_respond,
		SHELL_CMD(key, NULL, "Send Passkey.", send_passkey),
		SHELL_CMD(confirm, NULL, "Send Confirm.", send_confirm),
		SHELL_CMD(cancel,   NULL, "Send cancel.", send_cancel),
		SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(ble_smp_9116_sub,
		SHELL_CMD(enable, NULL, "Enable Security Manager Protocol (SMP)", smp_enable),
		SHELL_CMD(disable, NULL, "Disable Security Manager Protocol (SMP)", smp_disable),
		SHELL_CMD(callbacks,   NULL, "Show enabled callbacks.", show_enabled),
		SHELL_CMD(toggle, &ble_smp_9116_toggles, "Toggle callbacks.", NULL),
		SHELL_CMD(respond, &ble_smp_9116_respond, "Send response.", NULL),
		SHELL_SUBCMD_SET_END
		);
#endif

#if IS_ENABLED(CONFIG_BT_PERIPHERAL)
#include "tmo_ble_demo.h"
SHELL_STATIC_SUBCMD_SET_CREATE(ble_adv_sub,
		SHELL_CMD(conn, NULL, "Advertise as connectable", cmd_ble_adv_conn),
		SHELL_CMD(eddystone, NULL, "Advertise as eddystone beacon", cmd_ble_adv_ebeacon),
		SHELL_CMD(ibeacon, NULL, "Advertise as ibeacon\n"
			"\tUsage:\n"
			"\t tmo ble adv ibeacon <uuid (optional)> <major (optional)>\n"
			"\t  <minor (optional)> <rssi at 1m (optional)>",
			cmd_ble_adv_ibeacon),
		SHELL_SUBCMD_SET_END
		);
#endif

int cmd_dfu_print_settings(const struct shell *shell, size_t argc, char **argv)
{
	char iface_name[10] = {0};
	int iface = get_dfu_iface_type();

	switch (iface) {
		case 1:
			strncpy(iface_name, "modem", sizeof(iface_name));
			break;
		case 2:
			strncpy(iface_name, "wifi", sizeof(iface_name));
			break;
		default:
			strncpy(iface_name, "unknown", sizeof(iface_name));
			break;
	}
	printf("Interface: %d (%s)\n", get_dfu_iface_type(), iface_name);
	printf("Base URL: %s\n", get_dfu_base_url());
	return 0;
}

int cmd_dfu_set_iface(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	return set_dfu_iface_type(idx);
}

int cmd_dfu_base_url(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "incorrect parameters");
		return -EINVAL;
	}
	return set_dfu_base_url(argv[1]);
}

int cmd_dfu_auth_key(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "incorrect parameters");
		return -EINVAL;
	}
	return set_dfu_auth_key(argv[1]);
}

int cmd_dfu_download(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo dfu download <target> [filename]\n"
				"       target: 0 for mcu, 1 for modem, 2 for wifi\n"
				"       filename(optional): base filename e.g tmo_shell.tmo_dev_edge");
		return -EINVAL;
	}
	int target = (int) strtol(argv[1], NULL, 10);
	if (target > 2) {
		shell_error(shell, "Only 3 targets supported (0-2)");
		return -EINVAL;
	}

	if (argc <= 2 && target == 1) { // have modem
		shell_error(shell, "There are no updates at this time");
		return -EINVAL;
	}

	if (argc <= 2 && target == 2) { // have wifi
		return tmo_dfu_download(shell, target, "rs9116w/RS9116W.2.7.0.0.39", argv[3]);
	}

#ifndef BOOT_SLOT
	if (target == 0) {
		shell_warn(shell, "Bootloader is not in use");
	}
#endif
	return tmo_dfu_download(shell, target, argv[2], argv[3]);
}

#ifdef BOOT_SLOT

static int cmd_get_current_slot(const struct shell *shell, size_t argc, char **argv)
{
	int slot = get_current_slot();

	if (slot >= 0) {
		shell_print(shell, "Current active slot is Slot %d", slot);
	} else {
		shell_error(shell, "Current active slot is undefined");
	}
	return slot;
}

static int cmd_get_unused_slot(const struct shell *shell, size_t argc, char **argv)
{
	int slot = get_unused_slot();

	if (slot >= 0) {
		shell_print(shell, "Unused/inactive slot  is Slot %d", slot);
	} else {
		shell_error(shell, "Unused/inactive slot is undefined");
	}

	return slot;
}

static int cmd_erase_slot(const struct shell *shell, size_t argc, char **argv)
{
	int force = 0;

	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo bootloader erase <slot #> [1 to force]\n"
				"       slot #: 0 for Slot 0, 1 for Slot 1\n");
		return -EINVAL;
	}

	int slot = strtol(argv[1], NULL, 10);
	if (argc == 3) {
		force = strtol(argv[2], NULL, 10);
	}

	if (force || slot_is_safe_to_erase(slot)) {
		int ret =  erase_image_slot(slot);
		if (ret == 0) {
			shell_print(shell, "Slot %d was erased", slot);
		}
		else {
			shell_error(shell, "Slot %d removal failed", slot);
			return -ENOEXEC;
		}
	}
	else {
		shell_error(shell, "Not safe to erase Slot %d", slot);
		return -ENOEXEC;
	}
	return 0;
}

static int cmd_print_slot_info(const struct shell *shell, size_t argc, char **argv)
{
	print_gecko_slot_info();
	return 0;
}

#endif

static int cmd_version(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "Built for: %s", VERSION_BOARD);
	shell_print(shell, "Zephyr:    %s", VERSION_ZEPHYR_BASE_TAG);
	shell_print(shell, "TMO RTOS:  %s", VERSION_TMO_RTOS_TAG);
	shell_print(shell, "TMO SDK:   %s", VERSION_TMO_SDK_TAG);
	return 0;
}

int cmd_dfu_get_version(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo dfu version <target>\n"
				"       target: 0 for mcu, 1 for modem, 2 for wifi");
		return -EINVAL;
	}

	int version_target = (int) strtol(argv[1], NULL, 10);
	switch (version_target)
	{
		case DFU_GECKO:
			{
#ifndef BOOT_SLOT
				shell_print(shell, "NOTE: Bootloader not in use");
#else
				char version[16];
				int slot = atoi(BOOT_SLOT);
				int ret = get_gecko_fw_version(slot, version, sizeof(version));
				if (ret) {
					shell_error(shell, "Could not read version in slot %d", slot);
				} else {
					shell_print(shell, "Active slot is %d, version: %s", slot, version);
				}
#endif
				cmd_version(shell, argc, argv);
			}
			break;

		case DFU_MODEM:
			{
				char dfu_modem_fw_version[DFU_MODEM_FW_VER_SIZE] = { 0 };
				int status = dfu_modem_get_version(dfu_modem_fw_version);
				if (status != 0) {
					shell_error(shell, "reading the Murata 1SC FW version failed\n");
				}
			}
			break;

		case DFU_9116W:
			{
				char dfu_rs9116w_fw_version[DFU_RS9116W_FW_VER_SIZE] = { 0 };
				int status;
				status = dfu_wifi_get_version(dfu_rs9116w_fw_version);
				if (status != 0) {
					shell_error(shell, "reading the RS9116W FW version failed\n");
				}
			}
			break;

		default:
			shell_error(shell, "Unknown DFU target\n");
			break;
	}
	return 0;
}

int cmd_dfu_update(const struct shell *shell, size_t argc, char **argv)
{
	int firmware_target = (int) strtol(argv[1], NULL, 10);
	int delta_firmware_target;
	char *dfu_modem_filename;
	struct dfu_file_t dfu_modem_file;

	if (firmware_target == DFU_GECKO)
		delta_firmware_target = (int) strtol(argv[2], NULL, 10);
	else if (firmware_target == DFU_MODEM)
		dfu_modem_filename = argv[2];

	if (((argc < 2) && (firmware_target != DFU_GECKO)) ||
			((argc != 3) && (firmware_target == DFU_GECKO))) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo dfu update <target>\n"
				"       target : 0 for mcu, 1 for modem, 2 for wifi/ble\n"
				"       mcu_slot(optional): slot to update. Applicable only to mcu target\n"
				"       Usage (mcu): \n"
				"                   tmo dfu update 0 [slot]\n"
				"       Usage (modem): \n"
				"                   tmo dfu update 1 [modem_delta_file]\n"
				"       Usage (wifi/ble): \n"
				"                   tmo dfu update 2\n");
		return -EINVAL;
	}

	switch (firmware_target)
	{
		case DFU_GECKO:
			{
#ifndef BOOT_SLOT
				shell_error(shell,"Can't program over currently running firmware");
				return -EINVAL;
#else
				if ((strcmp(BOOT_SLOT, "0") || strcmp(BOOT_SLOT, "1"))) {
					if (atoi(BOOT_SLOT) == delta_firmware_target) {
						shell_error(shell,"Can't program slot you are running from");
						return -EINVAL;
					}
				}

				int status;
				char bin_file[DFU_FILE_LEN];
				char sha_file[DFU_FILE_LEN];

				if (delta_firmware_target == 0){
					strcpy(bin_file, "/tmo/zephyr.slot0.bin");
					strcpy(sha_file, "/tmo/zephyr.slot0.bin.sha1");
				} else if (delta_firmware_target == 1) {
					strcpy(bin_file, "/tmo/zephyr.slot1.bin");
					strcpy(sha_file, "/tmo/zephyr.slot1.bin.sha1");
				} else {
					shell_error(shell, "Invalid slot number");
					return -EINVAL;
				}

				shell_print(shell,"Starting the FW update for SiLabs Pearl Gecko");
				status = dfu_mcu_firmware_upgrade(delta_firmware_target,bin_file,sha_file);
				if (status != 0) {
					shell_error(shell, "The FW update for SiLabs Pearl Gecko failed");
				}
				else {
					shell_print(shell, "The FW update for SiLabs Pearl Gecko is complete!");
				}
#endif
			}
			break;

		case DFU_MODEM:
			{
				shell_print(shell,"\nStarting the FW update for Murata 1SC");
				int status;
				sprintf(dfu_modem_file.desc, "Murata 1SC Firmware Update");
				sprintf(dfu_modem_file.lfile, "/tmo/%s.ua", dfu_modem_filename);
				sprintf(dfu_modem_file.rfile, "%s.ua", dfu_modem_filename);

				status = dfu_modem_firmware_upgrade(&dfu_modem_file);
				if (status != 0) {
					shell_error(shell, "The FW update of Murata 1SC failed");
				}
				else {
					shell_print(shell, "The FW update for Murata 1SC is complete!");
				}
			}
			break;

		case DFU_9116W:
			{
				shell_print(shell,"\nStarting the FW update for SiLabs RS9116W");
				int status;
				status = dfu_wifi_firmware_upgrade();
				if (status != 0) {
					shell_error(shell, "The FW update of SiLabs RS9116W failed");
				}
				else {
					shell_print(shell, "The FW update for SiLabs RS9116W is complete!");
				}

				shell_print(shell, "Sleeping 40 seconds before system reboot...");
				k_sleep(K_SECONDS(40));

				shell_print(shell, "Rebooting...");
				sys_reboot(SYS_REBOOT_WARM);
			}
			break;

		default:
			shell_error(shell, "Unknown DFU target\n");
			break;
	}
	return 0;
}

int cmd_charging_status(const struct shell *shell, size_t argc, char **argv)
{
	int status;
	uint8_t charging = 0;
	uint8_t vbus = 0;
	uint8_t attached = 0;
	uint8_t fault = 0;

	status = get_battery_charging_status(&charging, &vbus, &attached, &fault);
	if (status != 0) {
		shell_error(shell, "Charger VBUS status command failed");
	}
	else {
		if (!attached) {
			shell_print(shell, "\tNo battery attached");
		} else if (!vbus) {
			shell_print(shell, "\tCharger is missing VBUS and is NOT charging");
		}
		else {
			if (vbus && charging) {
				shell_print(shell, "\tCharger has VBUS and is charging");
			}
			else {
				if (vbus && !charging) {
					shell_print(shell, "\tCharger has VBUS and is done charging");
				}
			}
		}
	}
	return 0;
}

int cmd_battery_voltage(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t millivolts = 0;
	uint8_t battery_attached = 0;
	uint8_t charging = 0;
        uint8_t vbus = 0;
	uint8_t fault = 0;

	get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
	millivolts = read_battery_voltage();
	shell_print(shell, "\tBattery voltage %d.%03dV", millivolts/1000, millivolts%1000);

	return millivolts;
}

extern uint8_t aio_btn_pushed;
int cmd_battery_discharge(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t set_point = 60;
	uint8_t percent = 100;
	uint8_t old_percent = 0;
	uint32_t millivolts = 0;
	uint8_t battery_attached = 0;
	uint8_t charging = 0;
        uint8_t vbus = 0;
	uint8_t fault= 0;

	if (argc > 2) {
		shell_error(shell, "Incorrect parameters");
		shell_print(shell, "usage: tmo battery discharge [set point (optional, default: 60)]");
		return -1;
	} if (argc == 2) {
		int val = strtol(argv[1], NULL, 10);
		if (val > 100) {
			set_point = 100;
		} else if (val < 0) {
			set_point = 0;
		} else {
			set_point = (uint8_t) val;
		}
	}
	shell_print(shell, "Discharge setpoint: %d", set_point);

	get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
	if (battery_attached !=  0) {
		millivolts = read_battery_voltage();
		millivolts_to_percent(millivolts, &percent);
		old_percent = percent;
	} else {
		shell_error(shell, "Battery not attached, aborting...");
		return -ENOEXEC;
	}

	shell_print(shell, "Battery level is currently %d%%", percent);

	if (charging) {
		shell_warn(shell, "Battery is currently charging, unplug USB-C to discharge!");
	} else if (percent > set_point) {
		shell_print(shell, "Discharging battery, this may take a while...");
	}
	shell_print(shell, "Press user button to abort...");

	if (percent <= set_point) {
		shell_warn(shell, "Battery already at or below %d%% (%d%%)", set_point, percent);
	} else {
#ifdef LED_PWM_WHITE
		led_on(device_get_binding("pwmleds"), LED_PWM_WHITE);
#endif /* LED_PWM_WHITE */
		led_on(device_get_binding("pwmleds"), LED_PWM_RED);
		led_on(device_get_binding("pwmleds"), LED_PWM_GREEN);
		led_on(device_get_binding("pwmleds"), LED_PWM_BLUE);
	}

	while (percent > set_point) {
		get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
		if (battery_attached !=  0) {
			millivolts = read_battery_voltage();
			millivolts_to_percent(millivolts, &percent);
		} else {
			shell_error(shell, "Battery not attached, aborting...");
			return -ENOEXEC;
		}
		if ((abs(old_percent - percent) > 5) && ((percent % 5) == 0)) {
			shell_print(shell, "Battery level is now (%d%%)...", percent);
			old_percent = percent;
		}
		if (aio_btn_pushed) {
			shell_warn(shell, "Button pressed, battery discharging aborted!");
#ifdef LED_PWM_WHITE
			led_off(device_get_binding("pwmleds"), LED_PWM_WHITE);
#endif /* LED_PWM_WHITE */
			led_off(device_get_binding("pwmleds"), LED_PWM_RED);
			led_off(device_get_binding("pwmleds"), LED_PWM_GREEN);
			led_off(device_get_binding("pwmleds"), LED_PWM_BLUE);
			return -ENOEXEC;
		}
	}

	shell_print(shell, "Battery is discharged (%d%%), shutting down...", percent);
	cmd_pmsysfulloff(shell, 0, NULL);

	return 0;
}

int cmd_battery_percentage(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t percent = 0;
	uint32_t millivolts = 0;
	uint8_t battery_attached = 0;
	uint8_t charging = 0;
        uint8_t vbus = 0;
	uint8_t fault= 0;

	get_battery_charging_status(&charging, &vbus, &battery_attached, &fault);
	millivolts = read_battery_voltage();
	millivolts_to_percent(millivolts, &percent);
	shell_print(shell, "\tBattery level %d percent", percent);
	return 0;
}

int udp_cred_dtls(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo udp %s <operation> <socket>>\n"
				"   operation:w to write,  d to delete dtls credential from modem NVRAM\n"
				"	socket: socket descriptor", argv[0]);
		return -EINVAL;
	}
	return tmo_dtls_cred(shell, argv[0], argv[1], (int) strtol(argv[2], NULL, 10));
}

int udp_profile_dtls(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo udp profile <operation> <socket>>\n"
						"       operation:a to add,  d to delete cert profile from  modem NVRAM\n"
						"       socket: socket descriptor from - tmo udp secure_create 1 ");
		return -EINVAL;
	}
	return tmo_profile_dtls(shell, argv[1], (int) strtol(argv[2], NULL, 10));
}

int golden_check()
{
	char cmd_buf[64];
	struct net_if *iface = net_if_get_by_index(MODEM_ID);
	int sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);
	strcpy(cmd_buf, "golden");
	strupper(cmd_buf);
	int res = fcntl(sd, GET_ATCMD_RESP, cmd_buf);
	zsock_close(sd);
	if (res < 0) {
		printf("Modem firmware type detect failed (%d)\n", res);
		return 1;
	} else {
		printf("Modem firmware type %s\n", cmd_buf);
		return strcmp(cmd_buf, "GOLDEN");
	}
}

bool light_on;

int restore_led;
static void led_blnk_tmr_f(struct k_timer *timer_id)
{
	if (light_on) {
#ifdef LED_PWM_WHITE
			led_off(device_get_binding("pwmleds"), LED_PWM_WHITE);
#else
			led_off(device_get_binding("pwmleds"), LED_PWM_RED);
			led_off(device_get_binding("pwmleds"), LED_PWM_GREEN);
			led_off(device_get_binding("pwmleds"), LED_PWM_BLUE);
			led_on(device_get_binding("pwmleds"), restore_led);
#endif /* LED_PWM_WHITE */
	} else {
#ifdef LED_PWM_WHITE
			led_on(device_get_binding("pwmleds"), LED_PWM_WHITE);
#else
			led_on(device_get_binding("pwmleds"), LED_PWM_RED);
			led_on(device_get_binding("pwmleds"), LED_PWM_GREEN);
			led_on(device_get_binding("pwmleds"), LED_PWM_BLUE);
#endif /* LED_PWM_WHITE */
	}
	light_on = !light_on;
}

K_TIMER_DEFINE(led_blink_timer, led_blnk_tmr_f, NULL);

int cmd_mfg_test(const struct shell *shell, size_t argc, char **argv)
{
	int rc = 0;

	shell_print(shell, "Run mfg tests...");
	k_timer_stop(&led_blink_timer);

	/* Print slot info */
#ifndef BOOT_SLOT
	shell_print(shell, "Bootloader not in use");
#else
	print_gecko_slot_info();
#endif

	/* Print version of tmo_shell */
	cmd_version(shell, argc, argv);

	rc |= buzzer_test();
	k_sleep(K_SECONDS(1));

	rc |= led_test();
	k_sleep(K_SECONDS(2));

	rc |=misc_test();
#if CONFIG_TMO_TEST_MFG_CHECK_GOLDEN
	rc |= golden_check();
#endif /* CONFIG_TMO_TEST_MFG_CHECK_GOLDEN */

	const struct device * pwm_dev = device_get_binding("pwmleds");
	bool fw_test_passed = !fw_test();
	bool acc_code_test_passed = !ac_test();
	if (rc == 0 && fw_test_passed && acc_code_test_passed) {
		led_on(pwm_dev, LED_PWM_GREEN); // Green LED
		shell_fprintf(shell_backend_uart_get_ptr(), SHELL_INFO, "TESTS PASSED\n");
	} else if (rc == 0 && !fw_test_passed) {
		led_on(pwm_dev, LED_PWM_BLUE); // Blue LED
		shell_warn(shell, "FIRMWARE OUT OF DATE");
		restore_led = LED_PWM_BLUE;
		k_timer_start(&led_blink_timer, K_MSEC(500), K_SECONDS(1));
	} else if (rc == 0 && !acc_code_test_passed) {
		led_on(pwm_dev, LED_PWM_BLUE); // Blue LED
		shell_warn(shell, "ACCESS CODE FAILURE");
		restore_led = LED_PWM_BLUE;
		k_timer_start(&led_blink_timer, K_MSEC(1), K_SECONDS(1));
	} else {
		led_on(pwm_dev, LED_PWM_RED); // Red LED
		restore_led = LED_PWM_RED;
		shell_error(shell, "TESTS FAILED");
		k_timer_start(&led_blink_timer, K_MSEC(500), K_MSEC(500));
	}
	return 0;
}

int cmd_qa_test(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "Run QA test (TBD)...");
	return 0;
}

int cmd_json_set_iface(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
	return  set_json_iface_type(idx);
}

int cmd_json_transmit_enable(const struct shell *shell, size_t argc, char **argv)
{
	set_transmit_json_flag(true);
	return 0;
}

int cmd_json_transmit_interval(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_help(shell);
		return -EINVAL;
	}
	int secs = strtol(argv[1], NULL, 10);
	if (secs >= 1)
	{
		set_transmit_interval(secs);
		return 0;
		}
	return -1;
}

int cmd_json_transmit_disable(const struct shell *shell, size_t argc, char **argv)
{
	set_transmit_json_flag(false);
	return 0;
}

int cmd_json_print_payload(const struct shell *shell, size_t argc, char **argv)
{
	printf("\n%s\n", get_json_payload_pointer());
	return 0;
}

int cmd_json_print_settings (const struct shell *shell, size_t argc, char **argv)
{
	struct web_demo_settings_t ws;
	get_web_demo_settings(&ws);
	printf("Transmit: %s\nInterface %d\nNumber transmissions: %d\nTransmit interval: %d secs\n",
			ws.transmit_flag ? "ENABLED":"DISABLED",
			ws.iface_type,
			ws.number_http_requests,
			ws.transmit_interval);
	printf("Base URL: '%s'\n", get_json_base_url());
	printf("Path: '%s'\n", get_json_path());
	return 0;
}

int cmd_json_base_url(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "incorrect parameters");
		return -EINVAL;
	}
	return set_json_base_url(argv[1]);
}

int cmd_json_path(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(shell, "incorrect parameters");
		return -EINVAL;
	}
	return set_json_path(argv[1]);
}

/* LITTLEFS */
#ifdef CONFIG_FILE_SYSTEM_LITTLEFS
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_data);
static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &lfs_data,
	.storage_dev = (void *)FIXED_PARTITION_ID(storage_partition1),
};
#endif

#define MOUNT_POINT "/tmo"

static int mountfs()
{
	if (littlefs_mnt.mnt_point != NULL) {
		return -EBUSY;
	}

	littlefs_mnt.mnt_point = MOUNT_POINT;

	int rc = fs_mount(&littlefs_mnt);

	if (rc != 0) {
		printf("Error mounting as littlefs: %d\n", rc);
		return -ENOEXEC;
	}

	return rc;
}

int cmd_play_tone(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo buzzer tone <pitch in Hz> <duration in msecs>");
		return -1;
	}

	int frequency = strtol(argv[1], NULL, 10);
	int duration = strtol(argv[2], NULL, 10);
	tmo_play_tone(frequency, duration);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_json_sub,
		SHELL_CMD(base_url, NULL, "Set JSON base URL", cmd_json_base_url),
		SHELL_CMD(disable, NULL, "Disable JSON transmission", cmd_json_transmit_disable),
		SHELL_CMD(enable, NULL, "Enable JSON transmission", cmd_json_transmit_enable),
		SHELL_CMD(iface, NULL, "Set JSON iface", cmd_json_set_iface),
		SHELL_CMD(interval, NULL, "Set transmit interval (secs)", cmd_json_transmit_interval),
		SHELL_CMD(path, NULL, "Set JSON path part of URL", cmd_json_path),
		SHELL_CMD(payload, NULL, "Print JSON data", cmd_json_print_payload),
		SHELL_CMD(settings, NULL, "Print JSON settings", cmd_json_print_settings),
		SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_mfg_sub,
		SHELL_CMD(all, NULL, "Run all MFG tests", cmd_mfg_test),
		SHELL_CMD(buzzer, NULL, "Run buzzer tests", buzzer_test),
		SHELL_CMD(led, NULL, "Run LED tests", led_test),
		SHELL_CMD(misc, NULL, "Run misc tests", misc_test),
		SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_test_sub,
		SHELL_CMD(mfg, &tmo_mfg_sub, "Manufacturing", NULL),
		SHELL_CMD(qa, NULL, "Quality Assurance (TBD)", cmd_qa_test),
		SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_dfu_sub,
		SHELL_CMD(auth_key, NULL, "Set FW download auth key", cmd_dfu_auth_key),
		SHELL_CMD(base_url, NULL, "Set FW download base URL", cmd_dfu_base_url),
		SHELL_CMD(download, NULL, "Download FW", cmd_dfu_download),
		SHELL_CMD(iface, NULL, "Set FW download iface", cmd_dfu_set_iface),
		SHELL_CMD(settings, NULL, "Print DFU settings", cmd_dfu_print_settings),
		SHELL_CMD(update, NULL, "Update FW", cmd_dfu_update),
		SHELL_CMD(version, NULL, "Get current FW version", cmd_dfu_get_version),
		SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_battery_sub,
		SHELL_CMD(charger, NULL, "Get charger status", cmd_charging_status),
		SHELL_CMD(discharge, NULL, "Discharge battery for shipping", cmd_battery_discharge),
		SHELL_CMD(percent, NULL, "Get battery level (percent)", cmd_battery_percentage),
		SHELL_CMD(voltage, NULL, "Get battery level (Volts)", cmd_battery_voltage),
		SHELL_SUBCMD_SET_END
		);

#ifdef BOOT_SLOT
SHELL_STATIC_SUBCMD_SET_CREATE(tmo_bootloader_sub,
		SHELL_CMD(current, NULL, "Get current active slot", cmd_get_current_slot),
		SHELL_CMD(erase, NULL, "Erase a slot image", cmd_erase_slot),
		SHELL_CMD(images, NULL, "Get slot images info", cmd_print_slot_info),
		SHELL_CMD(unused, NULL, "Get unused slot", cmd_get_unused_slot),
		SHELL_SUBCMD_SET_END
		);
#endif

int cmd_tmo_buzzer_vol(const struct shell *shell, int argc, char**argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "  tmo buzzer vol <volume_percent>");
		return 1;
	}
	int vol = strtol(argv[1], NULL, 10);
	tmo_buzzer_set_volume(vol);
	shell_print(shell, "Volume set to %d%%", vol);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_buzzer_sub,
		SHELL_CMD(jingle, NULL, "Play TMO jingle", tmo_play_jingle),
		SHELL_CMD(ramp, NULL, "Play ramp tune", tmo_play_ramp),
		SHELL_CMD(tone, NULL, "Play a tone for a time", cmd_play_tone),
		SHELL_CMD(vol, NULL, "Set buzzer volume", cmd_tmo_buzzer_vol),
		SHELL_SUBCMD_SET_END
		);
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)

#if CONFIG_MODEM
int cmd_tmo_cert_modem_load(const struct shell* shell, int argc, char **argv)
{
	bool force = false;
	if (argc > 2) {
		shell_print(shell, "  usage: tmo certs modem_load [force]");
	}
	if (argc == 2) {
		force = strtol(argv[1], NULL, 10);
	}
	struct murata_cert_params cparams;
	struct tls_credential cert;
	char name[32];
	snprintk(name, sizeof(name), "cst_%d.pem", ca_cert_idx);
	cert.buf = ca_cert;
	cert.len = ca_cert_sz;
	cert.type = TLS_CREDENTIAL_CA_CERTIFICATE;
	cparams.filename = name;
	cparams.cert = &cert;
	int sock = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, net_if_get_by_index(MODEM_ID));
	if (sock == -1) {
		shell_error(shell, "Unable to open modem socket!");
		return EIO;
	}

	if (fcntl_ptr(sock, CHECK_CERT, name) == 0) {
		if (!force) {
			shell_error(shell, "Cert already loaded!");
			zsock_close(sock);
			return EIO;
		}
		fcntl_ptr(sock, DEL_CERT, name);
	}
	fcntl_ptr(sock, STORE_CERT, &cparams);
	zsock_close(sock);

	return 0;
}
#endif

int cmd_hwid(const struct shell* shell, int argc, char **argv)
{
	shell_print(shell, "HWID Value = %d", read_hwid());
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(certs_sub,
		SHELL_CMD(dld, NULL, "Download certificates", cmd_tmo_cert_dld),
		SHELL_CMD(info, NULL, "Print certificate", cmd_tmo_cert_info),
		SHELL_CMD(list, NULL, "List certificates", cmd_tmo_cert_list),
		SHELL_CMD(load, NULL, "Load certificate", cmd_tmo_cert_load),
#if CONFIG_MODEM
		SHELL_CMD(modem_load, NULL, "Send loaded certificate to modem", cmd_tmo_cert_modem_load),
#endif
		SHELL_SUBCMD_SET_END
		);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_ble_sub,
#if IS_ENABLED(CONFIG_BT_SMP)
		SHELL_CMD(smp, &ble_smp_9116_sub, "BLE SMP Controls", NULL),
#endif
#if IS_ENABLED(CONFIG_BT_PERIPHERAL)
		SHELL_CMD(adv, &ble_adv_sub, "BLE advertisement test controls", NULL),
		SHELL_CMD(rssi, NULL, "BLE peripheral RSSI read", cmd_ble_conn_rssi),
#endif
		SHELL_SUBCMD_SET_END
		);

#if CONFIG_PM_DEVICE
SHELL_DYNAMIC_CMD_CREATE(dsub_device_name, pm_device_name_get);
SHELL_STATIC_SUBCMD_SET_CREATE(sub_pm,
	SHELL_CMD_ARG(get, &dsub_device_name, "Get a device's power management state", cmd_pmget,
			2, 2),
	SHELL_CMD_ARG(lock, &dsub_device_name, "Lock a devices power management state", cmd_pmlock,
			2, 2),
	SHELL_CMD_ARG(off, &dsub_device_name, "Put a device into the off/unpowered state", cmd_pmoff,
			2, 2),
	SHELL_CMD_ARG(on, &dsub_device_name, "Put a device into the on/powered state", cmd_pmon,
			2, 2),
	SHELL_CMD_ARG(resume, &dsub_device_name, "Resume a device from the suspended state", cmd_pmresume,
			2, 2),
	SHELL_CMD_ARG(suspend, &dsub_device_name, "Put a device into the suspended state", cmd_pmsuspend,
			2, 2),
	SHELL_CMD_ARG(unlock, &dsub_device_name, "Unlock a devices power management state", cmd_pmunlock,
			2, 2),
	SHELL_SUBCMD_SET_END
	);
#endif


#if CONFIG_PM
SHELL_STATIC_SUBCMD_SET_CREATE(sub_sys_pm,
	SHELL_CMD(active, NULL, "Put a device into the active state", cmd_pmsysactive),
	SHELL_CMD(fulloff, NULL, "Put system into the off state (Without RTCC)", cmd_pmsysfulloff),
	SHELL_CMD(idle, NULL, "Put system into idle state", cmd_pmsysidle),
	SHELL_CMD(off, NULL, "Put system into the off state (Retaining RTCC)", cmd_pmsysoff),
	SHELL_CMD(standby, NULL, "Put system into the standby state", cmd_pmsysstandby),
	SHELL_CMD(suspend, NULL, "Put system into the suspend state", cmd_pmsyssuspend),
	SHELL_SUBCMD_SET_END
	);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(tmo_file_sub,
		SHELL_CMD(cp, NULL, "Copy a file", tmo_cp),
		SHELL_CMD(ll, NULL, "Detailed file list", tmo_ll),
		SHELL_CMD(mv, NULL, "Move a file", tmo_mv),
		SHELL_CMD(sha1, NULL, "Compute a file SHA1", cmd_sha1),
		SHELL_SUBCMD_SET_END
		);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tmo,
		SHELL_CMD(battery, &tmo_battery_sub, "Battery and charger status", NULL),
		SHELL_CMD(ble, &tmo_ble_sub, "BLE test commands", NULL),
#ifdef BOOT_SLOT
		SHELL_CMD(bootloader, &tmo_bootloader_sub, "Bootloader status", NULL),
#endif
		SHELL_CMD(buzzer, &tmo_buzzer_sub, "Buzzer tests", NULL),
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
		SHELL_CMD(certs, &certs_sub, "CA cert commands", NULL),
#endif
		SHELL_CMD(dfu, &tmo_dfu_sub, "Device FW updates", NULL),
		SHELL_CMD(dns, NULL, "Perform dns lookup", cmd_dnslookup),
		SHELL_CMD(file, &tmo_file_sub, "File commands", NULL),
		SHELL_CMD(gnssversion, NULL, "Get GNSS chip version", cmd_gnss_version),
		SHELL_CMD(http, NULL, "Get http URL", cmd_http),
		SHELL_CMD(hwid, NULL, "Read the HWID divider voltage", cmd_hwid),
		SHELL_CMD(ifaces, NULL, "List network interfaces", cmd_list_ifaces),
		SHELL_CMD(json, &tmo_json_sub, "JSON data options", NULL),
#if CONFIG_TMO_SHELL_BUILD_EK
		SHELL_CMD(kermit, NULL, "Embedded kermit", cmd_ekermit),
#endif
		SHELL_CMD(location, NULL, "Get latitude and longitude", cmd_gnss),
#if CONFIG_MODEM
		SHELL_CMD(modem, NULL, "Modem status and control", &cmd_modem),
#endif
#if CONFIG_PING
		SHELL_CMD(ping, NULL, "ICMP Ping", cmd_ping),
#endif
#if CONFIG_PM_DEVICE
		SHELL_CMD(pm, &sub_pm, "Device power management controls", NULL),
#endif
		SHELL_CMD(sntp, NULL, "Retrieve the current time", cmd_sntp),
		SHELL_CMD(sockets, NULL, "List open sockets", cmd_list_socks),
#if CONFIG_PM
		SHELL_CMD(sys_pm, &sub_sys_pm, "System power management controls", NULL),
#endif
		SHELL_CMD(tcp, &tmo_tcp_sub, "Send/recv TCP packets", NULL),
		SHELL_CMD(test, &tmo_test_sub, "Run automated tests", NULL),
		SHELL_CMD(udp, &tmo_udp_sub, "Send/recv UDP packets", NULL),
		SHELL_CMD(version, NULL, "Print version details", cmd_version),
#ifdef CONFIG_WIFI
		SHELL_CMD(wifi, &tmo_wifi_commands, "WiFi status and control", NULL),
#endif
		SHELL_SUBCMD_SET_END /* Array terminated. */
		);

		SHELL_CMD_REGISTER(tmo, &sub_tmo, "TMO Shell Commands", NULL);

static void count_ifaces(struct net_if *iface, void *user_data)
{
	num_ifaces++;
}

void tmo_shell_main(void)
{
	shell = shell_backend_uart_get_ptr();

	net_if_foreach(count_ifaces, NULL);
	ext_flash_dev = device_get_binding(FLASH_DEVICE);

	if (!ext_flash_dev) {
		shell_print(shell, "External flash driver %s was not found!", FLASH_DEVICE);
		exit(-1);
	}
	else {
		shell_print(shell, "External flash driver %s ready!", FLASH_DEVICE);
	}

	gecko_flash_dev = device_get_binding(GECKO_FLASH_DEVICE);
        if (!gecko_flash_dev) {
                shell_print(shell, "Gecko flash driver not found");
        }
        else {
                shell_print(shell, "Gecko flash driver %s ready!", GECKO_FLASH_DEVICE);
        }

	// mount the flash file system
	mountfs();

	cxd5605_init();
	initADC();
#ifdef CONFIG_WIFI
	tmo_wifi_connect();
#endif

#ifdef BOOT_SLOT
	shell_print(shell, "BOOT_SLOT: %s", BOOT_SLOT);
#endif
}
