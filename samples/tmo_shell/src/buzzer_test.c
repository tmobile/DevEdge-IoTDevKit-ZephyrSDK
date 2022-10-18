/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr.h>
#include <kernel.h>
#include <drivers/pwm.h>

#include "tmo_shell.h"
#include "tmo_buzzer.h"

int buzzer_test()
{
	const struct device *buzzer2 = DEVICE_DT_GET(DT_ALIAS(pwm_buzzer));
	int ret = 0;
	ret = pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(130));
	ret = pwm_set_frequency(buzzer2, 0, 0);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, 0) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(20));
	ret = pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(130));
	ret = pwm_set_frequency(buzzer2, 0, 0);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, 0) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(20));
	ret = pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(130));
	ret = pwm_set_frequency(buzzer2, 0, 0);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, 0) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(20));
	ret = pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_HIGH);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_HIGH) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(130));
	ret = pwm_set_frequency(buzzer2, 0, 0);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, 0) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(20));
	ret = pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW) - function failed %d \n", ret);
		return (ret);
	}
	k_sleep(K_MSEC(521));
	ret = pwm_set_frequency(buzzer2, 0, 0);
	if (ret) {
		printf("pwm_set_frequency(buzzer2, 0, TMO_TUNE_PITCH_LOW) - function failed %d \n", ret);
		return (ret);
	}
	printf("buzzer_test - function was successful\n");
	return (ret);
}
