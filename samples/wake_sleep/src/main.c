/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(wake_sleep, CONFIG_PM_LOG_LEVEL);

#include <stdio.h>
#include <string.h>

/*
 * TODO: Remove the following workaround, and replace its functionality with a permanent, standard
 * solution.
 */
#include "strerror.h"

/*
 * Thread defines
 */
#define THREAD_PRIORITY K_PRIO_COOP(5)

/*
 * Deep-sleep defines
 */
#define SLEEP_DURATION K_MSEC(1)
#define SLEEP_MODE     PM_STATE_RUNTIME_IDLE

/*
 * Pushbutton defines and data
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 device-tree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

/*
 * LED data
 */
const struct device *pwm_leds;

/*
 * Thread data
 */
static struct k_thread thread_id;

/*
 * Timer interrupt service routine
 */
#if defined(CONFIG_WAKE_TIMER_DURATION) && defined(CONFIG_WAKE_TIMER_PERIOD)
#if (0 != CONFIG_WAKE_TIMER_DURATION) && (0 != CONFIG_WAKE_TIMER_PERIOD)
static void timer_isr(struct k_timer *dummy)
{
	k_thread_resume(&thread_id);
}
#define TIMER_STOP_FN NULL
static K_TIMER_DEFINE(pm_timer, timer_isr, TIMER_STOP_FN);
#endif
#endif

/*
 * User Push Button (sw0) interrupt service routine
 */
static void user_push_button_intr_callback(const struct device *port, struct gpio_callback *cb,
					   gpio_port_pins_t pin_mask)
{
	k_thread_resume(&thread_id);
}

/*
 * Gate the various LEDs on and off
 */
static void gate_leds(enum pm_device_action pm_device_action)
{

	switch (pm_device_action) {
	case PM_DEVICE_ACTION_SUSPEND:
		puts("Turn on white LED");
		led_set_brightness(pwm_leds, 0, 100);
		led_on(pwm_leds, 0); /* White */
		break;

	case PM_DEVICE_ACTION_RESUME:
		puts("Turn on red LED");
		led_set_brightness(pwm_leds, 1, 100);
		led_on(pwm_leds, 1); /* Red */
		break;

	case PM_DEVICE_ACTION_TURN_ON:
		puts("Turn on green LED");
		led_set_brightness(pwm_leds, 2, 100);
		led_on(pwm_leds, 2); /* Green */
		break;

	case PM_DEVICE_ACTION_TURN_OFF:
		puts("Turn on blue LED");
		led_set_brightness(pwm_leds, 3, 100);
		led_on(pwm_leds, 3); /* Blue */
		break;

	default:
		// puts("Turn off LEDs");
		led_off(pwm_leds, 0);
		led_off(pwm_leds, 1);
		led_off(pwm_leds, 2);
		led_off(pwm_leds, 3);
		led_set_brightness(pwm_leds, 0, 0);
		led_set_brightness(pwm_leds, 1, 0);
		led_set_brightness(pwm_leds, 2, 0);
		led_set_brightness(pwm_leds, 3, 0);
		break;
	}
}

/*
 * The power management thread
 */
static K_THREAD_STACK_DEFINE(thread_stack, 1024);
static void pm_thread(void *this_thread, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

#define STRUCT_INIT(enumerator)                                                                    \
	{                                                                                          \
		enumerator, #enumerator                                                            \
	}
	const struct {
		enum pm_device_action value;
		const char *name;
	} device_action[] = {
		STRUCT_INIT(PM_DEVICE_ACTION_SUSPEND), STRUCT_INIT(PM_DEVICE_ACTION_TURN_OFF),
		STRUCT_INIT(PM_DEVICE_ACTION_TURN_ON), STRUCT_INIT(PM_DEVICE_ACTION_RESUME)};
#undef STRUCT_INIT
	const size_t device_action_size = sizeof device_action / sizeof *device_action;

	const struct device *devices;
	const size_t n_devices = z_device_get_all_static(&devices);

	for (unsigned char action_index = 0; true;
	     action_index = (action_index + 1) % device_action_size) {

		gate_leds(-2);
		k_thread_suspend((struct k_thread *)this_thread);
		printf("%s(): awake\n", __func__);
		gate_leds(device_action[action_index].value);

		printf("device_action[%d].value: %d, device_action[%d].name: %s\n", action_index,
		       device_action[action_index].value, action_index,
		       device_action[action_index].name);
		for (size_t ii = 0; ii < n_devices; ii++) {
			int ret;
			enum pm_device_state pm_device_state;

			/*
			 * Ignore busy devices, wake up source and devices with
			 * runtime PM enabled.
			 */
			if (devices[ii].pm == NULL || pm_device_is_busy(&devices[ii]) ||
			    pm_device_runtime_is_enabled(&devices[ii])) {
				continue;
			}

			ret = pm_device_action_run(&devices[ii], device_action[action_index].value);
			(void)pm_device_state_get(&devices[ii], &pm_device_state);

			if (ret) {
				LOG_WRN("%s(): %s call status: %d (%s), device state: %s", __func__,
					devices[ii].name, ret, strerror(ret),
					pm_device_state_str(pm_device_state));
			} else {
				LOG_INF("%s(): %s call status: %d (%s), device state: %s", __func__,
					devices[ii].name, ret, strerror(ret),
					pm_device_state_str(pm_device_state));
			}
		}

		gate_leds(-1);
		printf("%s(): asleep\n\n", __func__);
	}
}

