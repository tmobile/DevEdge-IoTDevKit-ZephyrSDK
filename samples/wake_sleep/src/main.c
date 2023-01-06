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

#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/state.h>

#if 0
#if 'y' == CONFIG_WIFI
#if 'y' == CONFIG_WIFI_RS9116W
#include <zephyr/drivers/bluetooth/rs9116w.h>
#endif
#endif
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define TIMER_DURATION K_SECONDS(30)
#define TIMER_PERIOD   K_SECONDS(60)
#define PRIORITY       K_PRIO_COOP(5)

#define TIMER_STOP_FN NULL

#include <zephyr/logging/log.h>
#include <libc/minimal/strerror_table.h>
LOG_MODULE_DECLARE(wake_sleep, CONFIG_PM_LOG_LEVEL);


static const char *strerror_extended(int error_value)
{
	error_value = abs(error_value);
	if (sys_nerr < error_value) {
		return "";
	} else {
		return sys_errlist[error_value];
	}
}


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
		LOG_INF("%s(): asleep\n", __func__);
		k_thread_suspend((struct k_thread *)this_thread);
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
						 strerror_extended(ret));
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
					devices[ii].name, ret, strerror_extended(ret),
					pm_device_state_str(pm_device_state));
			} else {
				LOG_INF("%s(): %s call status: %d (%s), device state: %s", __func__,
					devices[ii].name, ret, strerror_extended(ret),
					pm_device_state_str(pm_device_state));
			}
		}
	}
}

void main(void)
{
	/* Create the power management thread, and schedule it for execution. */
	k_thread_create(
		/* Thread ID and stack specifics */
		&thread_id, thread_stack, K_THREAD_STACK_SIZEOF(thread_stack),
		/* Thread entry point function and (3) option parameters */
		pm_thread, &thread_id, NULL, NULL,
		/* Thread priority, options, and scheduling delay */
		K_PRIO_COOP(5), K_INHERIT_PERMS, K_NO_WAIT);

	/* Start a periodic timer. */
	k_timer_start(&pm_timer, TIMER_DURATION, TIMER_PERIOD);
}
