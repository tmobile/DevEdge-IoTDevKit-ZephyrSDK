/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/posix/unistd.h>
#include <getopt.h>
#include <zephyr/net/ping.h>
#include <zephyr/net/socket.h>
#include "tmo_shell.h"
#include "tmo_ping.h"

int ping_rxd;
char host_addr[NET_IPV6_ADDR_LEN];

K_SEM_DEFINE(ping_sem, 0, 1);

const static struct shell* sh;
void ping_cb(uint32_t ms)
{
	
	// shell_print("Request %d ");
    shell_print(sh, "Reply from %s; time = %dms", host_addr, ms);
    ping_rxd++;
	k_sem_give(&ping_sem);
}

static struct net_ping_handler ping_handler = {
	.handler = ping_cb
};

static inline void print_usage(const struct shell *shell)
{
    shell_print(shell, "usage: ping [-c count] [-s packetsize] [-t timeout] iface host");
}

int cmd_ping(const struct shell *shell, size_t argc, char **argv)
{
    int ping_cnt = 1, ret = 0;
    uint32_t timeout_ms = 5000;
    uint16_t sz = 64;
    int8_t c;
    ping_rxd = 0;
    sh = shell;
	net_ping_cb_register(&ping_handler);
    if (argc < 3) {
        print_usage(shell);
        ret = -EINVAL;
        goto exit;
    }
    while ((c = getopt(argc - 2, argv, "ht:s:c:")) != -1) {
        switch (c) {
        case 'h':
            print_usage(shell);
            goto exit;
        case 't':
            timeout_ms = strtol(optarg, NULL, 10);
            break;
        case 's':
            sz = strtol(optarg, NULL, 10);
            break;
        case 'c':
            ping_cnt = strtol(optarg, NULL, 10);
            break;
        case '?':
            shell_error(shell, "Illegal option -- %c", (char)optopt);
            ret = -EINVAL;
            print_usage(shell);
            goto exit;
        default:
            break;
        }
    }
    int if_idx = strtol(argv[argc - 2], NULL, 10);
    if (if_idx < 0 || if_idx > 2) {
        shell_error(shell, "Unknown iface %d", if_idx);
        print_usage(shell);
        goto exit;
    }
    struct net_if* iface = net_if_get_by_index(if_idx);
    struct sockaddr dst;
    char *host = argv[argc - 1];
    if (ping_cnt <= 0) {
        shell_error(shell, "Invalid ping count %d", ping_cnt);
        print_usage(shell);
    }
    if (!net_ipaddr_parse(host, strlen(host), &dst)) 
    {
        tmo_offload_init(if_idx);
        struct zsock_addrinfo *res;
        if (zsock_getaddrinfo(host, "1", NULL, &res)){
            shell_error(shell, "Cannot resolve %s: Unknown host", host);
            print_usage(shell);
            goto exit;
        }
        memcpy(&dst, res->ai_addr, sizeof(struct sockaddr));
        zsock_freeaddrinfo(res);
    }
    net_addr_ntop(dst.sa_family,
         ((dst.sa_family == AF_INET) ? (void*)&net_sin(&dst)->sin_addr : (void*)&net_sin6(&dst)->sin6_addr),
         host_addr, sizeof(host_addr));
    shell_print(shell, "PING %s (%s): %d data bytes", host, host_addr, sz);
    k_sem_reset(&ping_sem);
    for (int i = 0; i < ping_cnt; i++) {
        ret = net_ping(iface, &dst, sz);
        if (ret) {
            shell_error(shell, "Failed to initiate ping, err = %d", ret);
            goto exit;
        }
        if (k_sem_take(&ping_sem, K_MSEC(timeout_ms))) {
            shell_print(shell, "Request timeout");
        }
        k_msleep(1000);
    }
    shell_print(shell, "%d packets transmitted, %d packets received, %.1d%% packet loss",
         ping_cnt, ping_rxd, (100 * (ping_cnt - ping_rxd)) / ping_cnt);
    exit:
    k_sem_give(&ping_sem);
    net_ping_cb_unregister(&ping_handler);
    return ret;
}
