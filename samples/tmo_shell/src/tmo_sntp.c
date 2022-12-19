#include <ctype.h>
#include <zephyr/posix/time.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/timeutil.h>
#include "tmo_shell.h"
#include "tmo_sntp.h"
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sntp_client, LOG_LEVEL_DBG);

#include <zephyr/net/sntp.h>
#ifdef CONFIG_POSIX_API
#include <arpa/inet.h>
#endif

#include <stdlib.h>
#include <string.h>


static int resolve_dns(const char *host, char *ip_str, int *ipVer)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int errcode;
    char addrstr[100];
    void *ptr = NULL;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo (host, NULL, &hints, &res);
    if (errcode != 0)
    {
        printf ("[sntp] getaddrinfo\n");
        return -1;
    }
    
	if (!res) {
		printf("[sntp] result is null\n");
		return -1;
	}

    printf ("[sntp] Host: %s\n", host);
	inet_ntop (res->ai_family, res->ai_addr->data, addrstr, 100);

	switch (res->ai_family)
    {
        case AF_INET:
            ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
			*ipVer = 4;
        break;
        case AF_INET6:
            ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
			*ipVer = 6;
        break;
    }

	inet_ntop (res->ai_family, ptr, addrstr, 100);
	memcpy(ip_str, addrstr, strlen(addrstr));
	*ipVer = res->ai_family == PF_INET6 ? 6 : 4;
	printf ("[sntp] IPv%d address: %s\n", *ipVer,ip_str);
	freeaddrinfo(res);
    return 0;
}


static void date_print(const struct shell *shell, struct tm *tm)
{
	shell_print(shell,
			"%d-%02u-%02u "
			"%02u:%02u:%02u UTC",
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec);
}

static int time_date_set(const struct shell *shell, uint32_t epoch_sec)
{
#if defined(CONFIG_TIME_GECKO_RTCC)
	uint32_t time = 0;    ///< RTCC_TIME - Time of Day Register
	uint32_t date = 0;    ///< RTCC_DATE - Date Register
	uint32_t year = 0;
#endif
	struct tm tm;
	struct timespec tp;
	tp.tv_sec = (uint32_t)epoch_sec;

	gmtime_r(&tp.tv_sec, &tm);
	date_print(shell,&tm);

#if defined(CONFIG_TIME_GECKO_RTCC)
	time |= bin2bcd(tm.tm_sec) << _RTCC_TIME_SECU_SHIFT;
	time |= bin2bcd(tm.tm_min) << _RTCC_TIME_MINU_SHIFT;
	time |= bin2bcd(tm.tm_hour) << _RTCC_TIME_HOURU_SHIFT;
	date |= tm.tm_wday << _RTCC_DATE_DAYOW_SHIFT;
	date |= bin2bcd(tm.tm_mday) << _RTCC_DATE_DAYOMU_SHIFT;
	date |= bin2bcd(tm.tm_mon) << _RTCC_DATE_MONTHU_SHIFT;

	year = tm.tm_year;
	if (year >= 100) {
		RTCC->RET[0].REG = year / 100;
		year %= 100;
	}

	date |= bin2bcd(year) << _RTCC_DATE_YEARU_SHIFT;

	RTCC_TimeSet(time);
	RTCC_DateSet(date);

	uint32_t rtc_val =RTCC_TimeGet();

	if (rtc_val != time) {
		shell_error(shell, "Could not set time and date");
		return -EINVAL;
	} 

	rtc_val =RTCC_DateGet();
	if (rtc_val != date) {
		shell_error(shell, "Could not set time and date");
		return -EINVAL;
	}

	if( clock_settime( CLOCK_REALTIME, &tp) == -1 ) {
		shell_error( shell, "system clock set failed" );
		return -EINVAL;
	}
	shell_print(shell, "successfully updated RTC");

#endif

	return 0;
}

int isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
}


int tmo_update_time(const struct shell *shell, char *host, int iface_idx)
{
#if defined(CONFIG_NET_IPV6)
#ifdef DEBUG
	struct sockaddr_in6 addr6;
#endif
#endif
	char ip_addr[100];
	int ipVer = 4;

	// Create and zero out the packet. All 48 bytes worth.
	ntp_packet packet = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	memset( &packet, 0, sizeof( ntp_packet ) );

	// Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.
	*( ( char * ) &packet + 0 ) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

	struct net_if *iface = net_if_get_by_index(iface_idx);
	
	if (tmo_offload_init(iface_idx)) {
		return -1;
	}

	if (iface == NULL) {
		shell_error(shell, "Interface %d not found", iface_idx);
		return -EINVAL;
	}

	int sd = zsock_socket_ext(AF_INET, SOCK_DGRAM, IPPROTO_UDP, iface);
	if (sd == -1) {
		shell_error(shell, "Socket creation failed, errno = %d", errno);
		return 0;
	}

	memset(ip_addr, 0, sizeof(char) * 100);
    if (isValidIpAddress(host)) {
#ifdef DEBUG
        shell_print(shell, "ip");
#endif
        strncpy(ip_addr,host, strlen(host));
    }
    else {
#ifdef DEBUG
        shell_print(shell, "dns");
#endif
		resolve_dns(host, ip_addr, &ipVer);
    }

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(SNTP_PORT);
	inet_pton(AF_INET, ip_addr, &sin.sin_addr);
#ifdef DEBUG
	shell_error(shell, "IP Address %s converted %d", ip_addr, sin.sin_addr.s_addr);
#endif

	if (zsock_connect(sd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		shell_error(shell, "zsock_connect errno");
	}

	int stat = zsock_send(sd, ( char* ) &packet, sizeof( ntp_packet ), 0);
	if (stat == -1) {
		shell_error(shell, "Send failed, errno = %d", errno);
		zsock_close(sd);
		return -EINVAL;
	}
	k_msleep(2000);

	int recvsize = sizeof( ntp_packet );

	memset( ( char* ) &packet, 0, sizeof( ntp_packet ));
	int total = 0;
	while (total < recvsize || recvsize == 0) {
		stat = zsock_recv(sd,  (( char* ) &packet) + total, recvsize - total, ZSOCK_MSG_DONTWAIT);
		if (stat == -1) {
			shell_error(shell, "recv failed, errno = %d", errno);
			zsock_close(sd);
			return -1;
		}
		total += stat;
	}
	if (total == -1 && errno == EWOULDBLOCK) {
		shell_print(shell, "No data available!");
		zsock_close(sd);
		return total;
	}

	packet.txTm_s = ntohl( packet.txTm_s ); // Time-stamp seconds.
	packet.txTm_f = ntohl( packet.txTm_f ); // Time-stamp fraction of a second.

	time_t txTm = ( time_t ) ( packet.txTm_s - NTP_TIMESTAMP_DELTA );
#ifdef DEBUG
	shell_print(shell, "epoch %lld", txTm);
#endif
	time_date_set(shell,txTm);
	zsock_close(sd);
	return 0;
}
