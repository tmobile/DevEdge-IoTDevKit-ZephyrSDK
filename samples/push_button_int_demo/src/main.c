/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <soc.h>
#include <drivers/gpio.h>

#define GECKO_CONSOLE DT_LABEL(DT_CHOSEN(zephyr_console))
#define BUSY_WAIT_DURATION 2U
#define SLEEP_DURATION 2U

#define PUSH_BUTTON_GPIO_B_NAME  "GPIO_B"
#define PUSH_BUTTON_INT1              13   /* PB13 - configured here */

static const struct device *gpiob_dev;

/* User Push Button (sw0) interrupt callback */
static struct gpio_callback gpio_cb;
int  push_button_isr_count = 0;
bool push_button_isr_state_change = false;

void user_push_button_intr_callback(const struct device *port,
                struct gpio_callback *cb, uint32_t pins)
{
        push_button_isr_count++;
        push_button_isr_state_change = true;
        printf("\nPush Button (sw0) Interrupt detected\n");
}

/* Prevent the deep sleep (non-recoverable shipping mode) from being entered on long timeouts
 * or `K_FOREVER` due to the default residency policy.
 *
 * This has to be done before anything tries to sleep, which means
 * before the threading system starts up between PRE_KERNEL_2 and
 * POST_KERNEL.  Do it at the start of PRE_KERNEL_2.
 */

static int disable_deep_sleep(const struct device *dev)
{
	ARG_UNUSED(dev);

	pm_policy_state_lock_get(PM_STATE_SOFT_OFF, PM_ALL_SUBSTATES);
	return 0;
}

SYS_INIT(disable_deep_sleep, PRE_KERNEL_2, 0);

void main(void)
{
	int ret;

	/* Wait for zephyr kernel init to complete */
	k_msleep(2000);

	const struct device *console = device_get_binding(GECKO_CONSOLE);
	printk("\nThis Sample Demo App implements a Gecko Sleep-Wake Up on Push Button Interrupt using %s\n", CONFIG_BOARD);

	gpiob_dev = device_get_binding(PUSH_BUTTON_GPIO_B_NAME);
        if (!gpiob_dev) {
                printf("GPIOB driver error\n");
        }

        ret = gpio_pin_configure(gpiob_dev,
                                 PUSH_BUTTON_INT1,
                                 GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
        if (ret) {
                printf("Error configuring GPIO_B %d!\n", PUSH_BUTTON_INT1);
        }

        gpio_init_callback(&gpio_cb, user_push_button_intr_callback, BIT(PUSH_BUTTON_INT1));

        ret = gpio_add_callback(gpiob_dev, &gpio_cb);
        if (ret) {
                printf("Cannot setup callback!\n");
        }

        ret = gpio_pin_interrupt_configure(gpiob_dev,
                                           PUSH_BUTTON_INT1,
                                           GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);

        k_msleep(1000);

	printk("Test of the User Button (sw0) interrupt - press The User Push Button (sw0) to continue \n");
	while(push_button_isr_count == 0)
	{
                k_msleep(1000);
	}
	printk("\nThe User Button (sw0) interrupt passed (%d) - continue\n", push_button_isr_count);

	k_msleep(2000);

	/* Force the current thread to busy wait. */
        /* This routine causes the current thread to execute a "do nothing" loop for usec_to_wait microseconds. */
	printk("\nTest of Busy-wait %u seconds\n", BUSY_WAIT_DURATION);
	k_busy_wait(BUSY_WAIT_DURATION * USEC_PER_SEC);

	printk("\nTest of thread Busy-wait %u seconds with UART off\n", BUSY_WAIT_DURATION);
	ret = pm_device_action_run(console, PM_DEVICE_ACTION_SUSPEND);
	k_busy_wait(BUSY_WAIT_DURATION * USEC_PER_SEC);
	ret = pm_device_action_run(console, PM_DEVICE_ACTION_RESUME);

	/* Put the current thread to sleep. */
        /* This routine puts the current thread to sleep for duration, specified as a k_timeout_t object. */
	printk("\nSleep %u seconds\n", SLEEP_DURATION);
	k_sleep(K_SECONDS(SLEEP_DURATION));

	printk("\nSleep %u seconds with UART off\n", SLEEP_DURATION);
	ret = pm_device_action_run(console, PM_DEVICE_ACTION_SUSPEND);
	k_sleep(K_SECONDS(SLEEP_DURATION));
	ret = pm_device_action_run(console, PM_DEVICE_ACTION_RESUME);

	printk("\nTesting Gecko EM mode Sleep Wakeup - press the user button to wake up \n");

	/* Try EM2 mode sleep */
	pm_state_force(0u, &(struct pm_state_info){ PM_STATE_SUSPEND_TO_IDLE, 0, 0});

	/* Try EM3 mode sleep */
        // pm_state_force(0u, &(struct pm_state_info){ PM_STATE_STANDBY, 0, 0});
	/* Try Deep sleep - shipping mode */
	// pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});

	/*
	 * This will let the idle thread run and let the pm subsystem run in forced state. 
	 */
	k_sleep(K_SECONDS(SLEEP_DURATION));

	printk("ERROR: The forced Sleep setting failed\n");
	while (true) {
		/* spin to avoid fall-off behavior */
	}
}
