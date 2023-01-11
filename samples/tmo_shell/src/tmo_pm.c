/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/device.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include "tmo_pm.h"
#include "tmo_gnss.h"


int cmd_pmresume(const struct shell *shell, int argc, char** argv)
{
	int ret;
	const struct device *dev;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(shell, "Device unknown (%s)", argv[1]);
		return -ENODEV;
	}

	enum pm_device_state state;
	ret = pm_device_state_get(dev, &state);

	if (state == PM_DEVICE_STATE_OFF) {
		cmd_pmon(shell, argc, argv);
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
	if (ret == -ENOTSUP) {
		shell_warn(shell, "Device %s does not support action PM_DEVICE_ACTION_RESUME; Command ignored", argv[1]);
	} else if (ret) {
		shell_error(shell, "Failed to execute power management action, err=%d", ret);
	}
	return ret;
}

int cmd_pmsuspend(const struct shell *shell, int argc, char** argv)
{
	int ret;
	const struct device *dev;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(shell, "Device unknown (%s)", argv[1]);
		return -ENODEV;
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret == -ENOTSUP) {
		shell_warn(shell, "Device %s does not support action PM_DEVICE_ACTION_SUSPEND; Command ignored", argv[1]);
	} else if (ret) {
		shell_error(shell, "Failed to execute power management action, err=%d", ret);
	}
	return ret;
}

int cmd_pmoff(const struct shell *shell, int argc, char** argv)
{
	int ret;
	const struct device *dev;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(shell, "Device unknown (%s)", argv[1]);
		return -ENODEV;
	}

	enum pm_device_state state;
	ret = pm_device_state_get(dev, &state);

	if (state == PM_DEVICE_STATE_ACTIVE) {
		cmd_pmsuspend(shell, argc, argv);
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_OFF);
	if (ret == -ENOTSUP) {
		shell_warn(shell, "Device %s does not support action PM_DEVICE_ACTION_TURN_OFF; Command ignored", argv[1]);
	} else if (ret) {
		shell_error(shell, "Failed to execute power management action, err=%d", ret);
	}
	return ret;
}

int cmd_pmon(const struct shell *shell, int argc, char** argv)
{
	int ret;
	const struct device *dev;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(shell, "Device unknown (%s)", argv[1]);
		return -ENODEV;
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_ON);
	if (ret == -ENOTSUP) {
		shell_warn(shell, "Device %s does not support action PM_DEVICE_ACTION_TURN_ON; Command ignored", argv[1]);
	} else if (ret) {
		shell_error(shell, "Failed to execute power management action, err=%d", ret);
	}

	if (!strcmp("CXD5605",argv[1])) {
		gnss_enable_hardware();
	}
	return ret;
}

int cmd_pmget(const struct shell *shell, int argc, char** argv)
{
	int ret;
	const struct device *dev;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(shell, "Device unknown (%s)", argv[1]);
		return -ENODEV;
	}
	enum pm_device_state state;
	ret = pm_device_state_get(dev, &state);
	if (ret) {
		shell_error(shell, "Failed to execute power management action, err=%d", ret);
	} else {
		char* state_str;
		switch (state)
		{
		case PM_DEVICE_STATE_ACTIVE:
			state_str = "ACTIVE";
			break;
		case PM_DEVICE_STATE_SUSPENDED:
			state_str = "SUSPENDED";
			break;
		case PM_DEVICE_STATE_OFF:
			state_str = "OFF";
			break;
		case PM_DEVICE_STATE_SUSPENDING:
			state_str = "SUSPENDING";
			break;
		default:
			state_str = "UNKNOWN";
			break;
		}
		shell_print(shell, "Device is in state %s", state_str);
	}
	
	return ret;
}

const struct device *device_lookup(size_t idx,
				   const char *prefix)
{
	size_t match_idx = 0;
	const struct device *dev;
	size_t len = z_device_get_all_static(&dev);
	const struct device *dev_end = dev + len;

	while (dev < dev_end) {
		if (device_is_ready(dev)
			&& (dev->name != NULL)
			&& (strlen(dev->name) != 0)
			&& ((prefix == NULL)
			|| (strncmp(prefix, dev->name,
					strlen(prefix)) == 0))
			&& dev->pm
			) {
			if (match_idx == idx) {
				return dev;
			}
			++match_idx;
		}
		++dev;
	}

	return NULL;
}

void pm_device_name_get(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = device_lookup(idx, NULL);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help  = NULL;
	entry->subcmd = NULL;
}
