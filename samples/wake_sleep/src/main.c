/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/state.h>

#if 'y' == CONFIG_WIFI
#if 'y' == CONFIG_WIFI_RS9116W
#include <zephyr/drivers/bluetooth/rs9116w.h>
#endif
#endif

// #include <string.h>

#define TIMER_DURATION K_SECONDS(30)
#define TIMER_PERIOD   K_SECONDS(30)
#define PRIORITY       K_PRIO_COOP(5)

#define TIMER_STOP_FN NULL

// #if 'y' == CONFIG_LOG
#include <zephyr/logging/log.h>
// #include <libc/minimal/strerror_table.h>
// #if 'y' == CONFIG_LOG_MODE_MINIMAL
LOG_MODULE_DECLARE(wake_sleep, CONFIG_PM_LOG_LEVEL);
// #endif
// #endif

static struct k_thread thread_id;
void timer_isr(struct k_timer *dummy)
{
	k_thread_resume(&thread_id);
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
		STRUCT_INIT(PM_DEVICE_ACTION_SUSPEND),
		STRUCT_INIT(PM_DEVICE_ACTION_TURN_OFF),
		STRUCT_INIT(PM_DEVICE_ACTION_TURN_ON),
		STRUCT_INIT(PM_DEVICE_ACTION_RESUME),
	};
#undef STRUCT_INIT

	const struct device *devices;
	const size_t n_devices = z_device_get_all_static(&devices);
	struct {
		struct device *on_off_support, *resume_suspend_support;
		struct action {
			bool discovered, supported;
		} action[(sizeof device_action / sizeof device_action[0])];
	} device_action_support[n_devices];

	memset(&device_action_support, 0, sizeof device_action_support);
	for (unsigned char action_index = 0; true;
	     action_index = (action_index + 1) % (sizeof device_action / sizeof device_action[0])) {
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
			     pm_device_state_is_locked(&devices[ii]) ||
			     pm_device_wakeup_is_enabled(&devices[ii]) ||
			     pm_device_runtime_is_enabled(&devices[ii]))) {
				continue;
			}
			if (!device_action_support[ii]
				     .action[device_action[action_index].value]
				     .discovered) {
				LOG_INF("%s(): initial call to pm_device_action_run(%s, %s)",
					__func__, devices[ii].name,
					device_action[action_index].name);
				ret = pm_device_action_run(&devices[ii],
							   device_action[action_index].value);
				if (-ENOSYS == ret) {
					device_action_support[ii].action[PM_DEVICE_ACTION_SUSPEND] =
					device_action_support[ii].action[PM_DEVICE_ACTION_RESUME] =
					device_action_support[ii].action[PM_DEVICE_ACTION_TURN_OFF] =
					device_action_support[ii].action[PM_DEVICE_ACTION_TURN_ON] =
						(struct action){.discovered = true, .supported = false};
				} else {
					device_action_support[ii]
						.action[device_action[action_index].value]
						.discovered = true;
					device_action_support[ii]
						.action[device_action[action_index].value]
						.supported = -ENOTSUP != ret;
				}
			} else if (device_action_support[ii]
					   .action[device_action[action_index].value]
					   .supported) {
				LOG_INF("%s(): subsequent call to pm_device_action_run(%s, %s)",
					__func__, devices[ii].name,
					device_action[action_index].name);
				ret = pm_device_action_run(&devices[ii],
							   device_action[action_index].value);
			} else {
				continue;
			}
			/*
			if (-ENOSYS == ret) {
				pm_device_state_lock(&devices[ii]);
				if (!pm_device_state_is_locked(&devices[ii])) {
					LOG_ERR("Device %s did not enter %s state (%d)",
						devices[ii].name, device_action[action_index].name,
						ret);
				}
			}
			*/

			pm_device_state_get(&devices[ii], &pm_device_state);

			if (ret) {
				LOG_WRN("%s(): %s call status: %d, device state: %s", __func__,
					devices[ii].name, ret,
					pm_device_state_str(pm_device_state));
			} else {
				LOG_INF("%s(): %s call status: %d, device state: %s", __func__,
					devices[ii].name, ret,
					pm_device_state_str(pm_device_state));
			}
		}

		LOG_INF("%s(): asleep\n", __func__);
		k_thread_suspend((struct k_thread *)this_thread);
	}
}

void main(void)
{
	/* Create the power management thread, and schedule it for execution. */
	k_thread_create(
		/* Thread ID and stack specifics */
		&thread_id, thread_stack, K_THREAD_STACK_SIZEOF(thread_stack),
		/* Thread function address and (3) optional parameters */
		pm_thread, &thread_id, NULL, NULL,
		/* Thread priority, options, and scheduling delay */
		K_PRIO_COOP(5), K_INHERIT_PERMS, K_NO_WAIT);

	/* Start a periodic timer. */
	k_timer_start(&pm_timer, TIMER_DURATION, TIMER_PERIOD);
}
