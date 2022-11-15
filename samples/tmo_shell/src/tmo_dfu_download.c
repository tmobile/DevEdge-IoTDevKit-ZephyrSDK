/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(tmo_dfu_download, LOG_LEVEL_DBG);

#include <stdio.h>
#include <fs/fs.h>
#include <net/net_ip.h>
#include <net/socket.h>
#include <net/tls_credentials.h>
#include <net/http_client.h>
#include <net/wifi_mgmt.h>
#include <mbedtls/sha1.h>

#include "ca_certificate.h"
#include "tmo_dfu_download.h"
#include "dfu_gecko.h"
#include "dfu_murata_1sc.h"
#include "dfu_rs9116w.h"
#include "tmo_shell.h"
#include "tmo_http_request.h"

extern const struct dfu_file_t dfu_files_mcu[];
extern const struct dfu_file_t dfu_files_modem[];
extern const struct dfu_file_t dfu_files_rs9116w[];

extern uint8_t mxfer_buf[];

mbedtls_sha1_context sha1_ctx;
unsigned char sha1_output[20];

struct fs_file_t file = {0};
struct fs_dirent* my_finfo;

#define MAX_BASE_URL_LEN 256
static char base_url_s[MAX_BASE_URL_LEN] = "https://devkit.devedge.t-mobile.com/bin/latest/";

static int iface_s = WIFI_ID; // Default iface is wifi

int dfu_download(const struct dfu_file_t *dfu_file)
{
	int ret;
	unsigned char sha1_output[20];
	char url[DFU_URL_LEN] = {0};

	ret = snprintf(url, sizeof(url) - 1, "%s%s", base_url_s, dfu_file->rfile);
	if (ret < 0) {
		printf("URL was truncated\n");
	}

	printf("\nDownloading firmware for %s\n", dfu_file->desc);
	printf("from url: %s\n", url);
	printf("to file : %s\n", dfu_file->lfile);

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	tls_credential_delete(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE);
	if(strstr(base_url_s, "t-mobile.com")){
		tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				entrust_g2, sizeof(entrust_g2));
	} else {
		tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				digicert_ca, sizeof(digicert_ca));
	}
#endif

	ret = tmo_http_download(iface_s, url, dfu_file->lfile);
	if (ret < 0) {
		return ret;
	}

	// mbedtls_sha1_init(&sha1_ctx);
	memset(sha1_output, 0, sizeof(sha1_output));
	mbedtls_sha1_starts(&sha1_ctx);

	printf("\nChecking file %s\n", dfu_file->lfile);
	if (fs_open(&file, dfu_file->lfile, FS_O_READ) != 0) {
		LOG_ERR("Could not open file %s", dfu_file->lfile);
		return -1;
	}

	int readbytes = 0;
	int totalbytes = 0;
	int notdone = 1;
	int miscompareCnt = 0;
	while (notdone)
	{
		readbytes = fs_read(&file, mxfer_buf, 4096);
		if (readbytes < 0) {
			LOG_ERR("Could not read file %s", dfu_file->lfile);
			fs_close(&file);
			return 0;
		}
		//printf("read %d\n", readbytes);
		if (readbytes > 0) {
			totalbytes += readbytes;
			mbedtls_sha1_update(&sha1_ctx, (unsigned char *)mxfer_buf, readbytes);
			//printf("r .. %d %d\n", (int)readbytes, totalbytes);
			printk(".");
		}
		else {
			notdone = 0;
			//printf("done %d\n", readbytes);
		}
	}
	printf("\ntotal bytes read %d\n", totalbytes);

	mbedtls_sha1_finish(&sha1_ctx, sha1_output);

	/*
	   printf("\nInFlash  SHA1: ");
	   for (int i = 0; i < 20; i++) {
	   printf("%02x ", sha1_output[i]);
	   }
	   printf("\n");
	   */

	// printf("Expected SHA1: ");
	const char *expected_sha1 = dfu_file->sha1;
	for (int i = 0; i < 20; i++) {
		// printf("%02x ", expected_sha1[i]);
		if (sha1_output[i] != expected_sha1[i]) {
			miscompareCnt++;
		}
	}

	if (!strcmp("Gecko MCU", dfu_file->desc)) {
		if (miscompareCnt == 0) {
			printf("\nSHA1 PASSED for %s\n", dfu_file->lfile);
		}
		else {
			printf("\nSHA1 ERROR for %s\n", dfu_file->lfile);
		}
	}
	fs_close(&file);

	return totalbytes;
}

