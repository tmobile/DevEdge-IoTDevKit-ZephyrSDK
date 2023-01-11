/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// uart:~$ tmo pm get
// get: wrong parameter count
// get - Get a device's power management state
// Subcommands:
//   murata_1sc
//   rs9116w@0
//   pwmleds
//   sonycxd5605@24
//   tsl2540@39
// uart:~$

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/state.h>
#include <zephyr/pm/policy.h>
#include <zephyr/drivers/led.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <zephyr/drivers/gpio.h>

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define TIMER_DURATION K_SECONDS(30)
#define TIMER_PERIOD   K_SECONDS(60)

#define SLEEP_DURATION	K_SECONDS(1)
#define THREAD_PRIORITY K_PRIO_COOP(5)

#define TIMER_STOP_FN NULL

#include <zephyr/logging/log.h>
#include <libc/minimal/strerror_table.h>
LOG_MODULE_DECLARE(wake_sleep, CONFIG_PM_LOG_LEVEL);


#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 device-tree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
// static struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

/*
 * Variables pertaining to the interrupt service routine
 */
// static volatile int push_button_isr_count = 0;
// static volatile bool print_edges = false;
static struct k_thread thread_id;


void timer_isr(struct k_timer *dummy)
{
	k_thread_resume(&thread_id);
}


/*
 * User Push Button (sw0) interrupt callback
 */
void user_push_button_intr_callback(const struct device *port, struct gpio_callback *cb,
				    gpio_port_pins_t pin_mask)
{
	k_thread_resume(&thread_id);
}


#ifdef sys_nerr
#define strerror strerror_extended
static const char *strerror(int error_value)
{
	error_value = abs(error_value);
	if (sys_nerr < error_value) {
		return "";
	} else {
		return sys_errlist[error_value];
	}
}
#endif


static void gate_leds(enum pm_device_action pm_device_action)
{
	const struct device *pwm_leds = device_get_binding("pwmleds");

	switch(pm_device_action) {
		case PM_DEVICE_ACTION_SUSPEND:
			led_on(pwm_leds, 0);	/* White */
		break;

		case PM_DEVICE_ACTION_RESUME:
			led_on(pwm_leds, 1);	/* Red */
		break;

		case PM_DEVICE_ACTION_TURN_ON:
			led_on(pwm_leds, 2);	/* Green */
		break;

		case PM_DEVICE_ACTION_TURN_OFF:
			led_on(pwm_leds, 3);	/* Blue */
		break;

		default:
			led_off(pwm_leds, 0);
			led_off(pwm_leds, 1);
			led_off(pwm_leds, 2);
			led_off(pwm_leds, 3);
		break;
	}
}

K_TIMER_DEFINE(pm_timer, timer_isr, TIMER_STOP_FN);
K_THREAD_STACK_DEFINE(thread_stack, 1024);
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
		STRUCT_INIT(PM_DEVICE_ACTION_RESUME),
		STRUCT_INIT(PM_DEVICE_ACTION_SUSPEND),
		STRUCT_INIT(PM_DEVICE_ACTION_TURN_OFF),
		STRUCT_INIT(PM_DEVICE_ACTION_TURN_ON),
	};
