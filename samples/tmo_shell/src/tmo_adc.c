/*
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_adc.h"
#include "tmo_battery_ctrl.h"
#include "tmo_adc.h"
#include "board.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tmo_adc, LOG_LEVEL_INF);

#define adcFreq   16000000
K_SEM_DEFINE(adc_sem, 0, 1);

static ADC_InitSingle_TypeDef initSingle_bv = ADC_INITSINGLE_DEFAULT;
static ADC_InitSingle_TypeDef initSingle_hwid = ADC_INITSINGLE_DEFAULT;

/**
 * @brief Set the VBAT_SNS_EN Pin High to enable ADC readings
 * 
 */
static void set_vbat_sens_en(bool enable)
{
	//   pin = 0
	//   mode = gpioModeEnabled;
	//   out is 1 otherwise it will be input 
	//   Set PK0/PinE2 as output so it can be
#ifdef VBAT_EN_PORT
	GPIO_PinModeSet(VBAT_EN_PORT, VBAT_EN_PIN, gpioModePushPull, enable);
#endif /* VBAT_EN_PORT */
}

/**
 * @brief Initialize the Gecko ADC
 * 
 */
static void initADC(void)
{
	static bool lazy_init = false;	/* perform the init only once */
	/* use lazy init, to remove the call from tmo_shell.c */
	if (lazy_init) {
		return;
	}
	lazy_init = true;

	// Enable ADC0 clock
	CMU_ClockEnable(cmuClock_ADC0, true);

	// Declare init structs
	ADC_Init_TypeDef init = ADC_INIT_DEFAULT;

	// Modify init structs and initialize
	init.prescale = ADC_PrescaleCalc(adcFreq, 0); // Init to max ADC clock for Series 1

	initSingle_bv.diff       = false;        // single ended
	initSingle_bv.reference  = adcRef2V5;    // internal 2.5V reference
	initSingle_bv.resolution = adcRes12Bit;  // 12-bit resolution
	initSingle_bv.acqTime    = adcAcqTime32;  // set acquisition time to meet minimum requirement

	memcpy(&initSingle_hwid, &initSingle_bv, sizeof(initSingle_hwid));

	// Select ADC input. See README for corresponding EXP header pin.
	//  initSingle.posSel = adcPosSelAPORT4XCH10;
#ifdef HWID_APORT
	initSingle_hwid.posSel = HWID_APORT;
#endif /* HWID_APORT */
#ifdef VBAT_APORT
	initSingle_bv.posSel = VBAT_APORT;
#endif /* VBAT_APORT */

	init.timebase = ADC_TimebaseCalc(0);

	ADC_Init(ADC0, &init);
}

/*
 * @brief  This function writes the amount of battery charge remaining
 *         (to the nearest 1%) in bv.
 *         It returns true if successful, or false if there is an issue
 */
__weak bool millivolts_to_percent(uint32_t millivolts, uint8_t *percent) {
	float curBv = get_remaining_capacity((float) millivolts / 1000);
	*percent = (uint8_t) (curBv + 0.5);
	return true;
}

/**
 * @brief  Main function
 */
__weak int read_battery_voltage(void)
{
#ifdef VBAT_APORT
	initADC();

	uint32_t sample;
	uint32_t millivolts;
	float millivolts_f;
	// Start ADC conversion
	k_sem_take(&adc_sem, K_MSEC(500));
	
	set_vbat_sens_en(true);
	k_msleep(100);

	ADC_InitSingle(ADC0, &initSingle_bv);
	ADC_Start(ADC0, adcStartSingle);

	//  Wait for conversion to be complete
	while(!(ADC0->STATUS & _ADC_STATUS_SINGLEDV_MASK));

	// Get ADC result
	sample = ADC_DataSingleGet(ADC0);

	set_vbat_sens_en(false);

	k_sem_give(&adc_sem);

	// Calculate input voltage in mV
	millivolts_f = (sample * 2500.0) / 4096.0;

	// On the 2nd generation dev edge, voltage on PA2 is
	// one third the actual battery voltage
	millivolts = (uint32_t) (3.0 * millivolts_f + 0.5);

	return (millivolts);
#else 
	return 0;
#endif /* VBAT_APORT */
}

/**
 * @brief Read HWID divider voltage
 * 
 * @return int Millivolts
 */
__weak int read_hwid(void)
{
#ifdef HWID_APORT
	initADC();

	uint32_t sample;
	uint32_t millivolts;
	float millivolts_f;
	// Start ADC conversion
	k_sem_take(&adc_sem, K_MSEC(500));

	set_vbat_sens_en(true);
	k_msleep(100);

	ADC_InitSingle(ADC0, &initSingle_hwid);
	ADC_Start(ADC0, adcStartSingle);

	//  Wait for conversion to be complete
	while(!(ADC0->STATUS & _ADC_STATUS_SINGLEDV_MASK));

	// Get ADC result
	sample = ADC_DataSingleGet(ADC0);

	set_vbat_sens_en(false);

	k_sem_give(&adc_sem);

	// Calculate input voltage in mV
	millivolts_f = (sample * 2500.0) / 4096.0;

	millivolts = (uint32_t) millivolts_f;

	return (millivolts);
#else
	return 0;
#endif /* HWID_APORT */
}
