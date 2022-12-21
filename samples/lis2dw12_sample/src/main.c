/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <lis2dw12_reg.h>

#define DOUBLE_TAP_EVENT 0x10U
#define SINGLE_TAP_EVENT 0x08U
#define FREEFALL_EVENT	 0x02U
#define DATAREADY_EVENT	 0x01U

uint8_t reg_value;
struct sensor_value temperature;
bool sample_init_complete = false;

static void trigger_and_display(const struct device *sensor)
{
	static unsigned int count;
	struct sensor_value accel[3];
	const char *overrun = "";
	int rc = sensor_sample_fetch(sensor);

	if (!sample_init_complete) {
		return;
	}

	++count;
	if (rc == -EBADMSG) {
		/* Sample overrun.  Ignore in polled mode. */
		if (IS_ENABLED(CONFIG_LIS2DW12_TRIGGER)) {
			overrun = "[OVERRUN] ";
		}
		rc = 0;
	}
	if (rc == 0) {
		rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
	}

#if LIS2DW12_DEBUG
	if (rc < 0) {
		printf("\tERROR: Update failed: %d\n", rc);
	} else {
		printf("\t#%u @ %u ms: %sx %f , y %f , z %f", count, k_uptime_get_32(), overrun,
		       sensor_value_to_double(&accel[0]), sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
	}
#endif

#if LIS2DW12_DEBUG
	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_CTRL1, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register :%d\n", rc);
	} else {
		printf("\tCTRL1 (20h) (%xh)\n", (reg_value));
	}

	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_CTRL3, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register :%d\n", rc);
	} else {
		printf("\tCTRL3 (22h) (%xh)\n", (reg_value));
	}

	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_CTRL4_INT1_PAD_CTRL, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register LIS2DW12_CTRL4_INT1_PAD_CTRL: %d\n", rc);
	} else {
		printf("\tCTRL4 (23h) (%xh)\n", (reg_value));
	}

	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_CTRL6, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register LIS2DW12_CTRL6: %d\n", rc);
	} else {
		printf("\tCTRL6 (25h) (%xh)\n", (reg_value));
	}

	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_FREE_FALL, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register LIS2DW12_FREE_FALL: %d\n", rc);
	} else {
		printf("\tFREE FALL (36h) (%xh)\n", (reg_value));
	}
#endif

	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_STATUS, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register LIS2DW12_STATUS:%d\n", rc);
	} else {

		if ((reg_value & SINGLE_TAP_EVENT) == SINGLE_TAP_EVENT) {
			printf("\n\tSINGLE TAP was detected (%xh)\n", (reg_value));
		} else if ((reg_value & DOUBLE_TAP_EVENT) == DOUBLE_TAP_EVENT) {
			printf("\n\tDOUBLE TAP was detected (%xh)\n", (reg_value));
		} else if ((reg_value & FREEFALL_EVENT) == FREEFALL_EVENT) {
			printf("\n\tFREE FALL was detected (%xh)\n", (reg_value));
		} else if ((reg_value & DATAREADY_EVENT) == DATAREADY_EVENT) {
			printf("\n\tDATAREADY was detected (%xh)\n", (reg_value));
		} else {
			printf("\n\tUNKNOWN event was detected (%xh)\n", (reg_value));
		}
	}

	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_WAKE_UP_SRC, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register LIS2DW12_WAKE_UP_SRC:%d\n", rc);
	} else {
		if (reg_value) {
			if ((reg_value & 0x20) == 0x20) {
				printf("\tLIS2DW12_WAKE_UP_SRC - FREE FALL detected %x\n", (reg_value));
			}
		} else {
			printf("\tLIS2DW12_WAKE_UP_SRC - No WAKE SRC detected %x\n", (reg_value));
		}
	}

	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_TAP_SRC, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register LIS2DW12_TAP_SRC:%d\n", rc);
	} else {

		if (reg_value) {
			if ((reg_value & 0x01) == 0x01) {
				printf("\tReg 39h - TAP_EVENT  Negative Accel detected (%xh)\n",
				       (reg_value));
			} else {
				printf("\tReg 39h - TAP_EVENT Positive Accel detected (%xh)\n",
				       (reg_value));
			}
			if ((reg_value & 0x04) == 0x04) {
				printf("\tReg 39h - TAP_EVENT X-Axis detected\n");
			}
			if ((reg_value & 0x02) == 0x02) {
				printf("\tReg 39h - TAP_EVENT Y-Axis detected\n");
			}
			if ((reg_value & 0x01) == 0x01) {
				printf("\tReg 39h - TAP_EVENT Z-Axis detected\n");
			}
		} else {
			printf("\tReg 39h - No TAP_EVENT detected (%xh)\n", (reg_value));
		}
	}

#if LIS2DW12_DEBUG
	reg_value = 0;
	rc = sensor_register_get(sensor, LIS2DW12_ALL_INT_SRC, &reg_value, 1);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register LIS2DW12_ALL_INT_SRC:%d\n", rc);
	} else {

		if (reg_value) {
			printf("\tReg 3Bh - ALL INT SRC (%xh)\n", (reg_value));
		} else {
			printf("\tReg 3Bh - No ALL INT SRC (%xh)\n", (reg_value));
		}
	}
#endif
}

#ifdef CONFIG_LIS2DW12_TRIGGER
static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	trigger_and_display(dev);
}
#endif

