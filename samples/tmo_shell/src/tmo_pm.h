/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_PM_H
#define TMO_PM_H
#include <zephyr/shell/shell.h>

int cmd_pmresume(const struct shell *shell, int argc, char** argv);
int cmd_pmsuspend(const struct shell *shell, int argc, char** argv);
int cmd_pmoff(const struct shell *shell, int argc, char** argv);
int cmd_pmon(const struct shell *shell, int argc, char** argv);
int cmd_pmget(const struct shell *shell, int argc, char** argv);
int cmd_pmlock(const struct shell *shell, int argc, char** argv);
int cmd_pmunlock(const struct shell *shell, int argc, char** argv);
void pm_device_name_get(size_t idx, struct shell_static_entry *entry);

#endif /* TMO_PM_H */
