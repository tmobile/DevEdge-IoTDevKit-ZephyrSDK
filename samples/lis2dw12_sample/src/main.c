/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/lis2dw12.h>

#define DOUBLE_TAP_EVENT 0x10U
#define SINGLE_TAP_EVENT 0x08U
#define FREEFALL_EVENT	 0x02U
#define DATAREADY_EVENT	 0x01U

uint8_t reg_value;
struct sensor_value temperature;
bool sample_init_complete = false;

uint8_t status;
struct sensor_trigger trig;

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

	reg_value = 0;
	rc = sensor_attr_get(sensor, trig.chan, SENSOR_ATTR_STATUS, (struct sensor_value *) &reg_value);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register 0X27 :%d\n", rc);
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

	uint8_t reg_array[5] = {0};
	rc = sensor_attr_get(sensor, trig.chan, SENSOR_ATTR_ALL_WAKE_UP_SRC, (struct sensor_value *) &reg_array[0]);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register 0x38 :%d\n", rc);
	} else {
		if (reg_array[1] ) {
			if ((reg_array[1] & 0x20) == 0x20) {
				printf("\tReg 38h - FREE FALL detected (%xh)\n", (reg_array[1]));
			}
		} else {
			printf("\tReg 38h - No WAKE SRC detected (%xh)\n", (reg_array[1]));
		}
	}

	reg_value = 0;
	rc = sensor_attr_get(sensor, trig.chan, SENSOR_ATTR_TAP_SRC, (struct sensor_value *) &reg_value);
	if (rc < 0) {
		printf("\n\tERROR: Unable to read register 0x39 :%d\n", rc);
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

	struct sensor_value temp12bit;
        rc = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp12bit);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read ambient temperature :%d\n", rc);
        } else {
                float deltaT = (int16_t)temp12bit.val1 * 0.0625;
                float temperature16 = (25.0 + deltaT);
                printf("\tAmbient Temperature %.1f deg C %.1f delta %x raw\n", temperature16, deltaT, temp12bit.val1);
        }
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
	       "This sample application demonstrates the integrated DevEdge based motion sensor\n"
	       "(STMicroelectronics LIS2DW12 MEMS accelerometer).\n"
	       "The motion sensor can detect single-tap, double-tap, and free-fall events.\n"
	       "Motion events detected by the LIS2DW12 result in interrupts that trigger\n"
	       "a handler function, which collects and displays specific information about each event.\n\n"
	       "Note:\n"
	       "To generate \"tap\" events, quickly move the board in the X, Y, or Z-axes.\n"
	       "Free-fall events can be simulated with vertical up or down acceleration movements\n"
	       "in which you are simulating a moment of weightlessness and then a sudden increase\n"
	       "in shock (g-forces). For Free-fall try to keep the z-axis pointing straight up.\n\n");
}

void main(void)
{
	greeting();

	k_sleep(K_MSEC(5000));
	const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dw12);
	if (!sensor) {
                return;
	}
	  
#if CONFIG_LIS2DW12_TRIGGER
	
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

	struct sensor_value ctrl4_reg = {
                .val1 = 0x58,
        };

        rc = sensor_attr_set(sensor, trig.chan, SENSOR_ATTR_ENABLE_EVENT_INTERRUPT, &ctrl4_reg);
        if (rc != 0) {
                printf("\tFailed to set odr: %d\n", rc);
                return;
        }

        rc = sensor_attr_get(sensor, trig.chan, SENSOR_ATTR_CHIP_ID, (struct sensor_value *) &status);
        if (rc != 0) {
                printf("\tFailed to get status: %d\n", rc);
                return;
        }

        if (status == 0x44) {
                printf("\tWHOAMI: (%xh)\n", (status));
        } else {
                printf("\tError : WHOAMI (%xh) doesnt match the LIS2DW12 ?\n", (status));
                return;
        }

        rc = sensor_attr_get(sensor, trig.chan, SENSOR_ATTR_STATUS, (struct sensor_value *) &status);
        if (rc != 0) {
                printf("\tFailed to get status: %d\n", rc);
                return;
        }
        printf("\tSENSOR_ATTR_STATUS: (%xh)\n", status);

        /* Let the interrupt handler know the sample is ready to run */
        sample_init_complete = true;

        /* We should be spinning in this loop waiting for the user to trigger us.*/
        printf("\tWaiting here for a motion event trigger\n");
        while (true) {
                k_sleep(K_MSEC(2000));
        }

#else  /* CONFIG_LIS2DW12_TRIGGER */
	while (true) {
		trigger_and_display(sensor);
		k_sleep(K_MSEC(2000));
	}
#endif /* CONFIG_LIS2DW12_TRIGGER */
}



