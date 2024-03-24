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
#include <em_cryotimer.h>

#ifdef CONFIG_PM_DEVICE
#include <zephyr/pm/device.h>
#endif

const struct device *tmo_dev_list[] = {DEVICE_DT_GET(DT_NODELABEL(rs9116w)),
				       DEVICE_DT_GET(DT_NODELABEL(sonycxd5605)),
				       DEVICE_DT_GET(DT_NODELABEL(murata_1sc))};

#if CONFIG_PM_DEVICE
static void tmo_pm_sys_lp_wrapper(bool turn_off)
{
	const struct device *dev = NULL;
	for (int i = 0; i < sizeof(tmo_dev_list) / sizeof(tmo_dev_list[0]); i++) {
		dev = tmo_dev_list[i];
		pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
		if (turn_off && dev != DEVICE_DT_GET(DT_NODELABEL(murata_1sc))) {
			pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_OFF);
		}
	}
	dev = PWMLEDS;
	led_off(dev, 0);
	led_off(dev, 1);
	led_off(dev, 2);
	led_off(dev, 3);
	dev = device_get_binding("gnss_pwr");
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
}
#endif

int cmd_pmsysactive(const struct shell *shell, int argc, char **argv)
{
	shell_print(shell, "Entering active state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_ACTIVE, 0, 0});
	return 0;
}

int cmd_pmsysidle(const struct shell *shell, int argc, char **argv)
{
	shell_print(shell, "Entering runtime-idle state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_RUNTIME_IDLE, 0, 0});
	return 0;
}

int cmd_pmsyssuspend(const struct shell *shell, int argc, char **argv)
{
#if CONFIG_PM_DEVICE
	tmo_pm_sys_lp_wrapper(false);
#endif
	shell_print(shell, "Entering suspend-to-idle state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	return 0;
}

int cmd_pmsysstandby(const struct shell *shell, int argc, char **argv)
{
#if CONFIG_PM_DEVICE
	tmo_pm_sys_lp_wrapper(false);
#endif
	shell_print(shell, "Entering standby state...");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_STANDBY, 0, 0});
	return 0;
}

int cmd_pmsysoff(const struct shell *shell, int argc, char **argv)
{
#if CONFIG_PM_DEVICE
	tmo_pm_sys_lp_wrapper(true);
#endif
	shell_print(shell,
		    "Shutting down. Use the reset or user button to bring the board back alive");
	GPIO_IntDisable(0xFFFF);
	GPIO_IntClear(0xFFFFFFFF);
	GPIO_PinModeSet(gpioPortB, 13, gpioModeInputPullFilter, 1);
	GPIO_EM4EnablePinWakeup(BIT(9) << _GPIO_EM4WUEN_EM4WUEN_SHIFT, 0);
	CRYOTIMER_EM4WakeupEnable(false);
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 1, 0});
	return 0;
}
int cmd_pmsysfulloff(const struct shell *shell, int argc, char **argv)
{
#if CONFIG_PM_DEVICE
	tmo_pm_sys_lp_wrapper(true);
#endif
	shell_warn(shell, "Warning RTCC will be lost");
	shell_print(shell,
		    "Shutting down. Use the reset or user button to bring the board back alive");
	GPIO_IntDisable(0xFFFF);
	GPIO_IntClear(0xFFFFFFFF);
	GPIO_PinModeSet(gpioPortB, 13, gpioModeInputPullFilter, 1);
	GPIO_EM4EnablePinWakeup(BIT(9) << _GPIO_EM4WUEN_EM4WUEN_SHIFT, 0);
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
	return 0;
}
