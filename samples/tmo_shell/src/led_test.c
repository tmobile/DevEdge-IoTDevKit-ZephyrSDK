/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/led.h>

#include "tmo_shell.h"

int updown(int channel)
{
	const struct device * pwm_dev = device_get_binding("pwmleds");
	int ret = 0;

	for (int i = 0; i <= 100; i++) {
		if ((ret = led_set_brightness(pwm_dev, channel, i)) != 0) {
			printf("led_set_brightness(pwm_dev, %d, %d) - function failed %d \n", channel, i, ret);
			return (ret);
		}
		k_sleep(K_MSEC(10));
	}
	for (int i = 0; i <= 100; i++) {
		if ((ret = led_set_brightness(pwm_dev, channel, 100-i)) != 0) {
			printf("led_set_brightness(pwm_dev, %d, 100-%d) - function failed %d \n", channel, i, ret);
			return (ret);
		}
		k_sleep(K_MSEC(10));
	}
	return (ret);
}

int led_test()
{	
	const struct device * pwm_dev = device_get_binding("pwmleds");
	int ret = 0;

	if ((ret = led_off(pwm_dev, 0)) != 0) {
		printf("led_off(pwm_dev, 0) - function failed %d \n", ret);
		return (ret);
	}
	if ((ret = led_off(pwm_dev, 1)) != 0) {
		printf("led_off(pwm_dev, 1) - function failed %d \n", ret);
		return (ret);
	}
	if ((ret = led_off(pwm_dev, 2)) != 0) {
		printf("led_off(pwm_dev, 0) - function failed %d \n", ret);
		return (ret);
	}
	if ((ret = led_on(pwm_dev, 3)) != 0) {
		printf("led_on(pwm_dev, 3) - function failed %d \n", ret);
		return (ret);
	}

	printf("Blue LED should be on - function was successful\n");
	if ((ret = updown(3)) !=0) {
		return (ret);
	}

	if ((ret = led_off(pwm_dev, 3)) != 0) {
		printf("led_off(pwm_dev, 3) - function failed %d \n", ret);
		return (ret);
	}

	// This is not a great test for the white LED, but it will suffice for now
#if DT_NODE_EXISTS(DT_NODELABEL(bq24250))
	if ((ret = led_on(pwm_dev, 0)) != 0) {
		printf("led_on(pwm_dev, 0) - function failed %d \n", ret);
		return (ret);
	}

	printf("White LED should be on - function was successful\n");
	if ((ret = updown(0)) !=0) {
		return (ret);
	}

	if ((ret = led_off(pwm_dev, 0)) != 0) {
		printf("led_off(pwm_dev, 0) - function failed %d \n", ret);
		return (ret);
	}
#endif
	if ((ret = led_on(pwm_dev, 1)) != 0) {
		printf("led_on(pwm_dev, 1) - function failed %d \n", ret);
		return (ret);
	}

	printf("Red LED should be on - function was successful\n");
	if ((ret = updown(1)) !=0) {
		return (ret);
	}

	if ((ret = led_off(pwm_dev, 1)) != 0) {
		printf("led_off(pwm_dev, 1) - function failed %d \n", ret);
		return (ret);
	}
	if ((ret = led_on(pwm_dev, 2)) != 0) {
		printf("led_on(pwm_dev, 2) - function failed %d \n", ret);
		return (ret);
	}

	printf("Green LED should be on - function was successful\n");
	if ((ret = updown(2)) !=0) {
		return (ret);
	}

	if ((ret = led_off(pwm_dev, 2)) != 0) {
		printf("led_off(pwm_dev, 2) - function failed %d \n", ret);
		return (ret);
	}
	printf("led_test - function was successful\n");

	return (ret);
}
