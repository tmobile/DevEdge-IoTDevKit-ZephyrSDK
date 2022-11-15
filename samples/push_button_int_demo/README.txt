Title: tmo_shell

Description:

A sample to demonstrate the capabilities of the T-Mobile DevEdge dev kit

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

-Build tmo_shell:
	cd ~/zephyrproject
	west build ~/zephyrproject/tmo-zephyr-sdk/samples/tmo_shell -p -b tmo_dev_edge -- -DBOARD_ROOT=<home folder>/zephyrproject/tmo-zephyr-sdk
	(substitute your home folder for '<home folder>' in the command above)

-Connect DevEdge dev kit:
	For DevEdge dev kits running tmo_shell built with DevEdge SDK v1.9.1 or earlier:
		connect BOTH USB-C ports to your computer or wall power
	For DevEdge dev kits running tmo_shell built with DevEdge SDK v1.10.0 or later:
		connect only the USB-C port furthest from the button to your computer
		(both USB-C ports can be connected, but only one needs to be connected)

-Flash tmo_shell:
	cd ~/zephyrproject
	west flash

Sample commands:

tmo_shell contains a great deal of functionality. The tmo menu in tmo_shell looks like this:

uart:~$ tmo
tmo - TMO Shell Commands
Subcommands:
  ble        :BLE test commands
  buzzer     :Buzzer tests
  certs      :CA cert commands
  dfu        :Device FW updates
  dns        :Perform dns lookup
  file       :File commands
  gnsserase  :Erase GNSS chip (CXD5605)
  http       :Get http URL
  ifaces     :List network interfaces
  json       :JSON data options
  location   :Get latitude and longitude
  modem      :Modem status and control
  sockets    :List open sockets
  tcp        :Send/recv TCP packets
  test       :Run automated tests
  udp        :Send/recv UDP packets
  version    :Print version details
  wifi       :WiFi status and control

More details are available here for pre-release:
https://github.com/tmobile/iot-developer-kit/blob/main/documentation/06-Interacting-with-the-Kit-at-CLI-via-the-tmo_shell.md
or here for production:
https://devedge.t-mobile.com/support/iot-documentation/iot-developer-kit/interacting-with-the-kit-at-cli-via-tmo-shell
