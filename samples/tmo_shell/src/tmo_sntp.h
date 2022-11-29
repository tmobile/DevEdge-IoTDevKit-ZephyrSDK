#ifndef TMO_SHELL_INCLUDE_ONCE
#define TMO_SHELL_INCLUDE_ONCE
#include <net/sntp.h>
#include <net/socketutils.h>
#ifdef CONFIG_POSIX_API
#include <arpa/inet.h>
#endif

#include <net/net_ip.h>
#include <time.h>
#include <types.h>
#include <em_cmu.h>
#include <em_rtcc.h>
#include <zephyr/drivers/counter.h>
#include <shell/shell.h>

#define SNTP_PORT 123
#define NTP_TIMESTAMP_DELTA 2208988800ull

// Structure that defines the 48 byte NTP packet protocol.
typedef struct
{

	uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
							// li.   Two bits.   Leap indicator.
							// vn.   Three bits. Version number of the protocol.
							// mode. Three bits. Client will pick mode 3 for client.

	uint8_t stratum;         // 8 bits. Stratum level of the local clock.
	uint8_t poll;            // 8 bits. Maximum interval between successive messages.
	uint8_t precision;       // 8 bits. Precision of the local clock.

	uint32_t rootDelay;      // 32 bits. Total round trip delay time.
	uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
	uint32_t refId;          // 32 bits. Reference clock identifier.

	uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
	uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

	uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
	uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

	uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
	uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

	uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
	uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

} ntp_packet;              // Total: 384 bits or 48 bytes.

int tmo_update_time(const struct shell *shell, char * server, int iface_idx);
#endif