/* Prevent the deep sleep (non-recoverable shipping mode) from being entered on
 * long timeouts or `K_FOREVER` due to the default residency policy.
 *
 * This has to be done before anything tries to sleep, which means
 * before the threading system starts up between PRE_KERNEL_2 and
 * POST_KERNEL.  Do it at the start of PRE_KERNEL_2.
 */
static int disable_deep_sleep(const struct device *dev)
{
	ARG_UNUSED(dev);

	pm_policy_state_lock_get(PM_STATE_SOFT_OFF, PM_ALL_SUBSTATES);
	return 0;
}
SYS_INIT(disable_deep_sleep, PRE_KERNEL_2, 0);

/*
 * Print usage information
 */
static void greeting()
{
	puts("\n\n\t\t\tWelcome to T-Mobile DevEdge!\n\n"
	     "This application aims to demonstrate the Gecko's Energy Mode 2 (EM2) (Deep\n"
	     "Sleep Mode) and Wake capabilities in conjunction with the SW0 interrupt pin,\n"
	     "which is connected to the user pushbutton switch of the DevEdge module.\n\n"
	     "Press and release the user pushbutton to advance from one power management\n"
	     "mode to the next.\n");
}

/*
 *	Set up the GPIO and interrupt service structures
 */
static void setup(void)
{
	int ret;

	LOG_INF("Setting up GPIO and ISR structures");

	pwm_leds = device_get_binding("pwmleds");

	if (!device_is_ready(button.port)) {
		LOG_ERR("Error: button device %s is not ready", button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d", ret, button.port->name,
			button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_RISING);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret,
			button.port->name, button.pin);
		return;
	}
	gpio_init_callback(&button_cb_data, user_push_button_intr_callback, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	LOG_INF("Set up button at %s pin %d\n", button.port->name, button.pin);
}

/*
 * Entry point
 */
void main(void)
{
	/* Set up hardware and interrupt service routines */
	setup();

	/* Print usage information */
	greeting();

	/* Create the power management thread, and schedule it for execution. */
	k_thread_create(
		/* Thread ID and stack specifics */
		&thread_id, thread_stack, K_THREAD_STACK_SIZEOF(thread_stack),
		/* Thread entry point function and (3) option parameters */
		pm_thread, &thread_id, NULL, NULL,
		/* Thread priority, options, and scheduling delay */
		THREAD_PRIORITY, K_INHERIT_PERMS, K_NO_WAIT);

#if defined(CONFIG_WAKE_TIMER_DURATION) && defined(CONFIG_WAKE_TIMER_PERIOD)
#if (0 == CONFIG_WAKE_TIMER_DURATION) || (0 == CONFIG_WAKE_TIMER_PERIOD)
#warning Timer disabled
#else
#if (15 > CONFIG_WAKE_TIMER_DURATION)
#error Timer duration must be 15 seconds or more
#elif (20 > CONFIG_WAKE_TIMER_PERIOD)
#error Timer period must be 20 seconds or more
#else
	/* Start the power management periodic timer. */
	k_timer_start(&pm_timer, K_SECONDS(CONFIG_WAKE_TIMER_DURATION),
		      K_SECONDS(CONFIG_WAKE_TIMER_PERIOD));
#endif
#endif
#endif

#ifdef SLEEP_DURATION
	pm_state_force(0u, &(struct pm_state_info){SLEEP_MODE, 0, 0});
	k_sleep(SLEEP_DURATION);
#endif
}
