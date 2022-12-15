/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#define LIS2DW12_OUT_T_L                     0x0DU
#define LIS2DW12_OUT_T_H                     0x0EU
#define LIS2DW12_WHO_AM_I                    0x0FU
#define LIS2DW12_CTRL1                       0x20U
#define LIS2DW12_CTRL2                       0x21U
#define LIS2DW12_CTRL3                       0x22U
#define LIS2DW12_CTRL4_INT1_PAD_CTRL         0x23U
#define LIS2DW12_CTRL5_INT2_PAD_CTRL         0x24U
#define LIS2DW12_CTRL6                       0x25U
#define LIS2DW12_OUT_T                       0x26U
#define LIS2DW12_STATUS                      0x27U
#define LIS2DW12_OUT_X_L                     0x28U
#define LIS2DW12_OUT_X_H                     0x29U
#define LIS2DW12_OUT_Y_L                     0x2AU
#define LIS2DW12_OUT_Y_H                     0x2BU
#define LIS2DW12_OUT_Z_L                     0x2CU
#define LIS2DW12_OUT_Z_H                     0x2DU
#define LIS2DW12_FIFO_CTRL                   0x2EU
#define LIS2DW12_FIFO_SAMPLES                0x2FU
#define LIS2DW12_TAP_THS_X                   0x30U
#define LIS2DW12_TAP_THS_Y                   0x31U
#define LIS2DW12_TAP_THS_Z                   0x32U
#define LIS2DW12_INT_DUR                     0x33U
#define LIS2DW12_WAKE_UP_THS                 0x34U
#define LIS2DW12_WAKE_UP_DUR                 0x35U
#define LIS2DW12_FREE_FALL                   0x36U
#define LIS2DW12_STATUS_DUP                  0x37U
#define LIS2DW12_WAKE_UP_SRC                 0x38U
#define LIS2DW12_TAP_SRC                     0x39U
#define LIS2DW12_SIXD_SRC                    0x3AU
#define LIS2DW12_ALL_INT_SRC                 0x3BU
#define LIS2DW12_X_OFS_USR                   0x3CU
#define LIS2DW12_Y_OFS_USR                   0x3DU
#define LIS2DW12_Z_OFS_USR                   0x3EU
#define LIS2DW12_CTRL_REG7                   0x3FU

#define DOUBLE_TAP_EVENT                     0x10U
#define SINGLE_TAP_EVENT                     0x08U
#define FREEFALL_EVENT                       0x02U
#define DATAREADY_EVENT                      0x01U

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
		rc = sensor_channel_get(sensor,
					SENSOR_CHAN_ACCEL_XYZ,
					accel);
	}
	
