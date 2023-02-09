.
.
.
=============
Sample output
=============


>> *** Booting Zephyr OS build 596dd391de46 ***

                Welcome to T-Mobile - Internet of Things

This application aims to demonstrate the Gecko's Energy Mode 2 (EM2) (Deep Sleep
Mode) sleep/wake capabilities in conjunction with the high/low illuminance
threshold detection circuitry of the TSL2540 light sensor integrated into the
tmo_dev_edge.


While observing the console output, increase the light illuminating the
tmo_dev_edge's light sensor until the sensor readings at or above the high
threshold are displayed. Reducing the intensity of the light source will cause
the alerts to cease. Casting a shadow over the light sensor will cause sensor
readings at or below the low threshold to be displayed.


Awaiting TSL2540 illuminance threshold-high/threshold-low alerts

[00:00:01.007,000] <inf> illuminance: Set up button at gpio@4000a030 pin 13

        Set SENSOR_ATTR_UPPER_THRESH (1000lx)
        Set SENSOR_ATTR_LOWER_THRESH (10lx)
[00:00:01.259,000] <inf> tsl2540: Interrupt status(0x93): 0x10: AINT
uart:~$
