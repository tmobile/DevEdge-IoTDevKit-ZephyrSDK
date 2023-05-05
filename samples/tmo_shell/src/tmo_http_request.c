/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tmo_http_request, LOG_LEVEL_DBG);

#include <stdio.h>
#include <strings.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/fs/fs.h>

#include "ca_certificate.h"
#include "tmo_web_demo.h"
#include "tmo_shell.h"
#include "tmo_certs.h"

#if CONFIG_MODEM
#include <zephyr/drivers/modem/murata-1sc.h>
#include <zephyr/posix/fcntl.h>
#endif

#define MAX_RECV_BUF_LEN 512
static uint8_t recv_buf[MAX_RECV_BUF_LEN];

#define MAX_ENDPOINT_SIZE  200
static char endpoint[MAX_ENDPOINT_SIZE] = {0};

#define USE_AWS_SESSION_FILE 1
#if USE_AWS_SESSION_FILE
static const char* aws_session_file = "/tmo/aws_session.txt";
#else
static char* suffix = "aaaaaaaa";
#endif

/* Set the timeout for http requests to 10 minutes (in milliseconds) */
#define HTTP_CLIENT_REQ_TIMEOUT (10 * 60 * 1000)

int get_endpoint()
{
	int rc = 0;

	memset(endpoint, 0, sizeof(endpoint));
	snprintf(endpoint, sizeof(endpoint), "%s%s",
			get_json_base_url(), get_json_path());
#if USE_AWS_SESSION_FILE
	// Assume fs already mounted

	struct fs_file_t file = {0};
	rc = fs_open(&file, aws_session_file, FS_O_READ);
	if (rc != 0) {
		printf("Cannot open %s, error = %d\n", aws_session_file, rc);
	} else {
#define MAX_AWS_SESSION_SIZE 20
		char aws_session[MAX_AWS_SESSION_SIZE];
		int bytes_read = fs_read(&file, aws_session, sizeof(aws_session));
		if (bytes_read < 0) {
			printf("Cannot read %s\n", aws_session_file);
			return rc;
		}
		aws_session[MIN(bytes_read, MAX_AWS_SESSION_SIZE-1)] = '\0';
		strncat(endpoint, aws_session, sizeof(endpoint)-1);
		fs_close(&file);
	}
#else
	strncat(endpoint, suffix, sizeof(endpoint)-1);
#endif
	return rc;
}

static void response_cb_json(struct http_response *rsp,
		enum http_final_call final_data, void *user_data)
{
	if (final_data == HTTP_DATA_FINAL) {
		LOG_INF("Response status code: %d, %s", rsp->http_status_code, rsp->http_status);
		if (rsp->body_found) {
			LOG_INF("Body length: %d, Body: %s", rsp->recv_buf_len, rsp->recv_buf);
		}
	}
}


#define HTTP_PREFIX  "http://"
#define HTTPS_PREFIX "https://"

void tmo_http_json()
{
	int ret;
	struct http_request req;
	struct http_parser_url u;
	char port_sz[10];
	int tls = 0;

	get_endpoint();
	char *json_payload = get_json_payload_pointer();

	char *server_url = endpoint;
	printf("server_url: %s\npayload:\n%s\n", server_url, json_payload);

	const char *json_request_header[] = {
		"Content-Type: application/json\r\n",
		NULL
	};

	memset(&req, 0, sizeof(req));

	http_parser_url_init(&u);
	http_parser_parse_url(server_url, strlen(server_url), 0, &u);

	int port;

	if (u.port != 0) {
		port = u.port;
	}
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	else if (strncmp(server_url, HTTPS_PREFIX, strlen(HTTPS_PREFIX)) == 0) {
		port = 443;
		tls = 1;
	}
#endif
	else if (strncmp(server_url, HTTP_PREFIX, strlen(HTTP_PREFIX)) == 0) {
		port = 80;
	} else {
		printf("Unsupported schema\n");
		return;
	}
	snprintf(port_sz, sizeof(port_sz), "%d", port);

	char path[256], host[64];
	memset(path, 0, 256);
	if (u.field_set & (1 << UF_PATH)) {
		memcpy(path, server_url + u.field_data[UF_PATH].off, u.field_data[UF_PATH].len);
	} else {
		path[0] = '/';
	}
	memset(host, 0, 64);
	memcpy(host, server_url + u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);

	req.method = HTTP_POST;
	req.url = path;
	req.host = host;
	req.protocol = "HTTP/1.1";
	req.payload = json_payload;
	req.payload_len = strlen(req.payload);
	req.header_fields = json_request_header;
	req.response = response_cb_json;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	static struct zsock_addrinfo hints;
	struct zsock_addrinfo *res;

	ret = tmo_offload_init(get_json_iface_type());
	if (ret != 0) {
		printf("Could not init device, ret = %d\n", ret);
		return;
	}

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	ret = zsock_getaddrinfo(host, port_sz, &hints, &res);
	if (ret) {
		printf("Failed to resolve host %s\n", host);
		return;
	}

	int idx = get_json_iface_type();
	struct net_if *iface = net_if_get_by_index(idx);
	if (iface == NULL) {
		printf("Interface type %d not found", idx);
		zsock_freeaddrinfo(res);
		return;
	}

	int sock;

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	if (tls) {
		
		if (!ca_cert_sz) {
			tls_credential_delete(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE);
			tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
					entrust_g2, sizeof(entrust_g2));
		}

		sock = zsock_socket_ext(res->ai_family, res->ai_socktype, IPPROTO_TLS_1_2, iface);
	} else
