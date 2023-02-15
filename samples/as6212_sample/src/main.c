/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/tmp108.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>

#include <zephyr/logging/log.h>
// LOG_MODULE_REGISTER(as6212_sample, LOG_LEVEL_WRN);
LOG_MODULE_REGISTER(as6112_sample, CONFIG_LOG_DEFAULT_LEVEL);

#define INTERRUPT_MODE 0x0200

#define SLEEP_DURATION 2U

/* Thread properties */
#undef TASK_STACK_SIZE
#define TASK_STACK_SIZE 1024ul
#define PRIORITY	K_PRIO_COOP(5)

#define THREAD_SLEEP_TIME 1000ul
#define SEC_TO_MSEC	  1000ul

K_THREAD_STACK_DEFINE(stack_a, TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(stack_b, TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(pm_stack, TASK_STACK_SIZE);

static struct k_thread as6212_a_id;
static struct k_thread as6212_b_id;
static struct k_thread pm_thread_id;
const struct device *as6212;

/*
 * Pushbutton data
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 device-tree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;
static int user_push_button_isr_count = 0;

/*
 * User Push Button (sw0) interrupt service routine
 */
static void user_push_button_intr_callback(const struct device *port, struct gpio_callback *cb,
					   gpio_port_pins_t pin_mask)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pin_mask);

	user_push_button_isr_count++;
	k_thread_resume(&pm_thread_id);
}

/* AS6212 interrupt callback */
static int as6212_int1_int_isr_count = 0;

static void as6212_intr_callback(const struct device *device, const struct sensor_trigger *trigger)
{
	ARG_UNUSED(device);
	ARG_UNUSED(trigger);

	as6212_int1_int_isr_count++;
	k_thread_resume(&as6212_a_id);
	k_thread_resume(&as6212_b_id);
}

#if CONFIG_APP_ENABLE_ONE_SHOT
static void temperature_one_shot(const struct device *dev, const struct sensor_trigger *trigger)
{

	struct sensor_value temp_value;
	int result;

	result = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_value);

	if (result) {
		printf("%s(): error: sensor_channel_get failed: %d\n", __func__, result);
		return;
	}

	printf("%s(): One shot power saving mode enabled, temperature is %gC\n", __func__,
	       sensor_value_to_double(&temp_value));
}
#endif

static void temperature_alert(const struct device *dev, const struct sensor_trigger *trigger)
{

	struct sensor_value temp_flags = {0};

	sensor_attr_get(dev, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_CONFIGURATION, &temp_flags);

	if (INTERRUPT_MODE & temp_flags.val1) {
		puts("\nConfig Reg 0x01: Interrupt Mode: Alerts are Interrupt based!");
	} else {
		puts("\nConfig Reg 0x01: Comparator Mode: Interrupt Mode Alerts are disabled!");
	}
}

#if CONFIG_APP_REPORT_TEMP_ALERTS
static void enable_temp_alerts(const struct device *as6212)
{
	struct sensor_trigger sensor_trigger_type_temp_alert = {.chan = SENSOR_CHAN_AMBIENT_TEMP,
								.type = SENSOR_TRIG_THRESHOLD};

	struct sensor_value alert_upper_thresh;
	sensor_value_from_double(&alert_upper_thresh, CONFIG_APP_TEMP_ALERT_HIGH_THRESH);

	struct sensor_value alert_lower_thresh;
	sensor_value_from_double(&alert_lower_thresh, CONFIG_APP_TEMP_ALERT_LOW_THRESH);

	struct sensor_value thermostat_mode = {0, 0};

	sensor_attr_set(as6212, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_ALERT, &thermostat_mode);

	sensor_attr_set(as6212, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_UPPER_THRESH,
			&alert_upper_thresh);

	printf("\tSet SENSOR_ATTR_UPPER_THRESH (%gC)\n",
	       sensor_value_to_double(&alert_upper_thresh));

	sensor_attr_set(as6212, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_LOWER_THRESH,
			&alert_lower_thresh);

	printf("\tSet SENSOR_ATTR_LOWER_THRESH (%gC)\n",
	       sensor_value_to_double(&alert_lower_thresh));

	sensor_trigger_set(as6212, &sensor_trigger_type_temp_alert, temperature_alert);

	puts("\n\tSet temperature_alert");

	sensor_trigger_set(as6212, &sensor_trigger_type_temp_alert, as6212_intr_callback);
}
#endif

