/*
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <stdio.h>
#include <kernel.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_adc.h"
#include "em_gpio.h"
#include "tmo_battery_ctrl.h"

#define adcFreq   16000000
K_SEM_DEFINE(adc_sem, 0, 1);

/**************************************************************************
 * On the DevEdge board, there is a discrete circuit (on page 2) that can
 * be used to measure the battery voltage.
 * It is enabled by asserting (driving high) Pearl Gecko port PK0.
 * The analog battery voltage sense is connected to Pearl Gecko port PA2.
 * In the Pearl Gecko, the register field enumeration for connecting PA2 to
 * the ADC block is APORT3XCH10 or APORT4YCH10
 */

/***************************************************************************
  Drive High PK0, or PinE2 to enable VBATT_SNS_EN line
 ****************************************************************************/
void initPK0()
{
	//   pin = 0
	//   mode = gpioModeEnabled;
	//   out is 1 otherwise it will be input 
	//   Set PK0/PinE2 as output so it can be
	GPIO_PinModeSet(gpioPortK, 0, gpioModePushPull, 1);
}

/***************************************************************************
 * @brief  Initialize ADC function
 ***************************************************************************/
void initADC (void)
{
	// Enable ADC0 clock
	CMU_ClockEnable(cmuClock_ADC0, true);

	// Declare init structs
	ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
	ADC_InitSingle_TypeDef initSingle = ADC_INITSINGLE_DEFAULT;

	// Modify init structs and initialize
	init.prescale = ADC_PrescaleCalc(adcFreq, 0); // Init to max ADC clock for Series 1

	initSingle.diff       = false;        // single ended
	initSingle.reference  = adcRef2V5;    // internal 2.5V reference
	initSingle.resolution = adcRes12Bit;  // 12-bit resolution
	initSingle.acqTime    = adcAcqTime4;  // set acquisition time to meet minimum requirement

	// Select ADC input. See README for corresponding EXP header pin.
	//  initSingle.posSel = adcPosSelAPORT4XCH10;
	initSingle.posSel = adcPosSelAPORT3XCH10;
	init.timebase = ADC_TimebaseCalc(0);

	ADC_Init(ADC0, &init);
	ADC_InitSingle(ADC0, &initSingle);
}

/***************************************************************************
 * @brief  Exponential filter for battery level
 ***************************************************************************/
static void apply_filter(float *bv)
{
	static float s_filtered_capacity = -1;
	static bool s_battery_is_charging = false;
	bool battery_is_charging;

	// If there has been a switch between charger and battery, reset the filter
	battery_is_charging = is_battery_charging();
	if (s_battery_is_charging != battery_is_charging) {
		s_battery_is_charging = battery_is_charging;
		s_filtered_capacity = -1;
	}

	if (s_filtered_capacity < 0) {
		s_filtered_capacity = *bv;
	}
	*bv = s_filtered_capacity = s_filtered_capacity * 0.95 + (*bv) * 0.05;
}

/***************************************************************************
 * @brief  This function writes the amount of battery charge remaining
 *         (to the nearest 1%) in bv.
 *         It returns true if successful, or false if there is an issue
 ***************************************************************************/
bool millivolts_to_percent(uint32_t millivolts, uint8_t *percent) {
	float curBv = get_remaining_capacity((float) millivolts / 1000);
	apply_filter(&curBv);
	*percent = (uint8_t) (curBv + 0.5);
	return true;
}

/***************************************************************************
 * @brief  Main function
 ***************************************************************************/
int read_battery_voltage(void)
{
	uint32_t sample;
	uint32_t millivolts;
	float millivolts_f;
	// Start ADC conversion
	k_sem_take(&adc_sem, K_MSEC(500));
	ADC_Start(ADC0, adcStartSingle);

	//  Wait for conversion to be complete
	while(!(ADC0->STATUS & _ADC_STATUS_SINGLEDV_MASK));

	// Get ADC result
	sample = ADC_DataSingleGet(ADC0);

	k_sem_give(&adc_sem);

	// Calculate input voltage in mV
	millivolts_f = (sample * 2500.0) / 4096.0;

	// On the 2nd generation dev edge, voltage on PA2 is
	// one third the actual battery voltage
	millivolts = (uint32_t) (3.0 * millivolts_f + 0.5);

	return (millivolts);
}
