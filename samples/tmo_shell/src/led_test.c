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
#include "tmo_leds.h"
#include "board.h"

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
#ifdef LED_PWM_WHITE
		if ((ret = led_off(pwm_dev, LED_PWM_WHITE)) != 0) {
			printf("led_off(pwm_dev, LED_PWM_WHITE) - function failed %d \n", ret);
			return (ret);
		}
#endif /* LED_PWM_WHITE */
	if ((ret = led_off(pwm_dev, LED_PWM_RED)) != 0) {
		printf("led_off(pwm_dev, LED_PWM_RED) - function failed %d \n", ret);
		return (ret);
	}
	if ((ret = led_off(pwm_dev, LED_PWM_GREEN)) != 0) {
		printf("led_off(pwm_dev, LED_PWM_GREEN) - function failed %d \n", ret);
		return (ret);
	}
	if ((ret = led_on(pwm_dev, LED_PWM_BLUE)) != 0) {
		printf("led_on(pwm_dev, LED_PWM_BLUE) - function failed %d \n", ret);
		return (ret);
	}

	printf("Blue LED should be on - function was successful\n");
	if ((ret = updown(LED_PWM_BLUE)) !=0) {
		return (ret);
	}

	if ((ret = led_off(pwm_dev, LED_PWM_BLUE)) != 0) {
		printf("led_off(pwm_dev, LED_PWM_BLUE) - function failed %d \n", ret);
		return (ret);
	}

	// This is not a great test for the white LED, but it will suffice for now
#ifdef LED_PWM_WHITE
		if ((ret = led_on(pwm_dev, LED_PWM_WHITE)) != 0) {
			printf("led_on(pwm_dev, LED_PWM_WHITE) - function failed %d \n", ret);
			return (ret);
		}

		printf("White LED should be on - function was successful\n");
		if ((ret = updown(LED_PWM_WHITE)) !=0) {
			return (ret);
		}

		if ((ret = led_off(pwm_dev, LED_PWM_WHITE)) != 0) {
			printf("led_off(pwm_dev, LED_PWM_WHITE) - function failed %d \n", ret);
			return (ret);
		}
#endif /* LED_PWM_WHITE */

	if ((ret = led_on(pwm_dev, LED_PWM_RED)) != 0) {
		printf("led_on(pwm_dev, LED_PWM_RED) - function failed %d \n", ret);
		return (ret);
	}

	printf("Red LED should be on - function was successful\n");
	if ((ret = updown(LED_PWM_RED)) !=0) {
		return (ret);
	}

	if ((ret = led_off(pwm_dev, LED_PWM_RED)) != 0) {
		printf("led_off(pwm_dev, LED_PWM_RED) - function failed %d \n", ret);
		return (ret);
	}
	if ((ret = led_on(pwm_dev, LED_PWM_GREEN)) != 0) {
		printf("led_on(pwm_dev, LED_PWM_GREEN) - function failed %d \n", ret);
		return (ret);
	}

	printf("Green LED should be on - function was successful\n");
	if ((ret = updown(LED_PWM_GREEN)) !=0) {
		return (ret);
	}

	if ((ret = led_off(pwm_dev, LED_PWM_GREEN)) != 0) {
		printf("led_off(pwm_dev, LED_PWM_GREEN) - function failed %d \n", ret);
		return (ret);
	}
	printf("led_test - function was successful\n");

	return (ret);
}
