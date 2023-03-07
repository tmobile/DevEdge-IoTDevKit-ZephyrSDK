/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_HTTP_REQUEST_H
#define TMO_HTTP_REQUEST_H

void tmo_http_json();
int tmo_http_download(int devid, char url[], const char filename[], char *auth_key);

#endif
