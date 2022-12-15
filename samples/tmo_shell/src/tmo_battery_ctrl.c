/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <zephyr/devicetree.h>

#if DT_NODE_EXISTS(DT_NODELABEL(bq24250))
#include "tmo_bq24250.h"
#endif

#include "tmo_battery_ctrl.h"

#define TABLE_LEN 11
#define TABLE_VOLTAGE 0
#define TABLE_PERCENT 1

// 100% value based on actual new battery measurement
static float battery_discharging_tbl [TABLE_LEN][2] = {
	{4.145, 100.0 },
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
static float battery_charging_tbl [TABLE_LEN][2] = {
	{4.175, 100.0 },
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

bool is_battery_charging()
{
	uint8_t charging;
	uint8_t vbus;
	uint8_t attached;
	uint8_t fault;

	get_battery_charging_status(&charging, &vbus, &attached, &fault);
	return (vbus && charging);
}

float get_remaining_capacity(float battery_voltage)
{
	float (*voltage_capacity_table)[TABLE_LEN][2];
	float high_voltage = 0.0;
	float low_voltage = 0.0;
	uint32_t i;

	if (is_battery_charging())
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

	float pct_gap = (float) (*voltage_capacity_table)[i-1][TABLE_PERCENT] - (*voltage_capacity_table)[i][TABLE_PERCENT];
	return ((battery_voltage - low_voltage) / (high_voltage - low_voltage) * pct_gap) + (*voltage_capacity_table)[i][TABLE_PERCENT];
}

#if DT_NODE_EXISTS(DT_NODELABEL(pmic))
extern int get_pmic_status(uint8_t *charging, uint8_t *vbus, uint8_t *attached, uint8_t *fault, uint8_t *charge_status);
#endif

int get_battery_charging_status(uint8_t *charging, uint8_t *vbus, uint8_t *attached, uint8_t *fault)
{
#if DT_NODE_EXISTS(DT_NODELABEL(bq24250))
	int status = get_bq24250_status(charging, vbus, attached, fault);
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(pmic))
	uint8_t charge_status;
	int status = get_pmic_status(charging, vbus, attached, fault, &charge_status);
#endif

	return status;
}
