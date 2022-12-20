.. _lis2dw12:

LIS2DH: Motion Sensor Monitor
#############################

Sample Overview
***************

This sample application periodically reads accelerometer data from the
LIS2DW12 sensor and displays the sensor data on the console. 

Device Overview
***************

The LIS2DW12 is a 3D digital accelerometer system-in-package with a digital I²C/SPI serial interface
standard output, performing at 90 μA in high-resolution mode and below 1 μA in low-power mode.

The device has a dynamic user-selectable full-scale acceleration range of ±2/±4/±8/±16 g and is
capable of measuring accelerations with output data rates from 1.6 Hz to 1600 Hz. The LIS2DW12 can
be configured to generate interrupt signals by using hardware recognition of free-fall events, 6D
orientation, tap and double-tap sensing, activity or inactivity, and wake-up events.

Requirements
************

This sample uses the LIS2DW12, ST MEMS system-in-package featuring a 3D
digital output motion sensor.

References
**********

For more information about the LIS2DW12 motion sensor see
http://www.st.com/en/mems-and-sensors/lis2dw12.html.

Building and Running
********************

The LIS2DW12 or compatible sensors are available on a variety of boards
and shields supported by Zephyr, including:

* :ref:`tmo_dev_edge`

See the board documentation for detailed instructions on how to flash
and get access to the console where acceleration data is displayed.

Building on tmo_dev_edge   
========================

:ref:`tmo_dev_edge` includes an ST LIS2DW12 accelerometer which
supports the LIS2DW12 interface.

.. zephyr-app-commands::
   :zephyr-app: samples/sensor/lis2dw12_sample
   :board: actinius_icarus
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

    Polling at 0.5 Hz
    #1 @ 12 ms: x -5.387328 , y 5.578368 , z -5.463744
    #2 @ 2017 ms: x -5.310912 , y 5.654784 , z -5.501952
    #3 @ 4022 ms: x -5.349120 , y 5.692992 , z -5.463744

   <repeats endlessly every 2 seconds>
