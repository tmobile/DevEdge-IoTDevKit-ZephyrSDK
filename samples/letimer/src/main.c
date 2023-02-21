/*
 * Copyright (c) 2023 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>

void main(void)
{
	printk("Test start\n");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	k_msleep(1000);
	printk("1 second\n");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	k_msleep(1000);
	printk("1 second (2)\n");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	k_msleep(1000);
	printk("1 second (3)\n");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	k_msleep(1000);
	printk("1 second (4)\n");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	k_msleep(1000);
	printk("1 second (5)\n");
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SUSPEND_TO_IDLE, 0, 0});
	k_msleep(60000);
	printk("60 second\n");
}
