/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

bool sample_init_complete = false;


uint8_t reg_value;
struct sensor_value temperature;
struct sensor_value sensor_attr_val;

static void trigger_and_display(const struct device *sensor, const struct sensor_trigger *trig)
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
		/* Sample overrun.	Ignore in polled mode. */
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
	if (trig) {
		if (trig->type == SENSOR_TRIG_TAP) {
			printf("\nSINGLE TAP was detected\n");
		} else if (trig->type == SENSOR_TRIG_DOUBLE_TAP) {
			printf("\nDOUBLE TAP was detected\n");
		} else if (trig->type == SENSOR_TRIG_FREEFALL) {
			printf("\nFREE FALL was detected\n");
		} else if (trig->type == SENSOR_TRIG_DATA_READY) {
			printf("\nDATAREADY was detected\n");
		} else if (trig->type == SENSOR_TRIG_THRESHOLD) {
			printf("\nTHRESHOLD event was detected\n");
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

	if (trig->type == SENSOR_TRIG_TAP || trig->type == SENSOR_TRIG_DOUBLE_TAP) {
		if (trig->chan == SENSOR_CHAN_ACCEL_X) {
			printf("\tTAP_EVENT X-Axis detected\n");
		}
		if (trig->chan == SENSOR_CHAN_ACCEL_Y) {
			printf("\tTAP_EVENT Y-Axis detected\n");
		}
		if (trig->chan == SENSOR_CHAN_ACCEL_Z) {
			printf("\tTAP_EVENT Z-Axis detected\n");
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
	       "Note:\n"
	       "To generate \"tap\" events, quickly move the board in the X, Y, or Z-axes.\n"
	       "Free-fall events can be simulated with vertical up or down acceleration movements\n"
	       "in which you are simulating a moment of weightlessness and then a sudden increase\n"
	       "in shock (g-forces). For Free-fall try to keep the z-axis pointing straight "
	       "up.\n\n");
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

#if CONFIG_LIS2DW12_TRIGGER
	int rc;

	static struct sensor_trigger drdy_trig;
	drdy_trig.type = SENSOR_TRIG_DATA_READY;
	drdy_trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	struct sensor_value odr = {
		.val1 = 2,
	};

	rc = sensor_attr_set(sensor, drdy_trig.chan, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (rc != 0) {
		printf("\tFailed to set odr: %d\n", rc);
		return;
	}

#if CONFIG_LIS2DW12_SAMPLE_DRDY
	rc = sensor_trigger_set(sensor, &drdy_trig, trigger_handler);
#endif

	static struct sensor_trigger ff_trig;
	ff_trig.type = SENSOR_TRIG_FREEFALL;
	ff_trig.chan = SENSOR_CHAN_ACCEL_XYZ;

#if CONFIG_LIS2DW12_TAP
	rc = sensor_trigger_set(sensor, &ff_trig, trigger_handler);

	static struct sensor_trigger dtap_xyz_trig;
	dtap_xyz_trig.type = SENSOR_TRIG_DOUBLE_TAP;
	dtap_xyz_trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	rc = sensor_trigger_set(sensor, &dtap_xyz_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Double Tap trigger: %d\n", rc);
		return;
	}

	static struct sensor_trigger stap_xyz_trig;
	stap_xyz_trig.type = SENSOR_TRIG_TAP;
	stap_xyz_trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	rc = sensor_trigger_set(sensor, &stap_xyz_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Single Tap trigger: %d\n", rc);
		return;
	}

#if CONFIG_LIS2DW12_TAP_3D
	static struct sensor_trigger dtap_x_trig;
	dtap_x_trig.type = SENSOR_TRIG_DOUBLE_TAP;
	dtap_x_trig.chan = SENSOR_CHAN_ACCEL_X;

	rc = sensor_trigger_set(sensor, &dtap_x_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Double Tap X trigger: %d\n", rc);
		return;
	}

	
	static struct sensor_trigger dtap_y_trig;
	dtap_y_trig.type = SENSOR_TRIG_DOUBLE_TAP;
	dtap_y_trig.chan = SENSOR_CHAN_ACCEL_Y;

	rc = sensor_trigger_set(sensor, &dtap_y_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Double Tap Y trigger: %d\n", rc);
		return;
	}

	static struct sensor_trigger dtap_z_trig;
	dtap_z_trig.type = SENSOR_TRIG_DOUBLE_TAP;
	dtap_z_trig.chan = SENSOR_CHAN_ACCEL_Z;

	rc = sensor_trigger_set(sensor, &dtap_z_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Double Tap Z trigger: %d\n", rc);
		return;
	}

	static struct sensor_trigger stap_x_trig;
	stap_x_trig.type = SENSOR_TRIG_TAP;
	stap_x_trig.chan = SENSOR_CHAN_ACCEL_X;

	rc = sensor_trigger_set(sensor, &stap_x_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Single Tap X trigger: %d\n", rc);
		return;
	}

	static struct sensor_trigger stap_y_trig;
	stap_y_trig.type = SENSOR_TRIG_TAP;
	stap_y_trig.chan = SENSOR_CHAN_ACCEL_Y;

	rc = sensor_trigger_set(sensor, &stap_y_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Single Tap Y trigger: %d\n", rc);
		return;
	}

	static struct sensor_trigger stap_z_trig;
	stap_z_trig.type = SENSOR_TRIG_TAP;
	stap_z_trig.chan = SENSOR_CHAN_ACCEL_Z;

	rc = sensor_trigger_set(sensor, &stap_z_trig, trigger_handler);
	if (rc != 0) {
		printf("\tFailed to set Single Tap Z trigger: %d\n", rc);
		return;
	}
#endif /* CONFIG_LIS2DW12_TAP_3D */
#endif /* CONFIG_LIS2DW12_TAP */
#if CONFIG_LIS2DW12_THRESHOLD
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
#endif /* CONFIG_LIS2DW12_THRESHOLD */
	/* Let the interrupt handler know the sample is ready to run */
	sample_init_complete = true;

	/* We should be spinning in this loop waiting for the user to trigger us.*/
	printf("\tWaiting here for a motion event trigger\n");
	while (true) {
		k_sleep(K_MSEC(2000));
	}

#else  /* CONFIG_LIS2DW12_TRIGGER */
	while (true) {
		trigger_and_display(sensor, NULL);
		k_sleep(K_MSEC(2000));
	}
#endif /* CONFIG_LIS2DW12_TRIGGER */
}