#if CONFIG_APP_ENABLE_ONE_SHOT
static void enable_one_shot(const struct device *as6212)
{

	struct sensor_trigger sensor_trigger_type_temp_one_shot = {.chan = SENSOR_CHAN_AMBIENT_TEMP,
								   .type = SENSOR_TRIG_DATA_READY};

	sensor_attr_set(as6212, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_TMP108_ONE_SHOT_MODE, NULL);

	sensor_trigger_set(as6212, &sensor_trigger_type_temp_one_shot, temperature_one_shot);
}
#endif

static void get_temperature(const struct device *as6212)
{
	int result;
#if 1
	if ((result = sensor_sample_fetch(as6212))) {
		LOG_ERR("error: sensor_sample_fetch failed: %d", result);
	} else {
#endif
		struct sensor_value temp_value;

		if ((result = sensor_channel_get(as6212, SENSOR_CHAN_AMBIENT_TEMP, &temp_value))) {
			LOG_ERR("%s(): error: sensor_channel_get failed: %d", __func__, result);
		} else {
			LOG_INF("%s(): %gC", __func__, sensor_value_to_double(&temp_value));
		}
#if 1
	}
#endif
}

static void pm_thread(void *this_thread, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

#define STRUCT_INIT(enumerator)                                                                    \
	{                                                                                          \
		.pm_state = enumerator, .name = #enumerator                                        \
	}
	const struct {
		enum pm_state pm_state;
		const char *name;
		} pm_state[] = {
			STRUCT_INIT(PM_STATE_ACTIVE),			// Doesn't go to sleep, as expected
			STRUCT_INIT(PM_STATE_RUNTIME_IDLE),		// Doesn't go to sleep
			STRUCT_INIT(PM_STATE_SUSPEND_TO_IDLE),	// Appears to work
			STRUCT_INIT(PM_STATE_STANDBY),			// Appears to work
			STRUCT_INIT(PM_STATE_SUSPEND_TO_RAM),	// Doesn't go to sleep
			STRUCT_INIT(PM_STATE_SUSPEND_TO_DISK),	// Doesn't go to sleep
			// STRUCT_INIT(PM_STATE_SOFT_OFF), 		// Won't wake up

			};
#undef STRUCT_INIT
	const size_t pm_state_size = sizeof pm_state / sizeof *pm_state;
	static int user_push_button_count = 0;


	while (true) {
		int next_state = user_push_button_count % pm_state_size;

		LOG_INF("pm_state_force(%d, %d): %s", next_state, pm_state[next_state].pm_state, pm_state[next_state].name);
		pm_state_force(0u, &(struct pm_state_info){pm_state[next_state].pm_state, 0, 0});

		k_thread_suspend((struct k_thread *)this_thread);
		user_push_button_count++;
		LOG_INF("%s(): running", __func__);
		LOG_INF("%s(): Received user push button interrupt (%d/%d)", __func__,
			user_push_button_count,
			user_push_button_isr_count);
		get_temperature(as6212);
		// k_sleep(K_MSEC(50));	// Debounce
	}
}

static void as6212_thread1(void *this_thread, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_thread_suspend((struct k_thread *)this_thread);
		LOG_INF("%s(): running", __func__);
		LOG_INF("%s(): Received AS6212 Temperature Sensor ALERT Interrupt (%d)", __func__,
			as6212_int1_int_isr_count);
		get_temperature(as6212);
	}
}

