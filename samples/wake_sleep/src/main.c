/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// uart:~$ tmo pm get
// get: wrong parameter count
// get - Get a device's power manangement state
// Subcommands:
//   murata_1sc
//   rs9116w@0
//   pwmleds
//   sonycxd5605@24
//   tsl2540@39
// uart:~$

#include <zephyr/kernel.h>
#include <zephyr/device.h>
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
// #include <inttypes.h>

#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/state.h>

#include <errno.h>
#include <string.h>

#define TIMER_DURATION K_SECONDS(30)
#define TIMER_PERIOD   K_SECONDS(30)

#include <zephyr/logging/log.h>
// #include <libc/minimal/strerror_table.h>
LOG_MODULE_DECLARE(wake_sleep, CONFIG_PM_LOG_LEVEL);

#define THREAD_STACK_SIZE 1024ul
#define PRIORITY	  K_PRIO_COOP(5)

K_THREAD_STACK_DEFINE(thread_stack, THREAD_STACK_SIZE);
static struct k_thread thread_id;

// #ifdef CONFIG_PM_DEVICE
extern const struct device *__pm_device_slots_start[];

// #if !defined(CONFIG_PM_DEVICE_RUNTIME_EXCLUSIVE)
/* Number of devices successfully suspended. */
// static size_t num_susp;

#if 0
static const char *extended_strerror(int error_value)
{
	/*
	if (0 > error_value) {
		return sys_errlist[-error_value];
	} else {
		return sys_errlist[+error_value];
	}
	*/
	return strerror(0 > error_value ? -error_value : error_value);
}
#endif

#if 0
static int pm_suspend_devices(void)
{
	const struct device *devs;
	size_t devc;

	LOG_INF("%s(): starting", __func__);

	devc = z_device_get_all_static(&devs);

	num_susp = 0;

	for (const struct device *dev = devs + devc - 1; dev >= devs; dev--) {
		int ret;

		/*
		 * ignore busy devices, wake up source and devices with
		 * runtime PM enabled.
		 */
		if (pm_device_is_busy(dev) || pm_device_state_is_locked(dev) ||
		    pm_device_wakeup_is_enabled(dev) ||
		    ((dev->pm != NULL) && pm_device_runtime_is_enabled(dev))) {
			continue;
		}

		ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
		/* ignore devices not supporting or already at the given state */
		if ((ret == -ENOSYS) || (ret == -ENOTSUP) || (ret == -EALREADY)) {
			continue;
		} else if (ret < 0) {
			LOG_ERR("Device %s did not enter %s state (%d)", dev->name,
				pm_device_state_str(PM_DEVICE_STATE_SUSPENDED), ret);
			return ret;
		}

		__pm_device_slots_start[num_susp++] = dev;

		LOG_INF("%s(): %s status: %d", __func__, dev->name, ret);
	}

	LOG_INF("%s(): exiting\n", __func__);
	return 0;
}

static void pm_resume_devices(void)
{
	LOG_INF("%s(): starting", __func__);
	for (int ii = (num_susp - 1); ii >= 0; ii--) {
		int ret = pm_device_action_run(__pm_device_slots_start[ii], PM_DEVICE_ACTION_RESUME);

		LOG_INF("%s(): %s status: %d", __func__, __pm_device_slots_start[ii]->name, ret);
	}

	num_susp = 0;
	LOG_INF("%s(): exiting\n", __func__);
}
// #endif /* !CONFIG_PM_DEVICE_RUNTIME_EXCLUSIVE */
// #endif /* CONFIG_PM_DEVICE */
#endif

void timer_isr(struct k_timer *dummy)
{
	k_thread_resume(&thread_id);
}

K_TIMER_DEFINE(pm_timer, timer_isr, NULL);

static void pm_thread(void *this_thread, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

#if 0
	for (bool wake = false;; wake = !wake) {
		if (wake) {
			pm_resume_devices();
		} else {
			pm_suspend_devices();
		}
		k_thread_suspend((struct k_thread *)this_thread);
	}
#else
#define STRUCT_INIT(enumerator)                                                                    \
	{                                                                                          \
		enumerator, #enumerator                                                            \
	}
	const struct {
		enum pm_device_action value;
		const char *name;
	} device_action[] = {
		STRUCT_INIT(PM_DEVICE_ACTION_TURN_ON),
		STRUCT_INIT(PM_DEVICE_ACTION_RESUME),
		STRUCT_INIT(PM_DEVICE_ACTION_SUSPEND),
		STRUCT_INIT(PM_DEVICE_ACTION_TURN_OFF),
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
				LOG_INF("%s(): initial call to pm_device_action_run(\"%s\", %s)",
					__func__, devices[ii].name,
					device_action[action_index].name);
				ret = pm_device_action_run(&devices[ii],
							   device_action[action_index].value);
				if (-ENOSYS == ret) {
					device_action_support[ii].action[PM_DEVICE_ACTION_SUSPEND] = 
					device_action_support[ii].action[PM_DEVICE_ACTION_RESUME] = 
					device_action_support[ii].action[PM_DEVICE_ACTION_TURN_OFF] = 
					device_action_support[ii].action[PM_DEVICE_ACTION_TURN_ON] = 
						(struct action) {.discovered = true, .supported = false};
				} else {
					device_action_support[ii].action[device_action[action_index].value].discovered = true;
					device_action_support[ii].action[device_action[action_index].value].supported = /* -ENOTSUP != ret */ true;
				}
			} else if (device_action_support[ii]
					   .action[device_action[action_index].value]
					   .supported) {
				LOG_INF("%s(): subsequent call to pm_device_action_run(\"%s\", %s)",
					__func__, devices[ii].name,
					device_action[action_index].name);
				ret = pm_device_action_run(&devices[ii],
							   device_action[action_index].value);
			} else {
				continue;
			}
			/* ignore devices not supporting or already at the given state */
			/* if ((ret == -ENOSYS) || (ret == -ENOTSUP) || (ret == -EALREADY)) {
				continue;
			} else if (ret < 0) {
				LOG_ERR("Device %s did not enter %s state (%d)", devices[ii].name,
					device_action[action_index].name, ret);
			} */
			if (-ENOSYS == ret) {
				pm_device_state_lock(&devices[ii]);
				if (!pm_device_state_is_locked(&devices[ii])) {
					LOG_ERR("Device %s did not enter %s state (%d)",
						devices[ii].name, device_action[action_index].name,
						ret);
				}
			}

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
#endif
}

void main(void)
{
	/* Create and start power management thread. */
	k_thread_create(&thread_id, thread_stack, THREAD_STACK_SIZE, pm_thread, &thread_id, NULL,
			NULL, PRIORITY, K_INHERIT_PERMS, K_NO_WAIT);
	// k_thread_start(&thread_id);

	/* Start a periodic timer. */
	k_timer_start(&pm_timer, TIMER_DURATION, TIMER_PERIOD);

	/*
		struct {
		k_timer timer;
		k_timeout_t duration, period;
	} wake_sleep_timer = {.duration = K_SECONDS(5), .period = K_SECONDS(5)};
	k_timer_start(&wake_sleep_timer.timer, wake_sleep_timer.duration, wake_sleep_timer.period);
	*/
}