static void greeting()
{
	printf("\n\t\t\tWelcome to T-Mobile DevEdge!\n\n"
	       "This sample application aims to demonstrate the (STMicroelectronics LIS2DW12)\n"
	       "motion sensor integrated into T-Mobile's DevEdge. The motion sensor can detect\n"
	       "single-tap, double-tap, and free-fall events. Motion events detected by the\n"
	       "LIS2DW12 result in interrupts that trigger a handler function, which collects\n"
	       "and displays specific information about each event.\n\n"
	       "Note:\n"
	       "To generate \"tap\" events, quickly move the board in the X, Y, or Z-axises.\n"
	       "Free-fall events can be simulated with vertical up or down acceleration\n"
	       "approaching that imparted by Earth's gravitational force.\n\n");
}

void main(void)
{
	const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dw12);

	greeting();

#if CONFIG_LIS2DW12_TRIGGER
	{
		struct sensor_trigger trig;
		int rc;

		trig.type = SENSOR_TRIG_DATA_READY;
		trig.chan = SENSOR_CHAN_ACCEL_XYZ;

		struct sensor_value odr = {
			.val1 = 1,
		};

		rc = sensor_attr_set(sensor, trig.chan, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
		if (rc != 0) {
			printf("\tFailed to set odr: %d\n", rc);
			return;
		}

		/*
		rc = sensor_trigger_set(sensor, &trig, trigger_handler);
		if (rc != 0) {
			printf("\tFailed to set Chan XYZ Data Ready trigger: %d\n", rc);
			return;
		}
		*/

		trig.type = SENSOR_TRIG_DOUBLE_TAP;
		trig.chan = SENSOR_CHAN_ACCEL_XYZ;

		rc = sensor_trigger_set(sensor, &trig, trigger_handler);
		if (rc != 0) {
			printf("\tFailed to set Double Tap trigger: %d\n", rc);
			return;
		}

		trig.type = SENSOR_TRIG_THRESHOLD;
		trig.chan = SENSOR_CHAN_ACCEL_XYZ;

		rc = sensor_trigger_set(sensor, &trig, trigger_handler);
		if (rc != 0) {
			printf("\tFailed to set Threshold trigger: %d\n", rc);
			return;
		}

		reg_value = 0;
		rc = sensor_register_set(sensor, LIS2DW12_CTRL3, &reg_value, 1);
		if (rc < 0) {
			printf("\n\tERROR: Unable to read register LIS2DW12_CTRL3:%d\n", rc);
		}

		reg_value = 0x58;
		rc = sensor_register_set(sensor, LIS2DW12_CTRL4_INT1_PAD_CTRL, &reg_value, 1);
		if (rc < 0) {
			printf("\n\tERROR: Unable to read register LIS2DW12_CTRL4_INT1_PAD_CTRL:%d\n", rc);
		}

		reg_value = 0x00;
		/* WAKE_UP_DUR(35h): Bit 7 is MSB of FF duration */
		rc = sensor_register_set(sensor, LIS2DW12_WAKE_UP_DUR, &reg_value, 1);
		if (rc < 0) {
			printf("\n\tERROR: Unable to read register LIS2DW12_WAKE_UP_DUR:%d\n", rc);
		}

		reg_value = 0x0f;
		/* FREE_FALL(36h): Set Free-fall duration and threshold */
		rc = sensor_register_set(sensor, LIS2DW12_FREE_FALL, &reg_value, 1);
		if (rc < 0) {
			printf("\n\tERROR: Unable to read register LIS2DW12_FREE_FALL:%d\n", rc);
		}

		/* CTRL1(20h): Set ODR 100Hz,Low-Power mode 1 */
		reg_value = 0x15;
		rc = sensor_register_set(sensor, LIS2DW12_CTRL1, &reg_value, 1);
		if (rc < 0) {
			printf("\nERROR: Unable to read register LIS2DW12_CTRL1:%d\n", rc);
		}

		rc = sensor_register_get(sensor, LIS2DW12_WHO_AM_I, &reg_value, 1);
		if (rc < 0) {
			printf("\n\tERROR: Unable to read register LIS2DW12_WHO_AM_I:%d\n", rc);
		} else {
			if (reg_value == 0x44) {
				printf("\tWHOAMI (%xh)\n", (reg_value));
			} else {
				printf("\tError : WHOAMI (%xh) doesnt match the LIS2DW12 ?\n",
				       (reg_value));
				return;
			}
		}

		rc = sensor_register_get(sensor, LIS2DW12_OUT_T, &reg_value, 1);
		if (rc < 0) {
			printf("\n\tERROR: Unable to read register LIS2DW12_OUT_T:%d\n", rc);
		} else {
			int8_t temperature = 25 + (signed)reg_value;
			printf("\tOUT_T Reg 8-bit Temperature LIS2DW12_OUT_T = %d deg C\n", temperature);
		}

		/* Let the interrupt handler know the sample is ready to run */
		sample_init_complete = true;

		/* We should be spinning in this loop waiting for the user to trigger us.*/
		printf("\tWaiting for an event trigger\n");
		while (true) {
			k_sleep(K_MSEC(2000));
		}
	}
#else  /* CONFIG_LIS2DW12_TRIGGER */
	while (true) {
		trigger_and_display(sensor);
		k_sleep(K_MSEC(2000));
	}
#endif /* CONFIG_LIS2DW12_TRIGGER */
}
