/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <errno.h>

#include "tmo_modem_edrx.h"

// AT+CEDRXS=[mode],[AcT-type],[Requested_eDRX_value]

// mode - mode:
// 0 - eDRX mode is off
// 1 - eDRX mode is on
// 2 - eDRX mode enabled, unsolicited messages (URC) enabled

// AcT-type - radio access technology:
// 1 - EC-GSM-IoT
// 2 - GSM
// 3 - 3G
// 4 - LTE, LTE-M
// 5 - NB-IoT

// Requested_eDRX_value in NB-IoT mode	The duration of the period eDRX, edrx_time will be appended with 0 where
//the bit does not represent a valid eDRX cycle length duration.
// Bit
// 4 3 2 1      E-UTRAN eDRX cycle length duration
// 0 0 1 0      20.48 seconds
// 0 0 1 1      40.96 seconds
// 0 1 0 1      81.92 seconds
// 1 0 0 1      163.84 seconds
// 1 0 1 0      327.68 seconds
// 1 0 1 1      655.36 seconds
// 1 1 0 0      1310.72 seconds
// 1 1 0 1      2621.44 seconds
// 1 1 1 0      5242.88 seconds
// 1 1 1 1      10485.76 seconds
static const double edrx_time[] = { 0, 0, 20.48, 40.96, 0, 81.92, 0, 0, 0, 163.84, 327.68, 655.36, 1310.72, 2621.44, 5242.88, 10485.76, -1};

// From tmo_encoding.c
char* byte_to_binary_str(uint8_t byte)
{
	static char buf[8] = { 0 };

	for (int i = 0; i < 8; i++) {
		buf[7 - i] = (byte & 1 << i) ? '1' : '0';
	}

	return buf;
}

int tmo_edrx_timer_byte_to_secs(uint8_t byte, double* time)
{
	char* str   = byte_to_binary_str(byte);
	int result  = TMO_ERROR;
	double secs = 0;

	result = tmo_edrx_timer_str_to_secs(str, &secs);

	if (result < TMO_SUCCESS) {
		return result;
	}

	*time = secs;
	return result;
}

int tmo_edrx_timer_secs_to_approx_byte(double secs, uint8_t* byte)
{
	int index  = 0;
	int index_small_val = 0;
	double time_val_small = 0;
	double time_val_large = 0;

	*byte = 0;

	//0 time is not supported
	if (secs == 0) {
		return -ERANGE;
	}

	for ( ; (edrx_time[index] != -1) && (secs != 0); index++) {
		if(edrx_time[index] != 0) {
			if(secs == edrx_time[index]) {
				*byte = index;
				break;
			}
			if(secs > edrx_time[index]) {
				time_val_small = edrx_time[index];
				index_small_val = index;
			}
			if(secs < edrx_time[index]) {
				time_val_large = edrx_time[index];
				break;
			}
		}    
	}

	if (secs - time_val_small >= time_val_large - secs) {
		*byte = index; 
	} else {
		*byte = index_small_val;
	}

	return TMO_SUCCESS;
}

int tmo_edrx_timer_secs_to_str(double secs, char* str)
{
	int result  = TMO_ERROR;
	uint8_t byte = 0;

	if (secs > MAX_EDRX_TIME) {
		return -EINVAL;
	} 

	result = tmo_edrx_timer_secs_to_approx_byte(secs, &byte);

	if (result < TMO_SUCCESS) {
		return -EINVAL;
	}

	//get last 4 bits
	str = byte_to_binary_str(byte) + 4;

	return TMO_SUCCESS;
}

int tmo_edrx_timer_str_to_secs(const char* val, double* secs)
{
	int value = 0;

	if (val == NULL) {
		return -EINVAL;
	}

	value |= (val[3] == '1') ? 0x01 : 0;
	value |= (val[2] == '1') ? 0x02 : 0;
	value |= (val[1] == '1') ? 0x04 : 0;
	value |= (val[0] == '1') ? 0x08 : 0;

	*secs = edrx_time[value];

	return TMO_SUCCESS;
}
