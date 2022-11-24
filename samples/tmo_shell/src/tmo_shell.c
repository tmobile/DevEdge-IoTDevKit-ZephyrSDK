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

#include <logging/log.h>
LOG_MODULE_REGISTER(tmo_shell, LOG_LEVEL_INF);

#include <stdio.h>
#include <fs/fs.h>
#include <zephyr.h>
#include <fcntl.h>
#include <net/socket.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <shell/shell_help.h>
#include <net/http_client.h>
#include <drivers/led.h>
#include <sys/reboot.h>
#include <rsi_wlan_apis.h>

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include "tls_internal.h"
#include <net/tls_credentials.h>
#include "ca_certificate.h"
typedef int sec_tag_t;
#endif

#include "tmo_modem_psm.h"
#include "tmo_modem_edrx.h"

#if CONFIG_MODEM
#include <drivers/modem/murata-1sc.h>
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
#define FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))
#define FLASH_NAME "JEDEC SPI-NOR"
#elif (CONFIG_NORDIC_QSPI_NOR - 0) || \
	DT_NODE_HAS_STATUS(DT_INST(0, nordic_qspi_nor), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, nordic_qspi_nor))
#define FLASH_NAME "JEDEC QSPI-NOR"
#elif DT_NODE_HAS_STATUS(DT_INST(0, st_stm32_qspi_nor), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, st_stm32_qspi_nor))
#define FLASH_NAME "JEDEC QSPI-NOR"
#else
#error Unsupported flash driver
#endif

extern const struct device *ext_flash_dev;
extern const struct device *gecko_flash_dev;
extern int get_gecko_fw_version(void);
extern uint32_t getData;
extern int buzzer_test();
extern int led_test();
extern int misc_test();

const struct shell *shell = NULL;

#define READ_4K   4096
#define XFER_SIZE 5000
uint8_t mxfer_buf[XFER_SIZE+1];
int max_fragment = 1000;
bool board_has_gnss = false;

int murata_socket_offload_init(void);
int rs9116w_socket_offload_init(void);
void dump_addrinfo(const struct shell *shell, const struct addrinfo *ai);
int process_cli_cmd_modem_psm(const struct shell *shell, size_t argc, char **argv, int sd);
int process_cli_cmd_modem_edrx(const struct shell *shell, size_t argc, char **argv, int sd);

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
	res = fcntl(sd, cmd, params);
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
		z_shell_help_subcmd_print_selitem(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
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
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
		return -EINVAL;
	}
	int idx = strtol(argv[1], NULL, 10);
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
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
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
			if (strcmp(iface->if_dev->dev->name, "murata,1sc") == 0) {
				devid = MODEM_ID;
			} else if (strcmp(iface->if_dev->dev->name, "RS9116W_0") == 0) {
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
			if (target.sa_family == AF_INET6) {
				net_sin6(&target)->sin6_port = htons(port);
			} else {
				net_sin(&target)->sin_port = htons(port);
			}
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
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
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
		} else {
			net_sin6(&target)->sin6_family = AF_INET6;
			net_sin6(&target)->sin6_port = htons(port);
		}
	} else {
		static struct addrinfo hints;
		struct zsock_addrinfo *res;

		hints.ai_family = ai_family;
		hints.ai_socktype = SOCK_DGRAM;
		int devid = 0;
		struct net_if *iface = socks[sock_idx].dev;
		if (strcmp(iface->if_dev->dev->name, "murata,1sc") == 0) {
			devid = MODEM_ID;
		} else if (strcmp(iface->if_dev->dev->name, "RS9116W_0") == 0) {
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
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
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
		shell_error(shell, "Recieve failed, errno = %d", errno);
	} else if (stat == 0) {
		shell_error(shell, "Recieve failed, socket closed!");
	}
	return stat;
}

int sock_rcvfrom(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		z_shell_help_subcmd_print_selitem(shell);
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
	void *addr = (ai_family == AF_INET6) ? (void*)&net_sin6(&target)->sin6_addr : (void*)&net_sin(&target)->sin_addr;
	uint16_t port = ntohs((ai_family == AF_INET6) ? net_sin6(&target)->sin6_port : net_sin(&target)->sin_port);
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
		shell_error(shell, "Recieve failed, errno = %d", errno);
	}
	return stat;
}

int sock_close(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
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
	ret = fcntl(sock_idx, SMS_SEND, &sms);
	// printf("returned from fcntl, ret = %d\n", ret);
	return ret;
}