static void as6212_thread2(void *this_thread, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_thread_suspend((struct k_thread *)this_thread);
		LOG_INF("%s(): running", __func__);
		LOG_INF("%s(): Received AS6212 Temperature Sensor ALERT Interrupt (%d)", __func__,
			as6212_int1_int_isr_count);
		get_temperature(as6212);
	}
}

/*
 *	Print greeting
 */
static void greeting()
{
	printf("\n\t\t\tWelcome to T-Mobile DevEdge!\n\n"
	       "This application aims to demonstrate the Gecko's Energy Mode 2 (EM2) (Deep Sleep\n"
	       "Mode) and Wake capabilities in conjunction with the temperature interrupt\n"
	       "of DevEdge's (%s) AMS OSRAM AS6212 Digital Temperature Sensor.\n\n",
	       CONFIG_BOARD);
}

/*
 *	Set up the GPIO and interrupt service structures
 */
static void setup(void)
{
	int result;

	as6212 = DEVICE_DT_GET_ANY(ams_as6212);

	if (NULL == as6212) {
		puts("error: no AMS OSRAM AS6212 (ams_as6212) device found");
		return;
	} else if (!device_is_ready(as6212)) {
		puts("error: AMS OSRAM AS6212 (ams_as6212) device not ready");
		return;
	}
	if ((result = sensor_attr_set(as6212, SENSOR_CHAN_AMBIENT_TEMP,
				      SENSOR_ATTR_TMP108_CONTINUOUS_CONVERSION_MODE, NULL))) {
		printf("error: sensor_attr_set(): %d\n", result);
		return;
	}

	if (!device_is_ready(button.port)) {
		LOG_ERR("Error: button device %s is not ready", button.port->name);
		return;
	}

	result = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
	if (result != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d", result, button.port->name,
			button.pin);
		return;
	}

	result = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_RISING);
	if (result != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", result,
			button.port->name, button.pin);
		return;
	}
	gpio_init_callback(&button_cb_data, user_push_button_intr_callback, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	LOG_INF("Set up button at %s pin %d\n", button.port->name, button.pin);

#if CONFIG_APP_ENABLE_ONE_SHOT
	enable_one_shot(as6212);
#endif

#if CONFIG_APP_REPORT_TEMP_ALERTS
	enable_temp_alerts(as6212);
#endif

#if !CONFIG_APP_ENABLE_ONE_SHOT
	get_temperature(as6212);
#endif
}

void main(void)
{
	greeting();
	setup();

	k_thread_create(&as6212_a_id, stack_a, TASK_STACK_SIZE, as6212_thread1, &as6212_a_id, NULL,
			NULL, PRIORITY, K_INHERIT_PERMS, K_NO_WAIT);
	k_thread_create(&as6212_b_id, stack_b, TASK_STACK_SIZE, as6212_thread2, &as6212_b_id, NULL,
			NULL, PRIORITY, K_INHERIT_PERMS, K_NO_WAIT);
	k_thread_create(&pm_thread_id, pm_stack, TASK_STACK_SIZE, pm_thread, &pm_thread_id, NULL,
			NULL, PRIORITY, K_INHERIT_PERMS, K_NO_WAIT);

	puts("\nAwaiting the AS6212 temperature threshold-high/threshold-low (interrupt) "
	     "alerts.\n\n"
	     "While observing the console output, use a hair dryer (or similar forced air\n"
	     "heat source) to momentarily raise the temperature of DevEdge board, triggering\n"
	     "the AS6212 temperature-high alert. Remove the heat source and wait for the\n"
	     "AS6212 temperature-low alert.\n");
#if 1
	while (true) {
		/* Try EM2 mode sleep */
		// pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});

		/*
		 * This will let the idle thread run and let the pm subsystem run in forced state.
		 */

		k_sleep(K_SECONDS(SLEEP_DURATION));
		puts("\nError: Wake from EM2 sleep: We shouldn't ever get here!");
	}
#endif
}
