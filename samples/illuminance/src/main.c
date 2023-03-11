/*
 * Copyright (c) 2023 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/tsl2540.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>

#include <zephyr/logging/log.h>
// LOG_MODULE_REGISTER(illuminance, LOG_LEVEL_INF);
LOG_MODULE_REGISTER(illuminance, CONFIG_LOG_DEFAULT_LEVEL);

/* Thread properties */
K_THREAD_STACK_DEFINE(pm_stack, 1024);
static struct k_thread pm_thread_id;
const struct device *tsl2540;

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

static void fetch_lux(const struct device *tsl2540)
{
	int status;

	if ((status = sensor_sample_fetch(tsl2540))) {
		LOG_ERR("sensor_sample_fetch(%p): %d", tsl2540, status);
	}
}

static void print_lux(const struct device *tsl2540)
{
	int status;
	struct sensor_value sensor_value;

	if ((status = sensor_channel_get(tsl2540, SENSOR_CHAN_LIGHT, &sensor_value))) {
		LOG_ERR("sensor_channel_get(%p, SENSOR_CHAN_LIGHT, %p): %d", tsl2540, &sensor_value,
			status);
	} else {
		printk("%s(): %glx\n", __func__, sensor_value_to_double(&sensor_value));
	}
}

static void next_pm_state()
{
#define STRUCT_INIT(enumerator)                                                                    \
	{                                                                                          \
		.pm_state = enumerator, .name = #enumerator                                        \
	}
	const struct {
		enum pm_state pm_state;
		const char *name;
	} pm_state[] = {
		STRUCT_INIT(PM_STATE_ACTIVE),	       // Doesn't go to sleep, as expected
		STRUCT_INIT(PM_STATE_RUNTIME_IDLE),    // Doesn't go to sleep
		STRUCT_INIT(PM_STATE_SUSPEND_TO_IDLE), // EM2: Appears to work
		STRUCT_INIT(PM_STATE_STANDBY),	       // EM3: Appears to work
		// STRUCT_INIT(PM_STATE_SUSPEND_TO_RAM),  	// Doesn't go to sleep
		// STRUCT_INIT(PM_STATE_SUSPEND_TO_DISK), 	// Doesn't go to sleep
		// STRUCT_INIT(PM_STATE_SOFT_OFF), 			// EM4: Won't wake up
	};
#undef STRUCT_INIT
	const size_t pm_state_size = sizeof pm_state / sizeof *pm_state;
	static int call_count = 0;

	int next_state = call_count++ % pm_state_size;
	const struct pm_state_info *pm_state_info = pm_state_next_get(0);

	printk("pm_state_info->state: %d\n", pm_state_info->state);
	printk("pm_state_force(%d, %d): %s\n", next_state, pm_state[next_state].pm_state,
	       pm_state[next_state].name);
	pm_state_force(0u, &(struct pm_state_info){pm_state[next_state].pm_state, 0, 0});
}

/* TSL2540 interrupt callback */
#if defined(CONFIG_TSL2540_TRIGGER)
static int tsl2540_int1_int_isr_count = 0;
static void tsl2540_intr_callback(const struct device *device, const struct sensor_trigger *trigger)
{
	ARG_UNUSED(device);
	ARG_UNUSED(trigger);

	tsl2540_int1_int_isr_count++;

	sensor_sample_fetch(tsl2540);
	LOG_INF("%s(): Received illuminance sensor ALERT Interrupt (%d)", __func__,
		tsl2540_int1_int_isr_count);
	print_lux(tsl2540);
}

static void configure_sensor_alerts(const struct device *tsl2540)
{
	struct sensor_value alert_thresh;

	sensor_value_from_double(&alert_thresh, CONFIG_APP_LIGHT_ALERT_HIGH_THRESH);
	sensor_attr_set(tsl2540, SENSOR_CHAN_LIGHT, SENSOR_ATTR_UPPER_THRESH, &alert_thresh);
	printk("\tSet SENSOR_ATTR_UPPER_THRESH (%glx)\n", sensor_value_to_double(&alert_thresh));

	sensor_value_from_double(&alert_thresh, CONFIG_APP_LIGHT_ALERT_LOW_THRESH);
	sensor_attr_set(tsl2540, SENSOR_CHAN_LIGHT, SENSOR_ATTR_LOWER_THRESH, &alert_thresh);
	printk("\tSet SENSOR_ATTR_LOWER_THRESH (%glx)\n", sensor_value_to_double(&alert_thresh));

	sensor_trigger_set(
		tsl2540,
		&(struct sensor_trigger){.chan = SENSOR_CHAN_LIGHT, .type = SENSOR_TRIG_THRESHOLD},
		tsl2540_intr_callback);
}
#endif

