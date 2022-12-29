/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// #define _GNU_SOURCE 1

#include <zephyr/kernel.h>
#include <zephyr/device.h>
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/sys/util.h>
// #include <zephyr/sys/printk.h>
// #include <inttypes.h>

#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/state.h>

#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(wake_sleep, CONFIG_PM_LOG_LEVEL);

// #include <strerror_table.h>

#define THREAD_STACK_SIZE 1024ul
#define PRIORITY	  K_PRIO_COOP(5)

K_THREAD_STACK_DEFINE(thread_stack, THREAD_STACK_SIZE);
static struct k_thread thread_id;

// #ifdef CONFIG_PM_DEVICE
extern const struct device *__pm_device_slots_start[];

// #if !defined(CONFIG_PM_DEVICE_RUNTIME_EXCLUSIVE)
/* Number of devices successfully suspended. */
static size_t num_susp;

static char *extended_strerror(int error_value)
{

    // LOG_INF("%s(): _sys_nerr: %d", _sys_nerr);
	switch (error_value) {
	case -ENOSYS:
		return "Function not implemented";
	case -ENOTSUP:
		return "Unsupported value";
	case -EALREADY:
		return "Operation already in progress";
        // return sys_errlist[-error_value];
	default:
		return strerror(error_value);
	}
}

static int pm_waken_devices(void)
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

		ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
		/* ignore devices not supporting or already at the given state */
		if ((ret == -ENOSYS) || (ret == -ENOTSUP) || (ret == -EALREADY)) {
			LOG_INF("%s(): device name: %s, call status: %d: %s", __func__, dev->name,
				ret, extended_strerror(ret));
			continue;
		} else if (ret < 0) {
			LOG_ERR("Device %s did not enter %s state (%d)", dev->name,
				pm_device_state_str(PM_DEVICE_STATE_SUSPENDED), ret);
			return ret;
		}

		__pm_device_slots_start[num_susp] = dev;
		num_susp++;
	}

	LOG_INF("%s(): exiting\n", __func__);
	return 0;
}

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
			// ENOSYS(88)   : Function not implemented
			// ENOTSUP(134) : Unsupported value
			// EALREADY(120): Operation already in progress
			LOG_INF("%s(): device name: %s, call status: %d: %s", __func__, dev->name,
				ret, extended_strerror(ret));
			continue;
		} else if (ret < 0) {
			LOG_ERR("Device %s did not enter %s state (%d)", dev->name,
				pm_device_state_str(PM_DEVICE_STATE_SUSPENDED), ret);
			return ret;
		}

		__pm_device_slots_start[num_susp] = dev;
		num_susp++;
	}

	LOG_INF("%s(): exiting\n", __func__);
	return 0;
}

static void pm_resume_devices(void)
{
	LOG_INF("%s(): starting", __func__);
	for (int i = (num_susp - 1); i >= 0; i--) {
		int ret = pm_device_action_run(__pm_device_slots_start[i], PM_DEVICE_ACTION_RESUME);

		LOG_INF("%s(): device name: %s, device status: %d: %s", __func__,
			__pm_device_slots_start[i]->name, ret, extended_strerror(ret));
	}

	num_susp = 0;
	LOG_INF("%s(): exiting\n", __func__);
}
// #endif /* !CONFIG_PM_DEVICE_RUNTIME_EXCLUSIVE */
// #endif /* CONFIG_PM_DEVICE */

void timer_fn(struct k_timer *dummy)
{
	k_thread_resume(&thread_id);
}

K_TIMER_DEFINE(my_timer, timer_fn, NULL);

static void thread_fn(void *this_thread, void *p2, void *p3)
{
	for (bool wake = false; true; wake = !wake) {
		if (wake) {
			pm_resume_devices();
		} else {
			pm_suspend_devices();
		}
		k_thread_suspend((struct k_thread *)this_thread);
	}
}

void main(void)
{
	k_thread_create(&thread_id, thread_stack, THREAD_STACK_SIZE, thread_fn, &thread_id, NULL,
			NULL, PRIORITY, K_INHERIT_PERMS, K_FOREVER);
	/* Start a periodic timer that expires once a second. */
	pm_waken_devices();
	k_thread_start(&thread_id);
	k_timer_start(&my_timer, K_SECONDS(5), K_SECONDS(5));
	// k_msleep(1);
}
