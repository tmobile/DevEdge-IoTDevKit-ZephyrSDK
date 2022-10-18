/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_CERTS_H
#define TMO_CERTS_H

extern unsigned char ca_cert[];
extern int ca_cert_sz;
extern int ca_cert_idx;
int cmd_tmo_cert_load(const struct shell *shell, size_t argc, char **argv);
int cmd_tmo_cert_list(const struct shell *shell, size_t argc, char **argv);
int cmd_tmo_cert_info(const struct shell *shell, size_t argc, char **argv);
int cmd_tmo_cert_dld(const struct shell *shell, size_t argc, char **argv);

#endif
