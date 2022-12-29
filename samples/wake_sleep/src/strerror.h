/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STRERROR_WORKAROUND_H_
#define STRERROR_WORKAROUND_H_

#ifdef __cplusplus
extern "C" {
#endif
/*
 * The following is included for Zephyr's non-standard support of strerror
 */
#ifndef sys_nerr
#include "strerror_table.h"
#define strerror strerror_extended
const char *strerror(int error_value);
#endif

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* STRERROR_WORKAROUND_H_ */
