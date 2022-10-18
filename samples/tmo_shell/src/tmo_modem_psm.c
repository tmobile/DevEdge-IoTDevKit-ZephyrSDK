/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include "tmo_modem_psm.h"
#include "tmo_modem_edrx.h"

// <Requested_Periodic-RAU> Periodic RAU (T3312)
// Periodic routing area updating is used to periodically notify the availability 
// of the MS to the network. The procedure is controlled in the MS by the periodic 
// RA update timer, T3312. The value of timer T3312 is sent by the network to the
// MS in the messages ATTACH ACCEPT and ROUTING AREA UPDATE ACCEPT.
// Bits
// 8 7 6
// 0 0 0 value is incremented in multiples of 10 minutes
// 0 0 1 value is incremented in multiples of 1 hour
// 0 1 0 value is incremented in multiples of 10 hours
// 0 1 1 value is incremented in multiples of 2 seconds
// 1 0 0 value is incremented in multiples of 30 seconds
// 1 0 1 value is incremented in multiples of 1 minute

static const int T3312_multipliers[] = { 600, 3600, 36000, 2, 30, 60, -1 };

// <Requested_GPRS-READY-timer> Ready Time (T3314)
// The READY timer, T3314 is used in the MS and in the network per each assigned 
// P-TMSI to control the cell updating procedure
// Bits
// 8 7 6
// 0 0 0 value is incremented in multiples of 2 seconds
// 0 0 1 value is incremented in multiples of 1 minute
// 0 1 0 value is incremented in multiples of decihours (multiples of 6 minutes)
// 1 1 1 value indicates that the timer is deactivated

static const int T3314_multipliers[] = { 2, 60, 360, -1};


// <Requested_Periodic-TAU> Disabled Time (T3412) 
// Bits
// 8 7 6
// 0 0 0 value is incremented in multiples of 10 minutes
// 0 0 1 value is incremented in multiples of 1 hour
// 0 1 0 value is incremented in multiples of 10 hours
// 0 1 1 value is incremented in multiples of 2 secs
// 1 0 0 value is incremented in multiples of 30 secs
// 1 0 1 value is incremented in multiples of 1 minute
// 1 1 0 value is incremented in multiples of 320 hours
// 1 1 1 value indicates that the timer is deactivated

static const int T3412_multipliers[] = { 600, 3600, 36000, 2, 30, 60, 1152000, -1 };

// <Requested_Active-Time> Enabled time (T3324) 
//
// String type. One byte in an 8-bit format. Requested Active Time
// value (T3324_enabled_time) to be allocated to the UE.
// (e.g. "00100100" equals 4 minutes).
// Bits 5 to 1 represent the binary coded timer value.
// Bits 6 to 8 defines the timer value unit for the GPRS timer as
// follows:
// Bits
// 8 7 6
// 0 0 0 value is incremented in multiples of 2 secs
// 0 0 1 value is incremented in multiples of 1 minute
// 0 1 0 value is incremented in multiples of decihours (multiples of 6 minutes)
// 1 1 1 value indicates that the timer is deactivated

static const int T3324_multipliers[] = { 2, 60, 360, -1 };

int tmo_psm_timer_byte_to_secs(Timer_T3xxx_e timer, uint8_t byte)
{
	char* str   = byte_to_binary_str(byte);
	int result  = TMO_ERROR;
	time_t secs = 0;

	result = tmo_psm_timer_str_to_secs(timer, str, &secs);

	if (result < TMO_SUCCESS) {
		return result;
	}

	return secs;
}