int sock_recvsms(const struct shell *shell, size_t argc, char **argv)
{
	int ret;
	struct sms_in sms;


	if (argc != 3){
		shell_error(shell, "Missing required arguments");
		z_shell_help_subcmd_print_selitem(shell);
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
	ret = fcntl(sock_idx, SMS_RECV, &sms);
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
			"                  <cmd_str>: imei | imsi | iccid | ssi | sim | apn | msisdn | conn_sts |\n"
			"                             ip | ip6 | version | golden | sleep | wake | awake | psm | edrx");
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
	if (strcmp(iface->if_dev->dev->name, "murata,1sc") != 0) {
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
	} else if (!strcmp(argv[2], "psm")) {
		process_cli_cmd_modem_psm(shell, argc, argv, sd);
	} else {
		strcpy(cmd_buf, argv[2]);
		strupper(cmd_buf);
		int res = fcntl(sd, GET_ATCMD_RESP, cmd_buf);
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
		z_shell_help_subcmd_print_selitem(shell);
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
	ret = tmo_http_download(devid, argv[2], (argc == 4) ? argv[3] : NULL);
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
		SHELL_CMD(scan, NULL, "\"<iface>\"", cmd_wifi_scan),
		SHELL_CMD(status, NULL, "\"<iface>\"", cmd_wifi_status),
		SHELL_SUBCMD_SET_END
		);
#endif

int cmd_sntp(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo sntp <ip or domain> <server>");
		return -EINVAL;
	}	  

	
	return tmo_update_time(shell, argv[1]);
}
int cmd_gnss(const struct shell *shell, size_t argc, char **argv)
{
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
	shell_print(shell, "edrx values: 2,3,5,9,10,11,12,13,14, or 15");
}

void print_set_modem_psm_usage(const struct shell *shell)
{
	shell_print(shell, "tmo modem <iface> psm <mode> <T3312> <T3314> <T3412> <T3324>");
	shell_print(shell, "mode: 0 - off, 1 - on, T3312: 0-7  T3314: 0,1,2,7  T3412: 0-7  T3324: 0,1,2,7");
}

