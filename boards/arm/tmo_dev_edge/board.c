/*
 * Copyright (c) 2018 Christian Taedcke
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include "board.h"
#include <drivers/gpio.h>
#include <sys/printk.h>

static void powerup_led_on(struct k_timer *timer_id);

K_TIMER_DEFINE(powerup_led_timer, powerup_led_on, NULL);

static int tmo_dev_edge(const struct device *dev)
{
	const struct device *rs_dev; /* RS9116 Gpio Device */
	const struct device *bz_dev; /* PAM8904E Gpio Device */
	const struct device *gpiof;

	ARG_UNUSED(dev);

	rs_dev = device_get_binding(RS9116_GPIO_NAME);

	if (!rs_dev) {
		printk("RS9116 gpio port was not found!\n");
		return -ENODEV;
	}

	gpio_pin_configure(rs_dev, RS9116_RST_GPIO_PIN, GPIO_OUTPUT_LOW);
	gpio_pin_configure(rs_dev, RS9116_POC_GPIO_PIN, GPIO_OUTPUT_LOW);
	k_msleep(10);
	
	gpio_pin_configure(rs_dev, RS9116_POC_GPIO_PIN, GPIO_OUTPUT_HIGH);

	bz_dev = device_get_binding(BUZZER_ENx_GPIO_NAME);

	if (!bz_dev) {
		printk("PAM8904E gpio port was not found!\n");
		return -ENODEV;
	}

	gpio_pin_configure(bz_dev, BUZZER_EN1_GPIO_PIN, GPIO_OUTPUT_HIGH);
	gpio_pin_configure(bz_dev, BUZZER_EN2_GPIO_PIN, GPIO_OUTPUT_HIGH);

	gpio_pin_configure(bz_dev, WHITE_LED_GPIO_PIN, GPIO_OUTPUT_LOW);
	gpio_pin_configure(bz_dev, RED_LED_GPIO_PIN, GPIO_OUTPUT_LOW);
	gpio_pin_configure(bz_dev, GREEN_LED_GPIO_PIN, GPIO_OUTPUT_LOW);
	gpio_pin_configure(bz_dev, BLUE_LED_GPIO_PIN, GPIO_OUTPUT_LOW);

	gpiof=device_get_binding(GNSS_GPIO_NAME);
	if (gpiof == NULL)
	{
		printk("GNSS gpio port was not found!\n");
		return -ENODEV;
	}
	gpio_pin_configure(gpiof,GNSS_PWR_ON,GPIO_OUTPUT_HIGH);
	gpio_pin_configure(gpiof,GNSS_BOOT_REQ,GPIO_OUTPUT_LOW);
	gpio_pin_configure(gpiof,GNSS_RESET,GPIO_OUTPUT_LOW);

	k_sleep(K_MSEC(100));

	gpio_pin_configure(gpiof,GNSS_RESET,GPIO_OUTPUT_HIGH);

	gpio_pin_configure(gpiof,BQ_CHIP_ENABLE,GPIO_OUTPUT_LOW);

	k_timer_start(&powerup_led_timer, K_SECONDS(1), K_FOREVER);
	return 0;
}

#include <drivers/pwm.h>

static void powerup_led_on(struct k_timer *timer_id)
{
	const struct device *led_pwm;
	led_pwm = device_get_binding(LED_PWM_NAME);
	pwm_set(led_pwm, LED_PWM_RED, 100000, 10000, 0);
	pwm_set(led_pwm, LED_PWM_BLUE, 100000, 5000, 0);
}


/* needs to be done after GPIO driver init */
SYS_INIT(tmo_dev_edge, POST_KERNEL,
	 CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
