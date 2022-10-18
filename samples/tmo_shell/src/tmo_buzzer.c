/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>

#include "tmo_buzzer.h"
#include "tmo_leds.h"

const struct device *buzzer = DEVICE_DT_GET(DT_ALIAS(pwm_buzzer));

static float buzzer_duty_cycle = 50.0f;

int pwm_set_frequency(const struct device *dev, uint32_t pwm, uint16_t hz)
{
	int ret = 0;
	if (hz == 0) {
		ret = pwm_set(dev, pwm, 0, 0, 0);
		return(ret);
	}
	uint32_t nsec = 1000000000 / ((int32_t)hz);
	uint32_t width = (uint32_t)(((float)nsec * buzzer_duty_cycle) / 100.0f);
	ret = pwm_set(dev, pwm, nsec, width, 0);
	return(ret);
}

void tmo_play_jingle()
{
	led_set_brightness(PWMLEDS, 1, 100);
	led_set_brightness(PWMLEDS, 3, 50);
	pwm_set_frequency(buzzer, 0, TMO_TUNE_PITCH_LOW);
	k_msleep(130);
	pwm_set_frequency(buzzer, 0, 0);
	k_msleep(20);
	pwm_set_frequency(buzzer, 0, TMO_TUNE_PITCH_LOW);
	k_msleep(130);
	pwm_set_frequency(buzzer, 0, 0);
	k_msleep(20);
	pwm_set_frequency(buzzer, 0, TMO_TUNE_PITCH_LOW);
	k_msleep(130);
	pwm_set_frequency(buzzer, 0, 0);
	k_msleep(20);
	pwm_set_frequency(buzzer, 0, TMO_TUNE_PITCH_HIGH);
	k_msleep(130);
	pwm_set_frequency(buzzer, 0, 0);
	k_msleep(20);
	pwm_set_frequency(buzzer, 0, TMO_TUNE_PITCH_LOW);
	k_msleep(521);
	pwm_set_frequency(buzzer, 0, 0);
	led_set_brightness(PWMLEDS, 1, 10);
	led_set_brightness(PWMLEDS, 3, 5);
}

#define RAMP_DWELL 100
void tmo_play_ramp()
{
	pwm_set_frequency(buzzer, 0, TMO_PITCH_D3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_D3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_E3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_F3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Fs3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_G3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Gs3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_A3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_As3);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_B3);

	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_C4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Cs4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_D4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_E4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_F4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Fs4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_G4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Gs4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_A4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_As4);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_B4);
	

	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_C5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Cs5);

	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_E5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_F5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Fs5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_G5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Gs5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_A5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_As5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_B5);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_C6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Cs6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_D6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_E6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_F6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Fs6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_G6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Gs6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_A6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_As6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_B6);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_C7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Cs7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_D7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_E7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_F7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Fs7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_G7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Gs7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_A7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_As7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_B7);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_C8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Cs8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_D8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_E8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_F8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Fs8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_G8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Gs8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_A8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_As8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_B8);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_C9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Cs9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_D9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_E9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_F9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Fs9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_G9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Gs9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_A9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_As9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_B9);
	k_msleep(RAMP_DWELL); pwm_set_frequency(buzzer, 0, TMO_PITCH_Ds5);
	k_msleep(500); pwm_set_frequency(buzzer, 0, 0);
}

int tmo_play_tone(int frequency_hz, int duration_msecs)
{
	pwm_set_frequency(buzzer, 0, frequency_hz);
	k_msleep(duration_msecs);
	pwm_set_frequency(buzzer, 0, 0);
	return 0;
}

static float dc_from_volume(uint8_t volume_percent)
{
	// const float volume_table[] = {.023f, .053f, .12f, .28f, .65f, 1.5f, 3.5f, 8.2f, 19.0f, 45.0f};
	const float volume_table[] = {0.023f, 0.049f, 0.1f, 0.22f, 0.48f, 1.0f, 2.2f, 4.6f, 9.9f , 21.0f, 45.0f};
	if (volume_percent > 100) {
		volume_percent = 100;
	} else if (!volume_percent) {
		return 0;
	}
	float volume_dc = volume_table[volume_percent / 10];
	if (volume_percent % 10) {
		float delta = volume_table[(volume_percent / 10) + 1] - volume_table[volume_percent / 10];
		delta *= (volume_percent % 10) / 10.0f;
		volume_dc += delta;
	}
	return volume_dc;
}

void tmo_buzzer_set_volume(int percent)
{
	buzzer_duty_cycle = dc_from_volume(percent);
}
