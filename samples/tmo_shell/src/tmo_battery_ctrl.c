/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "tmo_battery_ctrl.h"

#define TABLE_LEN 11
#define TABLE_VOLTAGE 0
#define TABLE_PERCENTAGE 1

/** According to NSEP-720 Jm-battery-modeling.xlsx **/

// 100% value based on actual new battery measurement
static float battery_discharging_tbl [TABLE_LEN][2] = {
	{4.160, 100.0 },
	{4.072, 90.0  },
	{3.992, 80.0  },
	{3.923, 70.0  },
	{3.858, 60.0  },
	{3.806, 50.0  },
	{3.775, 40.0  },
	{3.752, 30.0  },
	{3.729, 20.0  },
	{3.686, 10.0  },
	{3.632,  0.0  }};

// 100% value based on actual new battery measurement
// 90% value based on an old dev board only getting to 4.135V fully charged
static float battery_charging_tbl [TABLE_LEN][2] = {
	{4.180, 100.0 },
	{4.130, 90.0  },
	{4.071, 80.0  },
	{4.005, 70.0  },
	{3.934, 60.0  },
	{3.886, 50.0  },
	{3.853, 40.0  },
	{3.833, 30.0  },
	{3.800, 20.0  },
	{3.741, 10.0  },
	{3.597,  0.0  }};

// extern bool battery_is_charging; The following line needs to be placed in the battery charging code
bool battery_is_charging = false;

float get_remaining_capacity(float battery_voltage)
{
	float (*voltage_capacity_table)[TABLE_LEN][2];
	float high_voltage = 0.0;
	float low_voltage = 0.0;
	uint32_t i;

	if (battery_is_charging)
		voltage_capacity_table = &battery_charging_tbl;
	else
		voltage_capacity_table = &battery_discharging_tbl;

	// check battery voltage limits

	if (battery_voltage >= (*voltage_capacity_table)[0][TABLE_VOLTAGE]) {
		return 100.0;
	}

	if (battery_voltage <= (*voltage_capacity_table)[TABLE_LEN - 1][TABLE_VOLTAGE]) {
		return 0.0;
	}

	// find the table percentage entries that correspond
	// to the voltage and interpolate between the two

	for (i = 1; i < TABLE_LEN; i++) {
		if (battery_voltage > (*voltage_capacity_table)[i][TABLE_VOLTAGE]) {
			high_voltage = (*voltage_capacity_table)[i - 1][TABLE_VOLTAGE];
			low_voltage = (*voltage_capacity_table)[i][TABLE_VOLTAGE];
			break;
		}
	}

	if ((i >= TABLE_LEN) || (high_voltage == low_voltage)) {
		return 0.0;
	}

	// make sure the averaging algorithm zeros the return value if
	// the voltage capacity is less than 10 percent

	float pct_gap = (float) (*voltage_capacity_table)[i-1][TABLE_PERCENTAGE] - (*voltage_capacity_table)[i][TABLE_PERCENTAGE];
	return ((battery_voltage - low_voltage) / (high_voltage - low_voltage) * pct_gap) + (*voltage_capacity_table)[i][TABLE_PERCENTAGE];
}

void apply_filter(float *bv)
{
	static float s_filtered_capacity = -1;
	static bool s_battery_is_charging = false;

	// If there has been a switch between charger and battery, reset the filter
	if (s_battery_is_charging != battery_is_charging) {
		s_battery_is_charging = battery_is_charging;
		s_filtered_capacity = -1;
	}

	if (s_filtered_capacity < 0) {
		s_filtered_capacity = *bv;
	}
	*bv = s_filtered_capacity = s_filtered_capacity * 0.95 + (*bv) * 0.05;
}