void generate_mcu_filename(struct dfu_file_t *dfu_files_mcu, char *base, char *version, int slots) {
    for (int i = 0; i < slots; i++) {
			/* BIN	*/
			sprintf(dfu_files_mcu[i].desc, "Gecko MCU %d/%d", i+1, slots*2);
			sprintf(dfu_files_mcu[i].lfile, "/tmo/zephyr.slot%d.bin", i);
			sprintf(dfu_files_mcu[i].rfile, "%s.slot%d.%s.bin",base, i, version);
			memset(dfu_files_mcu[i].sha1, 0, DFU_SHA1_LEN);

			/* SHA1 */
			sprintf(dfu_files_mcu[i+slots].desc, "Gecko MCU %d/%d", i + (1+slots), slots*2);
			sprintf(dfu_files_mcu[i+slots].lfile, "%s.sha1", dfu_files_mcu[i].lfile);
			sprintf(dfu_files_mcu[i+slots].rfile, "%s.sha1",dfu_files_mcu[i].rfile);
			memset(dfu_files_mcu[i+slots].sha1, 0, DFU_SHA1_LEN);
	}
}

int tmo_dfu_download(enum dfu_tgts dfu_tgt, char *base, char *version, int num_slots)
{
	mbedtls_sha1_init(&sha1_ctx);
	const struct dfu_file_t *dfu_files = NULL;
	struct dfu_file_t dfu_files_mcu_gen[5];

	memset(dfu_files_mcu_gen,0,sizeof(struct dfu_file_t) * 5);

	switch (dfu_tgt) {
		case DFU_GECKO:
			if (base == NULL) 
				dfu_files = dfu_files_mcu;
			else {
				if (version == NULL) {
					printf("Missing version number\n");
					return -1;
				}

				if (!num_slots) {
					printf("Slot should be greater than zero\n");
					return -1;
				}
				generate_mcu_filename(dfu_files_mcu_gen,base,version,num_slots);
				dfu_files = dfu_files_mcu_gen;
			}
			break;

		case DFU_MODEM:
			sprintf(dfu_files_modem[0].rfile, "%s.ua",base);
			sprintf(dfu_files_modem[1].rfile, "%s.sha1",dfu_files_modem[0].rfile);
			dfu_files = dfu_files_modem;
			break;

		case DFU_9116W:
			sprintf(dfu_files_rs9116w[0].rfile, "%s.rps",base);
			sprintf(dfu_files_rs9116w[1].rfile, "%s.sha1",dfu_files_rs9116w[0].rfile);
			dfu_files = dfu_files_rs9116w;
			break;

		default:
			break;
	}

	if (dfu_files == NULL) {
		printf("Unknown target\n");
		return -1;
	}

	int total = 0;
	int idx = 0;
	while (strlen(dfu_files[idx].desc)) {
		total += dfu_download(&dfu_files[idx++]);
	}
	printf("\nTotal size downloaded: %d\n", total);
	printf("Done!\n");

	return total;
}

int set_dfu_base_url(char *base_url)
{
	memset(base_url_s, 0, sizeof(base_url_s));
	strncpy(base_url_s, base_url, sizeof(base_url_s) - 1);

	return 0;
}

const char *get_dfu_base_url()
{
	return base_url_s;
}

int set_dfu_iface_type(int iface)
{
	if (iface < 1 || iface > 2) {
		printf("error: invalid iface\n");
		printf("use 1 for modem, 2 for wifi\n");
		return -1;
	}

	iface_s = iface;
	return 0;
}

int get_dfu_iface_type()
{
	return iface_s;
}
