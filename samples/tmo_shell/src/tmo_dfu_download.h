/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_DFU_DOWNLOAD_H
#define TMO_DFU_DOWNLOAD_H

#include <zephyr/shell/shell.h>

#include "tmo_shell.h"
#include "dfu_common.h"

#include "dfu_murata_1sc.h"
#include "dfu_rs9116w.h"
#include "dfu_gecko_lib.h"

enum dfu_tgts {
	DFU_GECKO = 0,
	DFU_MODEM,
	DFU_9116W
};

int tmo_dfu_download(const struct shell *shell, enum dfu_tgts dfu_tgt, char *filename,
		     char *version);
int set_dfu_base_url(char *base_url);
int set_dfu_auth_key(char *auth_key);
const char *get_dfu_base_url(void);
int set_dfu_iface_type(int iface);
int get_dfu_iface_type(void);

#endif
