#include <errno.h>
#include <zephyr/posix/fcntl.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/modem/murata-1sc.h>
#include <zephyr/net/socket.h>

/* Hardcoded for now */
#define TMO_MODEM_IFACE_NUMBER    1

#ifdef CONFIG_SOC_SERIES_EFM32PG12B

/**
 * @brief Passes a pointers data to fnctl which requires int.
 * This code is not portable, it expects the pointer to be representable
 * as a positive signed integer. This should not be an issue on the
 * EFM32PG12 since it does not address any RAM or ROM above 0x24000000
 *
 * Implementers of additional systems need to ensure this call is safe for their
 * platform.
 *
 * @param sock The sock to send to
 * @param cmd The command to send
 * @param ptr The pointer to send encoded as a positive signed integer.
 * @return int The fcntl return if the pointer can be cast, or -EOVERFLOW on error
 */
int fcntl_ptr(int sock, int cmd, const void* ptr) {
	uintptr_t uiptr = (uintptr_t)ptr;
	/* It's fine if this overflows */
	unsigned int flags = uiptr;

	if (uiptr > INT_MAX) {
		return -EOVERFLOW;
	}

	/* va_arg is defined for (unsigned T) -> (signed T) as long as the value can be represented by both types (C11 7.16.1.1) */
	return zsock_fcntl(sock, cmd, flags);
}


#else
int fcntl_ptr(int sock, int cmd, const void* ptr) {
	(void)sock;
	(void)cmd;
	(void)ptr;

	return -ENOTSUP;
}


#endif

#define MAX_CMD_BUF_SIZE    256
char cmd_io_buf[MAX_CMD_BUF_SIZE] = { 0 };

/* The resp buffer must be "owned" by whoever is making the call */
K_MUTEX_DEFINE(ioctl_lock);

/**
 * @brief A helper to use an offload socket like a normal one
 *
 * @param family
 * @param type
 * @param proto
 * @param iface
 * @return int
 */
static inline int zsock_socket_ext(int family, int type, int proto, struct net_if* iface) {
	if (iface->if_dev->offload && iface->if_dev->socket_offload != NULL) {
		return iface->if_dev->socket_offload(family, type, proto);
	} else {
		errno = EINVAL;

		return -1;
	}
}


/**
 * @brief Returns a new sock for the Murata modem
 *
 * @param idx The index to use to find the modem
 * @return int Sock on success, -err on failure
 */
static int tmo_modem_get_sock(int idx) {
	struct net_if* iface = net_if_get_by_index(idx);
	int sd;

	if (iface == NULL) {
		return -EINVAL;
	}
	if (!strstr(iface->if_dev->dev->name, "murata")) {
		return -EINVAL;
	}
	sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);

	return sd;
}


static int tmo_modem_get_atcmd_resp(enum mdmdata_e cmd) {
	int idx = 0;
	const char* cmd_str = NULL;
	int sd;
	int ret;

	while (cmd_pool[idx].str != NULL) {
		if (cmd_pool[idx].atcmd == cmd) {
			cmd_str = cmd_pool[idx].str;
			break;
		}
		++idx;
	}
	strncpy(cmd_io_buf, cmd_str, sizeof(cmd_io_buf));

	/* The handler expects a non const ptr that it will sometimes write a response into */
	sd  = tmo_modem_get_sock(TMO_MODEM_IFACE_NUMBER);
	ret = fcntl_ptr(sd, GET_ATCMD_RESP, cmd_io_buf);
	zsock_close(sd);

	return ret;
}


/**
 * @brief A number of modem calls return a string of a known length. This function
 * abstracts the convention of checking the result length, making the call and copying
 * the result into the result buffer if the call was a success.
 *
 * @param res The buffer to write into
 * @param res_len The length of res
 * @param type The type of call to make
 * @return int 0 on success, -err on error. -EINVAL if type is not known.
 */
static int tmo_modem_atcmd_str_get(char* res, int res_len, enum mdmdata_e type) {
	int ret = -1;

	/* Make sure the dest is long enough to support the expected response */
	switch (type) {
	case imei_e:
		if (res_len < MDM_IMEI_LENGTH) {
			return -EINVAL;
		}
		break;

	case iccid_e:
		if (res_len < MDM_ICCID_LENGTH) {
			return -EINVAL;
		}
		break;

	case imsi_e:
		if (res_len < MDM_IMSI_LENGTH) {
			return -EINVAL;
		}
		break;

	case msisdn_e:
		if (res_len < MDM_PHN_LENGTH) {
			return -EINVAL;
		}
		break;

	default: /* Unsupported commands are -EINVAL */
		return -EINVAL;
	}

	k_mutex_lock(&ioctl_lock, K_FOREVER);
	ret = tmo_modem_get_atcmd_resp(type);
	if (ret == 0) {
		strcpy(res, cmd_io_buf);
	}
	k_mutex_unlock(&ioctl_lock);

	return ret;
}


int tmo_modem_wake() {
	int ret;

	k_mutex_lock(&ioctl_lock, K_FOREVER);
	ret = tmo_modem_get_atcmd_resp(wake_e);
	k_mutex_unlock(&ioctl_lock);

	return ret;
}


int tmo_modem_sleep() {
	int ret;

	k_mutex_lock(&ioctl_lock, K_FOREVER);
	ret = tmo_modem_get_atcmd_resp(sleep_e);
	k_mutex_unlock(&ioctl_lock);

	return ret;
}


int tmo_modem_get_state() {
	int ret = -1;

	k_mutex_lock(&ioctl_lock, K_FOREVER);
	/* This call will set cmd_io_buf, but that provides no more information than the return code */
	ret = tmo_modem_get_atcmd_resp(awake_e);
	k_mutex_unlock(&ioctl_lock);

	return ret;
}


int tmo_modem_get_imei(char* res, int res_len) {
	return tmo_modem_atcmd_str_get(res, res_len, imei_e);
}


int tmo_modem_get_iccid(char* res, int res_len) {
	return tmo_modem_atcmd_str_get(res, res_len, iccid_e);
}


int tmo_modem_get_imsi(char* res, int res_len) {
	return tmo_modem_atcmd_str_get(res, res_len, imsi_e);
}


int tmo_modem_get_msisdn(char* res, int res_len) {
	return tmo_modem_atcmd_str_get(res, res_len, msisdn_e);
}

#if CONFIG_MODEM_SMS && CONFIG_TMO_SHELL_ASYNC_SMS
#include <zephyr/drivers/modem/sms.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

static void on_sms_recv(const struct device *dev, struct sms_in *sms, int csms_ref,
			 int csms_idx, int csms_tot)
{
	ARG_UNUSED(dev);
	const struct shell *sh = shell_backend_uart_get_ptr();

	shell_print(sh, "SMS Received [%d/%d] from %s, sent at %s:\n%s", 
			csms_idx, csms_tot, sms->phone, sms->time, sms->msg);
	
}

static struct sms_recv_cb recv_cb = {
	.recv = on_sms_recv,
};

static int sms_recv_cb_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(murata_1sc));

	sms_recv_cb_register(&recv_cb);
	sms_recv_cb_en(dev, 1);

	return 0;
}

SYS_INIT(sms_recv_cb_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif /* CONFIG_MODEM_SMS && CONFIG_TMO_SHELL_ASYNC_SMS */
