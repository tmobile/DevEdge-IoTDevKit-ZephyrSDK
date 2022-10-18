/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_MODEM_EDRX_H
#define TMO_MODEM_EDRX_H

#include <stdint.h>
#include <time.h>

#define MAX_EDRX_TIME 10485.76
#define TMO_SUCCESS 0
#define TMO_ERROR   -1

typedef enum edrx_mode {
	EDRX_DISABLED_URC_DISABLED,     // 0 - eDRX mode is off
	EDRX_ENABLED_URC_DISABLED,      // 1 - eDRX mode is on
	EDRX_ENABLED_URC_ENABLED        // 2 - eDRX mode enabled, unsolicited messages (URC) enabled
}edrx_mode_e;

typedef enum edrx_act_type {
	EC_GSM = 1,         // 1 - EC-GSM-IoT
	GSM = 2,            // 2 - GSM
	THIRD_GEN = 3,      // 3 - 3G
	LTE = 4,            // 4 - LTE, LTE-M
	NB_IOT = 5          // 5 - NB-IoT
} edrx_act_type_e;

char* byte_to_binary_str(uint8_t byte);

/**
 * converts a eDRX byte representation to seconds
 *
 * @param byte - a eDRX timer byte representation
 * @return - time in seconds
 */
int tmo_edrx_timer_byte_to_secs(uint8_t byte, double* time);

/**
 * converts a seconds value to the closet approximation of a eDRX timer
 *
 * @param secs - the seconds to convert
 * @param byte - a byte representation of the eDRX timer type
 * @return - ERANGE - a good representation was not found, TMO_SUCCESS a representation was found
 */
int tmo_edrx_timer_secs_to_approx_byte(double secs, uint8_t* byte);

/**
 * converts seconds to a approximate eDRX string
 *
 * @param secs - the seconds to convert
 * @return - EINVAL - could not do the conversion based on the string
 */
int tmo_edrx_timer_secs_to_str(double secs, char* str);

/**
 *
 * @param val - a eDRX string representation
 * @param secs - a time_t to store the converted value in
 * @return - EINVAL - could not do the conversion based on the string
 */
int tmo_edrx_timer_str_to_secs(const char* val, double* secs);

#endif