#undef STRUCT_INIT
	const size_t device_action_size = sizeof device_action / sizeof *device_action;

	const struct device *devices;
	const size_t n_devices = z_device_get_all_static(&devices);
	struct {
		struct action {
			bool probed:1, supported:1;
		} action[device_action_size];
	} device_info[n_devices];

	memset(&device_info, 0, sizeof device_info);
	for (unsigned char action_index = 0; true;
	     action_index = (action_index + 1) % device_action_size) {
		gate_leds(-1);
		LOG_INF("%s(): asleep\n", __func__);
		k_thread_suspend((struct k_thread *)this_thread);
		gate_leds(device_action[action_index].value);
		LOG_INF("%s(): awake", __func__);


		LOG_INF("device_action[%d].value: %d, device_action[%d].name: %s", action_index,
			device_action[action_index].value, action_index,
			device_action[action_index].name);
		for (size_t ii = 0; ii < n_devices; ii++) {
			int ret;
			enum pm_device_state pm_device_state;

			/*
			 * ignore busy devices, wake up source and devices with
			 * runtime PM enabled.
			 */
			if (devices[ii].pm != NULL &&
			    (pm_device_is_busy(&devices[ii]) ||
			    //  pm_device_state_is_locked(&devices[ii]) ||
			     pm_device_wakeup_is_enabled(&devices[ii]) ||
			     pm_device_runtime_is_enabled(&devices[ii]))) {
				continue;
			}
			if (!device_info[ii]
				     .action[device_action[action_index].value]
				     .probed) {
				LOG_INF("%s(): initial call to pm_device_action_run(%s, %s)",
					__func__, devices[ii].name,
					device_action[action_index].name);
				ret = pm_device_action_run(&devices[ii],
							   device_action[action_index].value);
				switch (ret) {
				case 0:
				case -EALREADY:
					device_info[ii]
					   .action[device_action[action_index].value] =
					   (struct action){.probed = true, .supported = true};
					break;
				case -ENOSYS:
					device_info[ii].action[PM_DEVICE_ACTION_SUSPEND] =
					device_info[ii].action[PM_DEVICE_ACTION_RESUME] =
					device_info[ii].action[PM_DEVICE_ACTION_TURN_OFF] =
					device_info[ii].action[PM_DEVICE_ACTION_TURN_ON] =
						(struct action){.probed = true, .supported = false};
					break;
				case -ENOTSUP:
					device_info[ii]
					   .action[device_action[action_index].value] =
					   (struct action){.probed = true, .supported = false};
					break;
				default:
					// assert(0 != ret);
					__ASSERT(0 != ret, "Unexpected return value: %d (%s)", ret,
						 strerror(ret));
					break;
				}
			} else if (device_info[ii]
					   .action[device_action[action_index].value]
					   .supported) {
				// LOG_INF("%s(): subsequent call to pm_device_action_run(%s, %s)",
				// 	__func__, devices[ii].name,
				// 	device_action[action_index].name);
				ret = pm_device_action_run(&devices[ii],
							   device_action[action_index].value);
			} else {
				continue;
			}

			(void) pm_device_state_get(&devices[ii], &pm_device_state);

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
 *	Set up the GPIO and interrupt service structures
 */
static void setup(void)
{
	int ret;

	puts("Setting up GPIO and IRS structures");

	if (!device_is_ready(button.port)) {
		printk("Error: button device %s is not ready\n", button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret, button.port->name,
		       button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_RISING);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n", ret,
		       button.port->name, button.pin);
		return;
	}
	gpio_init_callback(&button_cb_data, user_push_button_intr_callback, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printf("Set up button at %s pin %d\n\n", button.port->name, button.pin);
}


/*
 * Entry point
 */
void main(void)
{
	/* Set up hardware and IRS routines */
	setup();

	/* Create the power management thread, and schedule it for execution. */
	k_thread_create(
		/* Thread ID and stack specifics */
		&thread_id, thread_stack, K_THREAD_STACK_SIZEOF(thread_stack),
		/* Thread entry point function and (3) option parameters */
		pm_thread, &thread_id, NULL, NULL,
		/* Thread priority, options, and scheduling delay */
		THREAD_PRIORITY, K_INHERIT_PERMS, K_NO_WAIT);

#if defined(TIMER_DURATION) && defined(TIMER_PERIOD)
	/* Start a periodic timer. */
	k_timer_start(&pm_timer, TIMER_DURATION, TIMER_PERIOD);
#endif

#if defined(PM_STATE_SUSPEND_TO_IDLE) && defined(SLEEP_DURATION)
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	k_sleep(SLEEP_DURATION);
#endif
}