static void greeting(void)
{
	/*
	 *	Print greeting
	 */

	printk("\n\t\tWelcome to T-Mobile - Internet of Things\n"
	       "\nThis application aims to demonstrate the Gecko's Energy Mode 2 (EM2) (Deep "
	       "Sleep\n"
	       "Mode) sleep/wake capabilities in conjunction with the high/low illuminance\n"
	       "threshold detection circuitry of the TSL2540 light sensor integrated into the\n"
	       "%s.\n\n",
	       CONFIG_BOARD);

	/*
	 * Print instructions
	 */
	printk("\nWhile observing the console output, increase the light illuminating the\n"
	       "%s's light sensor until the sensor readings at or above the high\n"
	       "threshold are displayed. Reducing the intensity of the light source will cause\n"
	       "the alerts to cease. Casting a shadow over the light sensor will cause sensor\n"
	       "readings at or below the low threshold to be displayed.\n\n"
	       "\nAwaiting TSL2540 illuminance threshold-high/threshold-low alerts\n\n",
	       CONFIG_BOARD);
}

/*
 * The following thread facilitates the user with the ability to change the power management mode
 */
static void pm_thread(void *this_thread, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static int user_push_button_count = 0;

	while (true) {
		k_thread_suspend((struct k_thread *)this_thread);
		user_push_button_count++;
		LOG_INF("%s(): Received user push button interrupt (%d/%d)", __func__,
			user_push_button_count, user_push_button_isr_count);
		fetch_lux(tsl2540);
		print_lux(tsl2540);
		next_pm_state();
	}
}

/*
 *	Set up the GPIO and interrupt service structures
 */
static void pushbutton_setup()
{
	int status;

	if (!device_is_ready(button.port)) {
		LOG_ERR("button device %s is not ready", button.port->name);
		return;
	}

	status = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
	if (status) {
		LOG_ERR("Error %d: failed to configure %s pin %d", status, button.port->name,
			button.pin);
		return;
	}

	status = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_RISING);
	if (status) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", status,
			button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, user_push_button_intr_callback, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	LOG_INF("Set up button at %s pin %d\n", button.port->name, button.pin);
}

/*
 * Set up the light sensor
 */
static void sensor_setup(void)
{
	// device ir_sensor = light_sensor = DEVICE_DT_GET(DT_NODELABEL(tsl2540));
	// tsl2540 = DEVICE_DT_GET_ANY(tsl2540);
	tsl2540 = DEVICE_DT_GET(DT_NODELABEL(tsl2540));

	if (NULL == tsl2540) {
		LOG_ERR("AMS OSRAM TSL2540 (ams_tsl2540) device not found");
		return;
	} else if (!device_is_ready(tsl2540)) {
		LOG_ERR("AMS OSRAM TSL2540 (ams_tsl2540) device not ready");
		return;
	}

#if 1
	int status;
	struct sensor_value glass_vis_attenuation, glass_ir_attenuation;
	sensor_value_from_double(&glass_vis_attenuation, 2.27205);
	sensor_value_from_double(&glass_ir_attenuation, 2.34305);

	if ((status = sensor_attr_set(tsl2540, SENSOR_CHAN_LIGHT, SENSOR_ATTR_GAIN,
				      &(struct sensor_value){TSL2540_SENSOR_GAIN_1_2}))) {
		LOG_ERR("setting sensor gain: %d", status);
	} else if ((status = sensor_attr_set(tsl2540, SENSOR_CHAN_LIGHT,
					     SENSOR_ATTR_INTEGRATION_TIME,
					     &(struct sensor_value){500 /* ms */}))) {
		LOG_ERR("setting sensor integration time: %d", status);
	} else if ((status = sensor_attr_set(tsl2540, SENSOR_CHAN_LIGHT,
					     SENSOR_ATTR_GLASS_ATTENUATION,
					     &glass_vis_attenuation))) {
		LOG_ERR("setting sensor visible light attenuation: %d", status);
	} else if ((status = sensor_attr_set(tsl2540, SENSOR_CHAN_IR, SENSOR_ATTR_GLASS_ATTENUATION,
					     &glass_ir_attenuation))) {
		LOG_ERR("setting sensor IR light attenuation: %d", status);
	}
#endif

#if defined(CONFIG_TSL2540_TRIGGER)
	configure_sensor_alerts(tsl2540);
#endif
}

/*
 * Entry point
 */
void main(void)
{
	greeting();

	/*
	 * Configure hardware and interrupt service routines
	 */

	pushbutton_setup();
	sensor_setup();

	/*
	 * Launch the thread facilitating user access to the power management mode
	 */

	k_thread_create(&pm_thread_id, pm_stack, K_THREAD_STACK_SIZEOF(pm_stack), pm_thread,
			&pm_thread_id, NULL, NULL, K_PRIO_COOP(-1), K_INHERIT_PERMS, K_NO_WAIT);
}
