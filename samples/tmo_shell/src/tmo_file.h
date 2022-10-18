/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_FILE_H
#define TMO_FILE_H

int tmo_cp(const struct shell *shell, size_t argc, char **argv);
int tmo_ll(const struct shell *shell, size_t argc, char **argv);
int tmo_mv(const struct shell *shell, size_t argc, char **argv);
int cmd_sha1(const struct shell *shell, size_t argc, char **argv);

#endif