int tmo_psm_timer_secs_to_approx_byte(Timer_T3xxx_e timer, time_t secs, uint8_t* byte)
{
	int* tbl_ptr = NULL;
	int best_approx = HOURS_TO_SECS(720);
	int multiplier  = 0;
	int time_val  = 0;
	int best_time = 0;
	int best_mul  = -1;

	*byte = 0;

	if (timer == T3312_periodic_timer) {
		tbl_ptr = (int *)T3312_multipliers;
	} else if (timer == T3314_ready_timer) {
		tbl_ptr = (int *)T3314_multipliers;
	} else if (timer == T3324_active_timer) {
		tbl_ptr = (int *)T3324_multipliers;
	} else if (timer == T3412_disabled_timer) {
		tbl_ptr = (int *)T3412_multipliers;
	}

	if (tbl_ptr == NULL) {
		return -EINVAL;
	}

	for ( ; (tbl_ptr[multiplier] != -1) && (secs != 0); multiplier++) {
		// can the desired value in secs be divided by the current table value

		time_val = secs / tbl_ptr[multiplier];

		if ((time_val > 0) && (time_val <= 0x1F)) {
			// find out how close it comes to the desired value
			int approx = secs - time_val * tbl_ptr[multiplier];

			// is it better than the current closest value?
			if (approx < best_approx) {
				best_mul    = multiplier;
				best_time   = time_val;
				best_approx = approx;
			}
		}
	}

	// no good solution found for requested secs

	if ((best_mul == -1) && (secs != 0)) {
		return -ERANGE;
	}

	// if user requests 0 seconds this is considered a request to deactivate the timer

	if (secs == 0) {
		best_mul  = 7; // timer deactivate
		best_time = 0;
	}

	*byte = (best_mul << 5 | best_time);

	return TMO_SUCCESS;
}


int tmo_psm_timer_secs_to_str(Timer_T3xxx_e timer, time_t secs, char* str)
{
	int result   = TMO_ERROR;
	uint8_t byte = 0;

	// check for max secs values disabled max ~ 1 month, enabled max ~3 hrs

	if ((secs > MINS_TO_SECS(186)) && (timer == T3324_active_timer)) {
		return -EINVAL;
	} else if ((secs > HOURS_TO_SECS(720)) && (timer == T3412_disabled_timer)) {
		return -EINVAL;
	}

	result = tmo_psm_timer_secs_to_approx_byte(timer, secs, &byte);

	if (result < TMO_SUCCESS) {
		return -EINVAL;
	}

	str = byte_to_binary_str(byte);
	return TMO_SUCCESS;
}


int tmo_psm_timer_str_to_secs(Timer_T3xxx_e timer, const char* val, time_t* secs)
{
	int* tbl_ptr;
	unsigned int multipler = 0;
	unsigned int value = 0;

	if (val == NULL) {
		return -EINVAL;
	}

	multipler |= (val[2] == '1') ? 0x01 : 0;
	multipler |= (val[1] == '1') ? 0x02 : 0;
	multipler |= (val[0] == '1') ? 0x04 : 0;

	switch (timer) {
		case T3324_active_timer:
			tbl_ptr = (int *)T3324_multipliers;
			if((multipler < 0) || (multipler > 6)){
				return TMO_ERROR;
			}
			break;
		case T3412_disabled_timer:
			tbl_ptr = (int *)T3412_multipliers;
			if((multipler < 0) || (multipler > 2)){
				return TMO_ERROR;
			}
			break;
		case T3312_periodic_timer:
			tbl_ptr = (int *)T3312_multipliers;
			if((multipler < 0) || (multipler > 6)){
				return TMO_ERROR;
			}
			break;
		case T3314_ready_timer:
			tbl_ptr = (int *)T3314_multipliers;
			if((multipler < 0) || (multipler > 2)){
				return TMO_ERROR;
			}
			break;
		default:
			return TMO_ERROR;
	}

	multipler = tbl_ptr[multipler];

	if (multipler == 7) {
		multipler = value = 0;
	} else {
		value |= (val[7] == '1') ? 0x01 : 0;
		value |= (val[6] == '1') ? 0x02 : 0;
		value |= (val[5] == '1') ? 0x04 : 0;
		value |= (val[4] == '1') ? 0x08 : 0;
		value |= (val[3] == '1') ? 0x10 : 0;
	}

	*secs = (multipler * value);

	return TMO_SUCCESS;
}
