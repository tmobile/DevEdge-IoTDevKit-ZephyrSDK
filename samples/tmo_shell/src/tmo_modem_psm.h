/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_MODEM_PSM_H
#define TMO_MODEM_PSM_H

#include <time.h>

#define str_len_nb_psm 33
#define PSM_TIME_LEN 11
#define EDRX_TIME_LEN 6
#define MINS_TO_SECS(x)      ((x) * 60)
#define HOURS_TO_SECS(x)     (MINS_TO_SECS(x) * 60)

typedef enum Timer_T3xxx {
	T3312_periodic_timer,  // the timer to periodically notify the availability of the MS to the network
	T3314_ready_timer,     // the timer used in the MS and in the network per each assigned P-TMSI to control the cell updating procedure
	T3412_disabled_timer,  // the psm 3412 disabled timer
	T3324_active_timer     // the psm 3324 enabled timer
} Timer_T3xxx_e;

//add enum for enable disable mode
typedef enum Psm_Mode {
	disable_psm,
	enable_psm
} Psm_Mode_e;

/**
 * converts a psm byte representation to seconds
 *
 * @param timer - an enabled or disabled psm timer
 * @param byte - a psm timer byte representation
 * @return - time in seconds
 */
int tmo_psm_timer_byte_to_secs(Timer_T3xxx_e timer, uint8_t byte);

/**
 * converts a seconds value to the closet approximation of a psm timer
 *
 * @param timer - an enabled or disabled psm timer
 * @param secs - the seconds to convert
 * @param byte - a byte representation of the psm timer type
 * @return - ERANGE - a good representation was not found, TMO_SUCCESS a representation was found
 */
int tmo_psm_timer_secs_to_approx_byte(Timer_T3xxx_e timer, time_t secs, uint8_t* byte);

/**
 * converts seconds to a approximate psm string
 *
 * @param timer - an enabled or disabled psm timer
 * @param secs - the seconds to convert
 * @return -EINVAL - could not do a conversion, or a representation string
 */
int tmo_psm_timer_secs_to_str(Timer_T3xxx_e timer, time_t secs, char* str);

/**
 *
 * @param timer - which timer this string represents
 * @param val - a psm string representation
 * @param secs - a time_t to store the converted value in
 * @return - EINVAL - could not do the conversion based on the string
 */
int tmo_psm_timer_str_to_secs(Timer_T3xxx_e timer, const char* val, time_t* secs);

#endif