#if LIS2DW12_DEBUG
	if (rc < 0) {
		printf("\tERROR: Update failed: %d\n", rc);
	} else {
		printf("\t#%u @ %u ms: %sx %f , y %f , z %f",
		       count, k_uptime_get_32(), overrun,
		       sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
	}
#endif

#if LIS2DW12_DEBUG
	reg_value = 0;
        rc = sensor_register_get(sensor, iLIS2DW12_CTRL1, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register :%d\n", rc);
        }
        else {
                printf("\tCTRL1 (20h) (%xh)\n", (reg_value));
        }

	reg_value = 0;
        rc = sensor_register_get(sensor, LIS2DW12_CTRL3, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register :%d\n", rc);
        }
        else {
                printf("\tCTRL3 (22h) (%xh)\n", (reg_value));
        }

	reg_value = 0;
        rc = sensor_register_get(sensor, LIS2DW12_CTRL4_INT1_PAD_CTRL, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register 0x23: %d\n", rc);
        }
        else {
                printf("\tCTRL4 (23h) (%xh)\n", (reg_value));
        }

	reg_value = 0;
        rc = sensor_register_get(sensor, LIS2DW12_CTRL6, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register 0x25: %d\n", rc);
        }
        else {
                printf("\tCTRL6 (25h) (%xh)\n", (reg_value));
        }

	reg_value = 0;
        rc = sensor_register_get(sensor, LIS2DW12_FREE_FALL, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register 0x36: %d\n", rc);
        }
        else {
                printf("\tFREE FALL (36h) (%xh)\n", (reg_value));
        }

#endif

	reg_value = 0; 
        rc = sensor_register_get(sensor, LIS2DW12_STATUS, &reg_value, 1);
	if (rc < 0) {
                printf("\n\tERROR: Unable to read register 0X27 :%d\n", rc);
        } else {

                if ((reg_value & SINGLE_TAP_EVENT) == SINGLE_TAP_EVENT) { 
			printf("\n\tSINGLE TAP was detected (%xh)\n", (reg_value));
		}
		else if ((reg_value & DOUBLE_TAP_EVENT) == DOUBLE_TAP_EVENT) {
			printf("\n\tDOUBLE TAP was detected (%xh)\n", (reg_value));
		}
		else if ((reg_value & FREEFALL_EVENT) == FREEFALL_EVENT) {
                        printf("\n\tFREE FALL was detected (%xh)\n", (reg_value));
                }
		else if ((reg_value & DATAREADY_EVENT) == DATAREADY_EVENT) {
			printf("\n\tDATAREADY was detected (%xh)\n", (reg_value));
		}
		else {
                        printf("\n\tUNKNOWN event was detected (%xh)\n", (reg_value));
                }
        }

	reg_value = 0;
        rc = sensor_register_get(sensor, LIS2DW12_WAKE_UP_SRC, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register 0x38 :%d\n", rc);
        } else {
                if (reg_value) {
			if ((reg_value & 0x20) == 0x20) {
                                printf("\tReg 38h - FREE FALL detected %x\n", (reg_value));
			}
                }
                else {
                        printf("\tReg 38h - No WAKE SRC detected %x\n", (reg_value));
                }
        }

	reg_value = 0;
        rc = sensor_register_get(sensor, LIS2DW12_TAP_SRC, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register 0x39 :%d\n", rc);
        } else {

                if (reg_value) {
			if ((reg_value & 0x01) == 0x01) {
				printf("\tReg 39h - TAP_EVENT  Negative Accel detected (%xh)\n", (reg_value));
			}
			else {
			        printf("\tReg 39h - TAP_EVENT Positive Accel detected (%xh)\n", (reg_value));	
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
                }
		else {
                        printf("\tReg 39h - No TAP_EVENT detected (%xh)\n", (reg_value));
		}
        }

#if LIS2DW12_DEBUG
	reg_value = 0;
        rc = sensor_register_get(sensor, LIS2DW12_ALL_INT_SRC, &reg_value, 1);
        if (rc < 0) {
                printf("\n\tERROR: Unable to read register 0X3B :%d\n", rc);
        } else {

                if (reg_value) {
                        printf("\tReg 3Bh - ALL INT SRC (%xh)\n", (reg_value));
                }
                else {
                        printf("\tReg 3Bh - No ALL INT SRC (%xh)\n", (reg_value));
                }
        }
#endif

}

#ifdef CONFIG_LIS2DW12_TRIGGER
static void trigger_handler(const struct device *dev,
			    const struct sensor_trigger *trig)
{
	trigger_and_display(dev);
}
#endif

void main(void)
{
	const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dw12);

	printf("\n\tWelcome to the T-Mobile interrupt based ST LIS2DW12 motion sensor sample on %s\n\n", CONFIG_BOARD);
	printf("\tThis sample intends to provide a demonstration of using the\n");
        printf("\tST LIS2DW12 motion sensor for providing interrupt based \n");
	printf("\tmotion events such as Single TAP, Double TAP and FREE FALL.\n\n");
	printf("\tNote1: The user needs to move the board in an X, Y or Z-axis\n");
       	printf("\t       side to side snapping action to generate a TAP event.\n");
	printf("\tNote2: For FREE FALL the user needs to move the board in a\n");
        printf("\t       fast straight Vertical UP or DOWN movement.\n\n");

	if (sensor == NULL) {
		printf("\tNo device found\n");
		return;
	}
	if (!device_is_ready(sensor)) {
		printf("\tDevice %s is not ready\n", sensor->name);
		return;
	}

#if CONFIG_LIS2DW12_TRIGGER
	{
		struct sensor_trigger trig;
		int rc;

		trig.type = SENSOR_TRIG_DATA_READY;
		trig.chan = SENSOR_CHAN_ACCEL_XYZ;

		struct sensor_value odr = {
		              .val1 = 1,
		};

		rc = sensor_attr_set(sensor, trig.chan,
		                     SENSOR_ATTR_SAMPLING_FREQUENCY,
			             &odr);
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
                        printf("\n\tERROR: Unable to read register :%d\n", rc);
                }

                reg_value = 0x58;
                rc = sensor_register_set(sensor, LIS2DW12_CTRL4_INT1_PAD_CTRL, &reg_value, 1);
                if (rc < 0) {
                        printf("\n\tERROR: Unable to read register :%d\n", rc);
                }

                reg_value = 0x00;
                /* WAKE_UP_DUR(35h): Bit 7 is MSB of FF duration */
                rc = sensor_register_set(sensor, LIS2DW12_WAKE_UP_DUR, &reg_value, 1);
                if (rc < 0) {
                        printf("\n\tERROR: Unable to read register 0X35 :%d\n", rc);
                }

                reg_value = 0x0f;
                /* FREE_FALL(36h): Set Free-fall duration and threshold */
                rc = sensor_register_set(sensor, LIS2DW12_FREE_FALL, &reg_value, 1);
                if (rc < 0) {
                        printf("\n\tERROR: Unable to read register 0X36 :%d\n", rc);
                }

                /* CTRL1(20h): Set ODR 100Hz,Low-Power mode 1 */
                reg_value = 0x15;
                rc = sensor_register_set(sensor, LIS2DW12_CTRL1, &reg_value, 1);
                if (rc < 0) {
                        printf("\nERROR: Unable to read register :%d\n", rc);
                }

                rc = sensor_register_get(sensor, LIS2DW12_WHO_AM_I, &reg_value, 1);
                if (rc < 0) {
                        printf("\n\tERROR: Unable to read register 0X0F :%d\n", rc);
                }
                else {
                        if (reg_value == 0x44) {
                                printf("\tWHOAMI (%xh)\n", (reg_value));
                        }
                        else{
                                printf("\tError : WHOAMI (%xh) doesnt match the LIS2DW12 ?\n", (reg_value));
                                return;
                        }
                }

                rc = sensor_register_get(sensor, LIS2DW12_OUT_T, &reg_value, 1);
                if (rc < 0) {
                        printf("\n\tERROR: Unable to read register :%d\n", rc);
                }
                else {
                        int8_t temperature = 25 + (signed)reg_value;
                        printf("\tOUT_T Reg 8-bit Temperature 0x26 = %d deg C\n", temperature);
                }

                /* Let the interrupt handler know the sample is ready to run */
                sample_init_complete = true;

		/* We should be spinning in this loop waiting for the user to trigger us.*/
		printf("\tWaiting for an event trigger\n");
		while (true) {
			k_sleep(K_MSEC(2000));
		}
	}
#else /* CONFIG_LIS2DW12_TRIGGER */
	while (true) {
		trigger_and_display(sensor);
		k_sleep(K_MSEC(2000));
	}
#endif /* CONFIG_LIS2DW12_TRIGGER */
}

