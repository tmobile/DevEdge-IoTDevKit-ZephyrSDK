/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * TODO: Replace this interim implementation with the system default, assuming it exists.
 */

#include "strerror.h"
#include <stdlib.h>
/*
 * Interim support for strerror
 */
#ifdef sys_nerr
const char *strerror(int error_value)
{
	error_value = abs(error_value);
	if (sys_nerr < error_value) {
		return "";
	} else {
		return sys_errlist[error_value];
	}
}
#endif