#endif
	{
		sock = zsock_socket_ext(res->ai_family, res->ai_socktype, res->ai_protocol, iface);
	}

	if (sock < 0) {
		printf("Error creating socket, error: %d, errno: %d\n", sock, errno);
		zsock_freeaddrinfo(res);
		return;
	}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	if (tls) {
		sec_tag_t sec_tag_opt[] = {
			CA_CERTIFICATE_TAG,
		};
		zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST,
				sec_tag_opt, sizeof(sec_tag_opt));

		zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME,
				host, strlen(host) + 1);
	}
#endif
#if CONFIG_MODEM
	int tls_verify_val = TLS_PEER_VERIFY_NONE;
	zsock_setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &tls_verify_val, sizeof(tls_verify_val));
#endif
	//Now connect the socket
	ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);

	if (ret < 0) {
		printf("Error connecting socket, error: %d, errno: %d\n", ret, errno);
	} else {
		printf("Sending request...\n");
		ret = http_client_req(sock, &req, HTTP_CLIENT_REQ_TIMEOUT, NULL);
		printf("http_client_req returned %d\n", ret);
	}
	zsock_freeaddrinfo(res);
	zsock_close(sock);
}

static int http_total_received = 0;
static int http_total_written = 0;
static int http_content_length = 0;
static void response_cb_download(struct http_response *rsp,
		enum http_final_call final_data, void *user_data)
{
	struct fs_file_t *file = user_data;

	if (rsp->http_status_code < 200 && rsp->http_status_code > 299) {
		printf("\nHTTP Status %d: %s\n", rsp->http_status_code, rsp->http_status);
	}

	if (!http_content_length) {
		if (rsp->content_length) {
			http_content_length = rsp->content_length;
			printf("\nExpecting %d bytes\n", http_content_length);
		}
	}
	if (rsp->body_found) {
		http_total_received += rsp->body_frag_len;
		if (file) {
			int ret = fs_write(file, rsp->body_frag_start, rsp->body_frag_len);
			if (ret > 0) {
				http_total_written += ret;
			}
		}
		printf(".");
	}
}

#define HTTP_PREFIX  "http://"
#define HTTPS_PREFIX "https://"
extern uint8_t mxfer_buf[];

#ifndef CONFIG_TMO_HTTP_MOCK_SOCKET
int create_http_socket(bool tls, char* host, struct addrinfo *res, struct net_if *iface)
{
	int sock = -1;
	if (!tls) {
		sock = zsock_socket_ext(res->ai_family, res->ai_socktype, res->ai_protocol, iface);
	}
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	else {
#if IS_ENABLED(CONFIG_TMO_SHELL_USE_MBED)
		sock = zsock_socket(res->ai_family, res->ai_socktype, IPPROTO_TLS_1_2);
		if (sock >= 0) {
			int tls_native = 1;
			zsock_setsockopt(sock, SOL_TLS, TLS_NATIVE, &tls_native, sizeof(tls_native));
		}
#else
		sock = zsock_socket_ext(res->ai_family, res->ai_socktype, IPPROTO_TLS_1_2, iface);
#endif
		if (sock < 0) {
			return sock;
		}
		sec_tag_t sec_tag_opt[] = {
			CA_CERTIFICATE_TAG,
		};
		zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST,
				sec_tag_opt, sizeof(sec_tag_opt));

		zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME,
				host, strlen(host) + 1);
