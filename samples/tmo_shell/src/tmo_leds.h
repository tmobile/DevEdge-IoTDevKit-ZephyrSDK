/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_LEDS_H
#define TMO_LEDS_H

#include <drivers/gpio.h>
#include <zephyr/drivers/led.h>

#define PWMLEDS		device_get_binding("pwmleds")

#endif
