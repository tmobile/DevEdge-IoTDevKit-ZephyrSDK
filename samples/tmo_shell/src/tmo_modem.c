#include <errno.h>
#include <zephyr.h>
#include <net/socket.h>

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
	uintptr_t uiptr = (uintptr_t) ptr;
	/* It's fine if this overflows */
	unsigned int flags = uiptr;

	if (uiptr > INT_MAX) {
		return -EOVERFLOW;
	}

	/* va_arg is defined for (unsigned T) -> (signed T) as long as the value can be represented by both types (C11 7.16.1.1) */
	return fcntl(sock, cmd, flags);
}
#else
int fcntl_ptr(int sock, int cmd, const void* ptr) {
	(void) sock;
	(void) cmd;
	(void) ptr;
	return -ENOTSUP;
}
#endif