#if IS_ENABLED(CONFIG_TMO_SHELL_USE_MBED)
		struct ifreq ifreq = {0};
		strcpy(ifreq.ifr_name, iface->if_dev->dev->name);
		zsock_setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
				&ifreq, sizeof(ifreq));
#endif
	}
#endif
	return sock;
}
#else
extern int http_fail_unit_test_socket_create(void);
int create_http_socket(bool tls, char* host, struct addrinfo *res, struct net_if *iface)
{
	LOG_WRN("Using mocked socket for download.");
	int sock = -1;
	sock = http_fail_unit_test_socket_create();
	return sock;
}
#endif


int tmo_http_download(int devid, char url[], char filename[], char *auth_key)
{
	static struct addrinfo hints;
	struct addrinfo *res = NULL;
	int sock = -1;
	struct http_request req;
	struct http_parser_url u;
	char port_sz[10];
	int tls = 0;
	int ret = -1;
	struct fs_file_t file = {0};
	char *auth_header = NULL;
	char auth_header_buf[64];

	if (auth_key){
		snprintf(auth_header_buf, sizeof(auth_header_buf) - 1, "Authorization: Basic %s", auth_key);
		auth_header = auth_header_buf;
	}
	

	const char *headers[] = {
		auth_header, NULL
	};

	memset(&req, 0, sizeof(req));

	http_parser_url_init(&u);
	http_parser_parse_url(url, strlen(url), 0, &u);

	int port;

	if (u.port != 0) {
		port = u.port;
	}
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	else if (strncmp(url, HTTPS_PREFIX, strlen(HTTPS_PREFIX)) == 0) {
		port = 443;
		tls = 1;
	}
#endif
	else if (strncmp(url, HTTP_PREFIX, strlen(HTTP_PREFIX)) == 0) {
		port = 80;
	} else {
		printf("Error: unsupported schema");
		return -EINVAL;
	}

	memset(port_sz, 0, sizeof(port_sz));
	snprintf(port_sz, sizeof(port_sz), "%d", port);
	req.method = HTTP_GET;
	char path[256], host[64];
	memset(path, 0, 256);
	if (u.field_set & (1 << UF_PATH)) {
		memcpy(path, url + u.field_data[UF_PATH].off, u.field_data[UF_PATH].len +
				(u.field_set & (1 << UF_QUERY) ? u.field_data[UF_QUERY].len + 1 : 0));
	} else {
		path[0] = '/';
	}
	memset(host, 0, 64);
	memcpy(host, url + u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);

	req.url = path;
	req.host = host;
	req.protocol = "HTTP/1.1";
	req.header_fields = headers;
	req.response = response_cb_download;
	req.recv_buf = mxfer_buf;
	req.recv_buf_len = 4096;
	req.packet_timeout = 10000;

	ret = tmo_offload_init(devid);
	if (ret != 0) {
		printf("Error: could not init device %d", devid);
	}

	// hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	ret = zsock_getaddrinfo(host, port_sz, &hints, &res);
	if (ret) {
		printf("Failed to resolve host %s\n", host);
		return -EINVAL;
	}

	struct net_if *iface = net_if_get_by_index(devid);
	if (iface == NULL) {
		printf("Error: interface %d not found", devid);
		return -EINVAL;
	}

	sock = create_http_socket(tls, host, res, iface);
	if (sock < 0) {
		printf("Error creating socket, ret = %d, errno = %d", sock, errno);
		goto exit;
	}
	// struct ifreq ifreq = {0};
	// strcpy(ifreq.ifr_name, iface->if_dev->dev->name);
	// ret = zsock_setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
	// 			   &ifreq, sizeof(ifreq));
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS) && defined(CONFIG_MODEM)
	bool user_trust = false;
	int profile;
	if (devid == MODEM_ID && tls) {
		struct murata_tls_profile_params pparams = {0};
		pparams.profile_id_num = 255;
		pparams.ca_path = ".";
		fcntl(sock, CREATE_CERT_PROFILE, &pparams);
		ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
		if (ret == -1) {
			zsock_close(sock);
			sock = create_http_socket(tls, host, res, iface);
			profile = 255;
			zsock_setsockopt(sock, SOL_TLS, TLS_MURATA_USE_PROFILE, &profile, sizeof(profile));
			ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
			user_trust = true;
		}
	} else {
		ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
	}

