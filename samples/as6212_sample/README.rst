Title: as6212_sample

Description:

An AS6212 Digital Temperature Alert sensor sample to demonstrate the
capabilities of the T-Mobile DevEdge dev kit.

Overview:

This sample is provided as an example to demonstrate the high accuracy AS6212
Digital Temperature sensor with Alert functionality. The sample uses the
AS6212's temperature high/low alerting mechanism to generate interrupts that
awaken threads, which report the temperature data to the console and
subsequently enter EM2 sleep mode.

The alert-sourced interrupts, demonstrate the AS6212's temperature High and Low
threshold alerting capability.

- AS6212 Digital Temperature with Alert functionality.

--------------------------------------------------------------------------------

Requirements
************

-A T-Mobile DevEdge dev kit (https://devedge.t-mobile.com/)
-A Zephyr build environment (https://docs.zephyrproject.org/latest/develop/getting_started/index.html)

Building and Running Project:

How this project can be built:

-Checkout the T-Mobile downstream zephyr repo:
        cd ~/zephyrproject
        git clone https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS zephyr

-Checkout the T-Mobile zephyr-tmo-sdk repo:
        cd ~/zephyrproject
        git clone https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrSDK tmo-zephyr-sdk

-Run 'west update'
        cd ~/zephyrproject
        west update

-Build this sample:
        cd ~/zephyrproject
        west build ~/zephyrproject/DevEdge-IoTDevKit-ZephyrSDK/samples/as6212_sample -p -b tmo_dev_edge
        (substitute your home folder for '<home folder>' in the command above)

-Connect DevEdge dev kit:
	Connect the USB-C port furthest from the pushbutton to your computer. (The
	other USB-C port can be connected; however, it's not used for this sample.)

-Flash as6212_sample
        cd ~/zephyrproject
        west flash

Sample Output
=============

.. code-block:: console
*** Booting Zephyr OS build 4d07b602dd77 ***

                        Welcome to T-Mobile DevEdge!

This application aims to demonstrate the Gecko's Energy Mode 2 (EM2) (Deep Sleep
Mode) and Wake capabilities in conjunction with the temperature interrupt
of DevEdge's (tmo_dev_edge) AMS OSRAM AS6212 Digital Temperature Sensor.

        Set SENSOR_ATTR_UPPER_THRESH (44)
        Set SENSOR_ATTR_LOWER_THRESH (38)

        Set temperature_alert

        Call enable_temp_alerts
        get_temperature(): temperature is 29.2656C

        as6212_thread1(): running
        get_temperature(): temperature is 29.2656C

        as6212_thread2(): running
        get_temperature(): temperature is 29.2656C


Awaiting the AS6212 temperature threshold-high/threshold-low (interrupt) alerts.

While observing the console output, use a hair dryer (or similar forced air
heat source) to momentarily raise the temperature of DevEdge board, triggering
the AS6212 temperature-high alert. Remove the heat source and wait for the
AS6212 temperature-low alert.


as6212_intr_callback(): Received AS6212 Temperature Sensor ALERT Interrupt (1)
        as6212_thread1(): running
        get_temperature(): temperature is 44.0312C

        as6212_thread2(): running
        get_temperature(): temperature is 44.0312C


as6212_intr_callback(): Received AS6212 Temperature Sensor ALERT Interrupt (2)
        as6212_thread1(): running
        get_temperature(): temperature is 37.9766C

        as6212_thread2(): running
        get_temperature(): temperature is 37.9766C

