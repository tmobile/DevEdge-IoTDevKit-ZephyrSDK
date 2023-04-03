/*
 * Copyright (c) 2023 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/kernel.h>

typedef enum {
	BUTTON_PRESS_NORMAL = 0,
	BUTTON_PRESS_TIMEOUT = 1
} button_press_type;

typedef enum {
	BUTTON_STATE_ERROR = -1,
	BUTTON_STATE_DEINIT = 0,
	BUTTON_STATE_INIT = 1,
	BUTTON_STATE_DEBOUNCING = 2,
	BUTTON_STATE_DEBOUNCED = 3,
	BUTTON_STATE_TIMEOUT = 4
} button_press_state;

/**
 * @brief Button callback definition, the user will implement the callback in the application code.
 * It gives the timestamp of the button pressed (from system uptime in millisecs), the duration of
 * how long the button was pressed, and the type of button press, whether a normal or a timeout.
 *
 * The duration parameter will be the max timeout value when a timeout occurs, this won't represent
 * the real physical button press duration though. The callback will be invoked as soon as the
 * timeout reaches even if the user has not release the button.
 */
typedef void (*button_press_callback_t)(uint32_t timestamp, uint32_t duration,
					button_press_type type);

/**
 * @brief Button press initialization. Called by user application code to configure and initialize
 * user button to generate callback on button presses.
 *
 * @param max_timeout   [in]    Max timeout value for which the user button can be pressed, after
 * which it is considered to be not normal operation.
 * @param callback      [in]    Function pointer to the callback that will be called when the button
 * has been pressed for a certain duration
 * @return int  status code indicating success/failure.
 */
int button_press_module_init(uint32_t max_timeout, button_press_callback_t callback);

/**
 * @brief De-initialize the button interrupt and callbacks if it was previously done so. Usually
 * performed if we want to set a new configuration on the button.
 *
 * @return int status code indicating success/failure
 */
int button_press_module_deinit(void);

/**
 * @brief Get the button configured state. Handy getter function that user application code can use
 * to determine if existing callback is set before configuring it with new parameters.
 *
 * @return button_press_state
 */
button_press_state button_press_get_state(void);

#endif