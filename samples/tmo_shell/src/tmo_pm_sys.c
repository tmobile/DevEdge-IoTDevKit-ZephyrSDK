/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/device.h>
#include <zephyr/pm/pm.h>
#include "tmo_pm_sys.h"
#include "tmo_leds.h"

#include <em_emu.h>
#include <em_gpio.h>
// #include <efm32pg12b_gpio.h>

#ifdef CONFIG_PM_DEVICE
#include <zephyr/pm/device.h>
#endif

int cmd_pmsysactive(const struct shell *shell, int argc, char** argv)
{
	shell_print(shell, "Entering active state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_ACTIVE, 0, 0});
	return 0;
}

int cmd_pmsysidle(const struct shell *shell, int argc, char** argv)
{
	shell_print(shell, "Entering runtime-idle state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_RUNTIME_IDLE, 0, 0});
	return 0;
}

int cmd_pmsyssuspend(const struct shell *shell, int argc, char** argv)
{
	shell_print(shell, "Entering suspend-to-idle state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	return 0;
}

int cmd_pmsysstandby(const struct shell *shell, int argc, char** argv)
{
	shell_print(shell, "Entering standby state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_STANDBY, 0, 0});
	return 0;
}

int cmd_pmsysoff(const struct shell *shell, int argc, char** argv)
{
	#if CONFIG_PM_DEVICE
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(rs9116w));
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_OFF);
	dev = DEVICE_DT_GET(DT_NODELABEL(sonycxd5605));
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_OFF);
	dev = DEVICE_DT_GET(DT_NODELABEL(murata_1sc));
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	dev = PWMLEDS;
	led_off(dev, 0);
	led_off(dev, 1);
	led_off(dev, 2);
	led_off(dev, 3);
	#endif
	shell_print(shell, "Shutting down. Use the reset button to bring the board back alive");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 1, 0});    
	return 0;
}
int cmd_pmsysfulloff(const struct shell *shell, int argc, char** argv)
{
	#if CONFIG_PM_DEVICE
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(rs9116w));
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_OFF);
	dev = DEVICE_DT_GET(DT_NODELABEL(sonycxd5605));
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_OFF);
	dev = DEVICE_DT_GET(DT_NODELABEL(murata_1sc));
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	dev = PWMLEDS;
	led_off(dev, 0);
	led_off(dev, 1);
	led_off(dev, 2);
	led_off(dev, 3);
	#endif
	shell_warn(shell, "Warning RTCC will be lost");
	shell_print(shell, "Shutting down. Use the reset button to bring the board back alive");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});    
	return 0;
}
