/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_PM_SYS_H
#define TMO_PM_SYS_H
#include <zephyr/shell/shell.h>

int cmd_pmsysactive(const struct shell *shell, int argc, char** argv);
int cmd_pmsysidle(const struct shell *shell, int argc, char** argv);
int cmd_pmsyssuspend(const struct shell *shell, int argc, char** argv);
int cmd_pmsysstandby(const struct shell *shell, int argc, char** argv);
int cmd_pmsysoff(const struct shell *shell, int argc, char** argv);
int cmd_pmsysfulloff(const struct shell *shell, int argc, char** argv);

#endif /* TMO_PM_SYS_H */
