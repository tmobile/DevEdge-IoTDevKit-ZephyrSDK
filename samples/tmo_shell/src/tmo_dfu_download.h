/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_DFU_DOWNLOAD_H
#define TMO_DFU_DOWNLOAD_H

#include "tmo_shell.h"
#include <zephyr/shell/shell.h>

#ifndef __DFU_FILE__
#define __DFU_FILE__

#define DFU_DESC_LEN 64
#define DFU_FILE_LEN 64
#define DFU_SHA1_LEN 20
#define DFU_URL_LEN  256

struct dfu_file_t {
	char desc[DFU_DESC_LEN];
	char lfile[DFU_FILE_LEN];
	char rfile[DFU_FILE_LEN];
	char sha1[DFU_SHA1_LEN];
};
#endif /* __DFU_FILE__ */

#include "dfu_murata_1sc.h"
#include "dfu_rs9116w.h"
#include "dfu_gecko_lib.h"

enum dfu_tgts {
	DFU_GECKO = 0,
	DFU_MODEM,
	DFU_9116W
};

int tmo_dfu_download(const struct shell *shell, enum dfu_tgts dfu_tgt, char *filename, char *version);
int set_dfu_base_url(char *base_url);
const char *get_dfu_base_url(void);
int set_dfu_iface_type(int iface);
int get_dfu_iface_type(void);

#endif
