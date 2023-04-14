/*
 * Copyright (c) 2023 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <stdio.h>

#include "button.h"

#define FACTORY_RESET_TIMEOUT_MS (10 * 1000)

static k_tid_t main_thread_id = 0;

/*
 * User callbacks to be called when button is pressed
 */

static void factory_reset_callback(uint32_t timestamp, uint32_t duration, button_press_type type)
{
	printk("%s: Button pressed: timestamp: %u; duration: %u, button press type: %s\n", __func__,
	       timestamp, duration,
	       type == BUTTON_PRESS_TIMEOUT ? "BUTTON_PRESS_TIMEOUT" : "BUTTON_PRESS_NORMAL");

	/* Wakeup the main thread to proceed with other reinint */
	k_wakeup(main_thread_id);
}

/*
 *	Print greeting
 */
static void greeting()
{
	puts("\n\n\t\t\tWelcome to T-Mobile DevEdge!\n\n"
	     "This application aims to demonstrate the button press callback sample module which\n"
	     "uses SW0 interrupt pin, which is connected to the user pushbutton switch of the\n"
	     "DevEdge module.\n");
}

/* Entry point */
void main(void)
{
	int ret = 0;
	greeting();

	main_thread_id = k_current_get();

	/* Initialize the button with the callback and max timeout */
	ret = button_press_module_init(FACTORY_RESET_TIMEOUT_MS, factory_reset_callback);
	if (ret != 0) {
		printk("Error %d: failed to configure button press module\n", ret);
	}

	/* Wait until a callback occurs atleast once */
	k_sleep(K_FOREVER);

	/* For the purposes of demonstrating this sample application to deinit and reinit the button
	 * callback with new parameters, adding another polled method to wait until user has release
	 * the button only AFTER a timeout button event had occurred*/
	static volatile button_press_state state;
	if (button_press_get_state() == BUTTON_STATE_TIMEOUT) {
		do {
			state = button_press_get_state();
		} while (state != BUTTON_STATE_INIT);
	}

	/* If previously configured, deinit the button callback here to demonstrate how to change
	 * the parameters*/
	if (button_press_get_state() == BUTTON_STATE_INIT) {
		ret = button_press_module_deinit();
		if (ret != 0) {
			printk("Error %d: failed to deinit button callback module\n", ret);
		}
	}

	/* Initialize the button callback with new parameters after successful deinit */
	if (button_press_get_state() == BUTTON_STATE_DEINIT) {
		ret = button_press_module_init(FACTORY_RESET_TIMEOUT_MS / 2,
					       factory_reset_callback);
		if (ret != 0) {
			printk("Error %d: failed to configure button press module\n", ret);
		}
	}
}