int process_cli_cmd_modem_psm(const struct shell *shell, size_t argc, char **argv, int sd)
{
	union params_cmd params_cmd_u;
	union params_cmd* u = &params_cmd_u;
	if (argc == 8) {
		// This is setting the PSM timer
		int mode = strtol(argv[3], NULL, 10);
		int t3312 = strtol(argv[4], NULL, 10);
		int t3314 = strtol(argv[5], NULL, 10);
		int t3412 = strtol(argv[6], NULL, 10);
		int t3324 = strtol(argv[7], NULL, 10);
		if ( (mode >= 0 && mode <=1) &&  (t3312 >= 0 && t3312 <= 7) &&
				(t3314  >= 0 && (t3314 <= 2 || t3314 == 7) )  &&
				(t3412 >=0 && t3412 <= 7) &&
				(t3324  >= 0 && (t3324 <= 2 || t3324 == 7) ) ) {
			u->psm.mode = mode; u->psm.t3312_mask = t3312; u->psm.t3314_mask = t3314; u->psm.t3412_mask = t3412;
			u->psm.t3324_mask = t3324;
			shell_print(shell, "Set PSM mode: %d, T3312: %d, T3314: %d, T3412: %d, T3324: %d",
					mode, t3312, t3314, t3412, t3324);
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
				((u->edrx.time_mask  == 2 || u->edrx.time_mask == 3 || u->edrx.time_mask == 5) ||
				 ( u->edrx.time_mask >= 9 && u->edrx.time_mask <= 15))) {
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
		SHELL_CMD(ibeacon, NULL, "Advertise as ibeacon", cmd_ble_adv_ibeacon),
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
		z_shell_help_subcmd_print_selitem(shell);
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

	if (argv[2] == NULL && target > 0) {
		shell_error(shell, "There are no updates at this time");
		return -EINVAL;
	}

	return tmo_dfu_download(shell, target, argv[2], argv[3]);
}

int cmd_dfu_get_version(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo dfu version <target>\n"
				"       target: 0 for mcu, 1 for modem, 2 for wifi");
		return -EINVAL;
	}

	shell_print(shell, "cmd_dfu_get_version: target: %d\n", (int) strtol(argv[1], NULL, 10));

	int version_target = (int) strtol(argv[1], NULL, 10);
	switch (version_target) 
	{
		case DFU_GECKO:
			{
				int status = get_gecko_fw_version();
				if (status != 0) {
					shell_error(shell, "reading the Pearl Gecko FW version failed\n");
				}
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
				else {
					shell_print(shell, "The current RS9116W FW version is: %s\n", dfu_rs9116w_fw_version);
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
	int delta_firmware_target = (int) strtol(argv[2], NULL, 10);

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
#ifdef BOOT_SLOT
				if ((strcmp(BOOT_SLOT, "0") || strcmp(BOOT_SLOT, "1"))) {
					if (atoi(BOOT_SLOT) == delta_firmware_target) {
						shell_error(shell,"Can't program slot you are running from");
						return -EINVAL;
					}
				}
#else
				if (delta_firmware_target != 1) {
					shell_error(shell,"Can't program over currently running firmware");
					return -EINVAL;
				}
#endif
				shell_print(shell,"\nStarting the FW update for SiLabs Pearl Gecko");
				int status;
				status = dfu_mcu_firmware_upgrade(delta_firmware_target);
				if (status != 0) {
					shell_error(shell, "The FW update for SiLabs Pearl Gecko failed");
				}
				else {
					shell_print(shell, "The FW update for SiLabs Pearl Gecko is complete!");
				}
			}
			break;

		case DFU_MODEM:
			{
				shell_print(shell,"\nStarting the FW update for Murata 1SC");
				int status;
				status = dfu_modem_firmware_upgrade(delta_firmware_target);
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
		if (!vbus) {
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
	if (battery_attached !=  0) {
		millivolts = read_battery_voltage();
	}
	shell_print(shell, "\tBattery voltage %d.%03dV", millivolts/1000, millivolts%1000);

	return millivolts;
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
	if (battery_attached !=  0) {
		millivolts = read_battery_voltage();
		millivolts_to_percent(millivolts, &percent);
	}
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

int cmd_mfg_test(const struct shell *shell, size_t argc, char **argv)
{
	int rc = 0;
	shell_print(shell, "Run mfg tests...");

	rc |= buzzer_test();
	k_sleep(K_SECONDS(1));
	rc |= led_test();
	k_sleep(K_SECONDS(2));
	misc_test();

	const struct device * pwm_dev = device_get_binding("pwmleds");
	if (rc == 0) {
		led_on(pwm_dev, 2); // Green LED
		shell_fprintf(shell_backend_uart_get_ptr(), SHELL_INFO, "TESTS PASSED\n");
	} else {
		led_on(pwm_dev, 1); // Red LED
		shell_fprintf(shell_backend_uart_get_ptr(), SHELL_ERROR, "TESTS FAILED\n");
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
		z_shell_help_subcmd_print_selitem(shell);
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
		z_shell_help_subcmd_print_selitem(shell);
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
#include <fs/littlefs.h>
#include <storage/flash_map.h>

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_data);
static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &lfs_data,
	.storage_dev = (void *)FLASH_AREA_ID(storage),
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

static int cmd_version(const struct shell *shell, size_t argc, char **argv)
{
	printf("Built for: %s\n", VERSION_BOARD);
	printf("Zephyr:    %s\n", VERSION_ZEPHYR_BASE_TAG);
	printf("TMO RTOS:  %s\n", VERSION_TMO_RTOS_TAG);
	printf("TMO SDK:   %s\n", VERSION_TMO_SDK_TAG);
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
		SHELL_CMD(percent, NULL, "Get battery level (percent)", cmd_battery_percentage),
		SHELL_CMD(voltage, NULL, "Get battery level (Volts)", cmd_battery_voltage),
		SHELL_SUBCMD_SET_END
		);

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

	if (fcntl(sock, CHECK_CERT, name) == 0) {
		if (!force) {
			shell_error(shell, "Cert already loaded!");
			zsock_close(sock);
			return EIO;
		}
		fcntl(sock, DEL_CERT, name);
	}
	fcntl(sock, STORE_CERT, &cparams);
	zsock_close(sock);

	return 0;
}
#endif

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
	SHELL_CMD_ARG(get, &dsub_device_name, "Get a device's power manangement state", cmd_pmget,
			2, 2),
	SHELL_CMD_ARG(off, &dsub_device_name, "Put a device into the off/unpowered state", cmd_pmoff,
			2, 2),
	SHELL_CMD_ARG(on, &dsub_device_name, "Put a device into the on/powered state", cmd_pmon,
			2, 2),
	SHELL_CMD_ARG(resume, &dsub_device_name, "Resume a device from the suspended state", cmd_pmresume,
			2, 2),
	SHELL_CMD_ARG(suspend, &dsub_device_name, "Put a device into the suspended state", cmd_pmsuspend,
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
		SHELL_CMD(buzzer, &tmo_buzzer_sub, "Buzzer tests", NULL),
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
		SHELL_CMD(certs, &certs_sub, "CA cert commands", NULL),
#endif
		SHELL_CMD(dfu, &tmo_dfu_sub, "Device FW updates", NULL),
		SHELL_CMD(dns, NULL, "Perform dns lookup", cmd_dnslookup),
		SHELL_CMD(file, &tmo_file_sub, "File commands", NULL),
		SHELL_CMD(gnssversion, NULL, "Get GNSS chip version", cmd_gnss_version),
		SHELL_CMD(http, NULL, "Get http URL", cmd_http),
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

void tmo_shell_main (void)
{
	ext_flash_dev = device_get_binding(FLASH_DEVICE);
	if (!ext_flash_dev) {
		printf("SPI NOR external flash driver %s was not found!\n", FLASH_DEVICE);
		exit(-1);
	}
	else {
		printf("SPI NOR external flash driver %s ready!\n", FLASH_DEVICE);
	}

	gecko_flash_dev = device_get_binding("FLASH_CTRL");
	if (!gecko_flash_dev) {
		printf("\nFLASH_CTRL: Device driver FLASH_CTRL not found\n");
		exit(-1);
	}
	else {
		printf("Gecko flash driver FLASH_CTRL ready!\n");
	}

	// mount the flash file system
	mountfs();

	cxd5605_init();
	initPK0();
	initADC();
	shell = shell_backend_uart_get_ptr();
#ifdef CONFIG_WIFI
	tmo_wifi_connect();
#endif

#ifdef BOOT_SLOT
	printf("BOOT_SLOT: %s\n", BOOT_SLOT);
#endif
}
