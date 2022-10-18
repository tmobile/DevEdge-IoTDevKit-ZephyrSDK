/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CA_CERTIFICATE_H__
#define __CA_CERTIFICATE_H__

enum tls_tag {
	CA_CERTIFICATE_TAG = 1,
	CLIENT_CERTIFICATE_TAG,		//in tls_credential.h defined as TLS_CREDENTIAL_SERVER_CERTIFICATE
	CLIENT_KEY_TAG,
	PSK_TAG
};

#define TLS_PEER_HOSTNAME ""

/* This is the same cert as what is found in net-tools/echo-apps-cert.pem file
*/
static const unsigned char dev_certificate[] = {
#include "devcert.der.inc"
};

static const unsigned char dtls_server_cert[] = {
#include "servercert.der.inc"
};

static const unsigned char entrust_g2[] = {
#include "entrust_g2_ca.der.inc"
};

static const unsigned char dev_private_key[] = {
#include "devkey.der.inc"
};

static const unsigned char digicert_ca[] = {
#include "digicert_ca.der.inc"
};

#if defined(CONFIG_MBEDTLS_KEY_EXCHANGE_PSK_ENABLED)
#include CONFIG_NET_SAMPLE_PSK_HEADER_FILE
#endif

#endif /* __CA_CERTIFICATE_H__ */
