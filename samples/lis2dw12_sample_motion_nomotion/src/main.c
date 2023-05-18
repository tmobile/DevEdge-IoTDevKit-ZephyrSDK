/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>

bool sample_init_complete = false;

void nomotion_fn(struct k_timer *timer_id)
{
	printf("\n\nNo motion detected in 10 seconds!\n\n");
	pm_state_force(0, &(struct pm_state_info){PM_STATE_STANDBY, 0, 0});
}

K_TIMER_DEFINE(nomotion_timer, nomotion_fn, NULL);

uint8_t reg_value;
struct sensor_value temperature;
struct sensor_value sensor_attr_val;

static void trigger_and_display(const struct device *sensor, const struct sensor_trigger *trig)
{
	int rc;
#if LIS2DW12_DEBUG
	static unsigned int count;
	struct sensor_value accel[3];
	const char *overrun = "";
	rc = sensor_sample_fetch(sensor);

	if (!sample_init_complete) {
		return;
	}

	++count;
	if (rc == -EBADMSG) {
		/* Sample overrun.	Ignore in polled mode. */
		if (IS_ENABLED(CONFIG_LIS2DW12_TRIGGER)) {
			overrun = "[OVERRUN] ";
		}
		rc = 0;
	}

	if (rc == 0) {
		rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
	}

	if (rc < 0) {
		printf("\tERROR: Update failed: %d\n", rc);
	} else {
		printf("\t#%u @ %u ms: %sx %f , y %f , z %f", count, k_uptime_get_32(), overrun,
		       sensor_value_to_double(&accel[0]), sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
	}
#endif
	if (trig) {
		if (trig->type == SENSOR_TRIG_THRESHOLD) {
			printf("\n\nMotion Detected!\n\n");
			printf("\nTHRESHOLD event was detected\n");
			k_timer_start(&nomotion_timer, K_SECONDS(10), K_NO_WAIT);
		} else {
			printf("\nUNKNOWN event was detected (%d)\n", (trig->type));
		}
	}

	if (trig->type == SENSOR_TRIG_THRESHOLD) {
		if (trig->chan == SENSOR_CHAN_ACCEL_X) {
			printf("\tTHRESHOLD X-Axis detected\n");
		}
		if (trig->chan == SENSOR_CHAN_ACCEL_Y) {
			printf("\tTHRESHOLD Y-Axis detected\n");
		}
		if (trig->chan == SENSOR_CHAN_ACCEL_Z) {
			printf("\tTHRESHOLD Z-Axis detected\n");
		}
	}

	rc = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read ambient temperature :%d\n", rc);
	} else {

		printf("\t\tAmbient Temperature: %.2f C\n", sensor_value_to_double(&temperature));
	}
}

#ifdef CONFIG_LIS2DW12_TRIGGER
static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	trigger_and_display(dev, trig);
	pm_state_force(0, &(struct pm_state_info){PM_STATE_STANDBY, 0, 0});
}
#endif

static void greeting()
{
	printf("\n\t\t\tWelcome to T-Mobile DevEdge!\n\n"
	       "This sample application demonstrates the integrated DevEdge based motion sensor\n"
	       "(STMicroelectronics LIS2DW12 MEMS accelerometer).\n"
	       "The motion sensor can detect single-tap, double-tap, and free-fall events.\n"
	       "Motion events detected by the LIS2DW12 result in interrupts that trigger\n"
	       "a handler function, which collects and displays specific information about each "
	       "event.\n\n"
	       "This sample simply determines between motion/stationary scenarious using these\n"
		   "interrupts.\n");
}

void suspend_lock_all(void)
{
	const struct device *dev;
	size_t len = z_device_get_all_static(&dev);
	const struct device *dev_end = dev + len;

	while (dev < dev_end) {
		if (device_is_ready(dev) && dev->pm) {
			pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
			pm_device_state_lock(dev);
		}
		dev++;
	}
}

void main(void)
{
	k_sleep(K_MSEC(2000));
	greeting();

	k_sleep(K_MSEC(5000));
	const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dw12);
	if (!sensor) {
		return;
	}

	pm_device_state_lock(sensor);

	/* Suspend and lock all other devices */
	suspend_lock_all();

	int rc;

	struct sensor_value odr = {
		.val1 = 2,
	};

	rc = sensor_attr_set(sensor, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (rc != 0) {
		printf("\tFailed to set odr: %d\n", rc);
		return;
	}

	struct sensor_value thresh = {
		.val1 = 0,
		.val2 = 153228,
	};

	rc = sensor_attr_set(sensor, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_LOWER_THRESH, &thresh);
	if (rc != 0) {
		printf("\tFailed to set threshold: %d\n", rc);
		return;
	}


	static struct sensor_trigger thresh_xyz_trig;
	thresh_xyz_trig.type = SENSOR_TRIG_THRESHOLD;
	thresh_xyz_trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	rc = sensor_trigger_set(sensor, &thresh_xyz_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Threshold trigger: %d\n", rc);
		return;
	}

#if CONFIG_LIS2DW12_THRESHOLD_3D
	static struct sensor_trigger thresh_x_trig;
	thresh_x_trig.type = SENSOR_TRIG_THRESHOLD;
	thresh_x_trig.chan = SENSOR_CHAN_ACCEL_X;

	rc = sensor_trigger_set(sensor, &thresh_x_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Threshold X trigger: %d\n", rc);
		return;
	}

	static struct sensor_trigger thresh_y_trig;
	thresh_y_trig.type = SENSOR_TRIG_THRESHOLD;
	thresh_y_trig.chan = SENSOR_CHAN_ACCEL_Y;

	rc = sensor_trigger_set(sensor, &thresh_y_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Threshold Y trigger: %d\n", rc);
		return;
	}

	static struct sensor_trigger thresh_z_trig;
	thresh_z_trig.type = SENSOR_TRIG_THRESHOLD;
	thresh_z_trig.chan = SENSOR_CHAN_ACCEL_Z;

	rc = sensor_trigger_set(sensor, &thresh_z_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Threshold Z trigger: %d\n", rc);
		return;
	}
#endif /* CONFIG_LIS2DW12_THRESHOLD_3D */
	/* Let the interrupt handler know the sample is ready to run */
	sample_init_complete = true;

	/* We should be spinning in this loop waiting for the user to trigger us.*/
	printf("\tWaiting here for a motion event trigger\n");
	k_timer_start(&nomotion_timer, K_SECONDS(10), K_NO_WAIT);
	pm_state_force(0, &(struct pm_state_info){PM_STATE_STANDBY, 0, 0});
	while (true) {
		k_sleep(K_MSEC(30000));
	}

}
