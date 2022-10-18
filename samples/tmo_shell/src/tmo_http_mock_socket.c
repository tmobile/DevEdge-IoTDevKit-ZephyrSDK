/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <strings.h>
#include <zephyr/zephyr.h>
#include <net/socket_offload.h>
#include "sockets_internal.h"

#define FAILURE_POINT 1000000
#define DOWNLOAD_SIZE 2000000

const char *http_resp_header =
"HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: "
"text/html\r\nConnection: Close\r\n\r\n";

static struct {
	bool header_sent;
	int ptr;
	int len;
	int so_rcvtimeo;
} sock_data;

static ssize_t s_recvfrom(void *obj, void *buf, size_t len, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
	char gen_buffer[128];
	if (sock_data.ptr >= FAILURE_POINT && sock_data.so_rcvtimeo) {
		k_msleep(sock_data.so_rcvtimeo);
		errno = EAGAIN;
		return -1;
	} else {
		k_msleep(5);
	}
	if (!sock_data.header_sent) {
		snprintk(gen_buffer, sizeof(gen_buffer), http_resp_header,
				sock_data.len);
		int cpl = MIN(strlen(gen_buffer + sock_data.ptr), len);
		memcpy(buf, gen_buffer, cpl);
		sock_data.ptr += cpl;
		if (sock_data.ptr >= strlen(gen_buffer)) {
			sock_data.header_sent = true;
			sock_data.ptr = 0;
		}
		return cpl;
		// sizeof(http_resp_header);
	} else {
		memset(gen_buffer, 0, sizeof(gen_buffer));
		snprintk(gen_buffer, sizeof(gen_buffer), "%d",
				sock_data.ptr + (DOWNLOAD_SIZE - sock_data.len));
		int cpl = MIN(sizeof(gen_buffer), len);
		cpl = MIN(cpl, sock_data.len - sock_data.ptr);
		memcpy(buf, gen_buffer, cpl);
		sock_data.ptr += cpl;
		return cpl;
	}
}

const char *strncasestr(const char *big, const char *little, int mxlen)
{
	size_t count = 0;
	mxlen -= (strlen(little) - 1);
	while (count < mxlen) {
		if (!strncasecmp(big + count, little, strlen(little))) {
			return count + big;
		}
		count++;
	}
	return NULL;
}

static ssize_t s_sendto(void *obj, const void *buf, size_t len, int flags,
		const struct sockaddr *to, socklen_t tolen)
{
	// strstr()
	// strncasecmp(header, "Content-Length", sizeof("Content-Length") - 1)
	const char *rh = strncasestr(buf, "Range: bytes=", len);

	if (rh) {
		sock_data.len =
			DOWNLOAD_SIZE -
			strtol(rh + sizeof("Range: bytes=") - 1, NULL, 10);
	}

	k_msleep(100);
	return len;
}

static int s_connect(void *obj, const struct sockaddr *addr, socklen_t addrlen)
{
	k_msleep(100);
	return 0;
}

static ssize_t s_read(void *obj, void *buffer, size_t count)
{
	return s_recvfrom(obj, buffer, count, 0, NULL, 0);
}

static ssize_t s_write(void *obj, const void *buffer, size_t count)
{
	return s_sendto(obj, buffer, count, 0, NULL, 0);
}

static int s_close(void *obj) { return 0; }

static int s_setsockopt(void *obj, int level, int optname, const void *optval,
		socklen_t optlen)
{
	if (level != SOL_SOCKET || optname != SO_RCVTIMEO ||
			optlen != sizeof(struct timeval)) {
		return -EINVAL;
	}
	const struct timeval *ptv = optval;
	sock_data.so_rcvtimeo = ptv->tv_sec * 1000;
	sock_data.so_rcvtimeo += ptv->tv_usec / 1000;
	return 0;
}

static const struct socket_op_vtable socket_fd_op_vtable = {
	.fd_vtable =
	{
		.read = s_read,
		.write = s_write,
		.close = s_close,
	},
	.connect = s_connect,
	.sendto = s_sendto,
	.recvfrom = s_recvfrom,
	.setsockopt = s_setsockopt,
};

int http_fail_unit_test_socket_create(void)
{
	int fd = z_reserve_fd();

	if (fd < 0) {
		return -1;
	}

	sock_data.ptr = 0;
	sock_data.len = DOWNLOAD_SIZE;
	sock_data.header_sent = 0;
	sock_data.so_rcvtimeo = 0;

	z_finalize_fd(fd, &sock_data,
			(const struct fd_op_vtable *)&socket_fd_op_vtable);

	return fd;
}