#else 
	ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
#endif
	// //Now create the socket
	// ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
	if (ret < 0) {
		printf("Error connecting, ret = %d, errno = %d", ret, errno);
		goto exit;
	}

	http_total_received = 0;
	http_total_written = 0;
	http_content_length = 0;
	int fail_count = 0;

	if (filename) {
		// Assume fs is already mounted

		printf("Opening file %s\n", filename);
		ret = fs_open(&file, filename, FS_O_CREATE | FS_O_WRITE);
		if (ret != 0) {
			printf("Error: could not open file %s\n", filename);
			goto exit;
		}

		ret = fs_truncate(&file, 0);
		if (ret != 0) {
			printf("Could not truncate file %s\n", filename);
			goto exit;
		}
		errno = 0;
		ret = http_client_req(sock, &req, HTTP_CLIENT_REQ_TIMEOUT, &file);
		while (http_content_length && http_content_length > http_total_received && fail_count < 5) {
			fail_count++;
			printf("\nTransfer failure detected, reinitializing transfer... (%d/5) (%d < %d)\n", fail_count, http_total_received, http_content_length);
			zsock_close(sock);
			sock = create_http_socket(tls, host, res, iface);
			if (user_trust)
				zsock_setsockopt(sock, SOL_TLS, TLS_MURATA_USE_PROFILE, &profile, sizeof(profile));
			errno = 0;
			k_msleep(2000);
			zsock_connect(sock, res->ai_addr, res->ai_addrlen);
			char *headers[] = {
				NULL, auth_header, NULL
			};
			char range_header[32] = {0};
			snprintk(range_header, sizeof(range_header), "Range: bytes=%d-\r\n", http_total_received);
			headers[0] = range_header;
			req.header_fields = (const char**)headers;
			// req.header_fields
			int last_rcvd_cnt = http_total_received;
			http_client_req(sock, &req, HTTP_CLIENT_REQ_TIMEOUT, &file);
			/* Reset count if new data has been transfered */
			if (last_rcvd_cnt < http_total_received) {
				fail_count = 0;
			}
		}
		if (http_total_received == http_total_written) {
			printf("\nReceived:%d, Wrote: %d\n", http_total_received, http_total_written);
		} else {
			printf("\nReceived:%d, Wrote: %d\n", http_total_received, http_total_written);
		}
	} else {
		errno = 0;
		ret = http_client_req(sock, &req, HTTP_CLIENT_REQ_TIMEOUT, NULL);
		while (http_content_length && http_content_length > http_total_received && fail_count < 5) {
			fail_count++;
			printf("\nTransfer failure detected, reinitializing transfer... (%d/5) (%d < %d)\n", fail_count, http_total_received, http_content_length);
			zsock_close(sock);
			sock = create_http_socket(tls, host, res, iface);
			if (user_trust)
				zsock_setsockopt(sock, SOL_TLS, TLS_MURATA_USE_PROFILE, &profile, sizeof(profile));
			errno = 0;
			k_msleep(2000);
			zsock_connect(sock, res->ai_addr, res->ai_addrlen);
			char *headers[] = {
				NULL, auth_header, NULL
			};
			char range_header[32] = {0};
			snprintk(range_header, sizeof(range_header), "Range: bytes=%d-\r\n", http_total_received);
			headers[0] = range_header;
			req.header_fields = (const char**)headers;
			// req.header_fields
			int last_rcvd_cnt = http_total_received;
			http_client_req(sock, &req, HTTP_CLIENT_REQ_TIMEOUT, &file);
			/* Reset count if new data has been transfered */
			if (last_rcvd_cnt < http_total_received) {
				fail_count = 0;
			}
		}
		printf("\n\nReceived:%d\n", http_total_received);
	}
	if (fail_count == 5 && http_total_received != http_content_length) {
		printf("Error: Exceded maximum number of attempts for download\n");
		ret = -EAGAIN;
		goto exit;
	}
exit:
	if (res) {
		freeaddrinfo(res);
	}
	if (filename) {
		fs_close(&file);
	}
	if (sock >= 0) {
		zsock_close(sock);
	}
	if (ret < 0) {
		return ret;
	} else {
		return http_total_received;
	}
}
