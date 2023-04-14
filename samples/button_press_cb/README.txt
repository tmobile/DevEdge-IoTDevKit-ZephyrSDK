Title: button_press_cb

Description:

A sample to demonstrate a callback feature of the user push button on the DevEdge kit
The user will implement the Button callback function in the application code.
It gives the timestamp of the button pressed (from system uptime in millisecs), the duration of
how long the button was pressed (also in millisecs), and the type of button press, whether a normal or a timeout.

The duration parameter will be the max timeout value when a timeout occurs, this won't represent
the real physical button press duration though. The callback will be invoked as soon as the
timeout reaches even if the user has not release the button.

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

-Build button_press_cb:
	cd ~/zephyrproject
	west build ~/zephyrproject/tmo-zephyr-sdk/samples/button_press_cb -p -b tmo_dev_edge -- -DBOARD_ROOT=<home folder>/zephyrproject/tmo-zephyr-sdk
	(substitute your home folder for '<home folder>' in the command above)

-Connect DevEdge dev kit:
	For DevEdge dev kits running tmo_shell built with DevEdge SDK v1.9.1 or earlier:
		connect BOTH USB-C ports to your computer or wall power
	For DevEdge dev kits running tmo_shell built with DevEdge SDK v1.10.0 or later:
		connect only the USB-C port furthest from the button to your computer
		(both USB-C ports can be connected, but only one needs to be connected)

-Flash button_press_cb:
	cd ~/zephyrproject
	west flash

Sample output:
```
*** Booting Zephyr OS build ccdf94636486 ***


                        Welcome to T-Mobile DevEdge!

This application aims to demonstrate the button press callback sample module which
uses SW0 interrupt pin, which is connected to the user pushbutton switch of the
DevEdge module.

Set up button at gpio@4000a030 pin 13

factory_reset_callback: Button pressed: timestamp: 2562; duration: 10000, button press type: BUTTON_PRESS_TIMEOUT
Set up button at gpio@4000a030 pin 13

factory_reset_callback: Button pressed: timestamp: 14983; duration: 159, button press type: BUTTON_PRESS_NORMAL
factory_reset_callback: Button pressed: timestamp: 15920; duration: 137, button press type: BUTTON_PRESS_NORMAL
factory_reset_callback: Button pressed: timestamp: 16690; duration: 5000, button press type: BUTTON_PRESS_TIMEOUT
factory_reset_callback: Button pressed: timestamp: 22771; duration: 139, button press type: BUTTON_PRESS_NORMAL
factory_reset_callback: Button pressed: timestamp: 23757; duration: 179, button press type: BUTTON_PRESS_NORMAL
```

