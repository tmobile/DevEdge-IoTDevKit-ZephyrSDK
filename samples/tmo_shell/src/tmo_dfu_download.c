/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tmo_dfu_download, LOG_LEVEL_DBG);

#include <stdio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/wifi_mgmt.h>
#include <mbedtls/sha1.h>

#include "ca_certificate.h"
#include "tmo_dfu_download.h"
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
static char base_url_s[MAX_BASE_URL_LEN];
static char user_base_url_s[MAX_BASE_URL_LEN] = "https://devkit.devedge.t-mobile.com/bin/";

static int iface_s = WIFI_ID; // Default iface is wifi

char *dfu_target_str(enum dfu_tgts dfu_tgt) {
	switch(dfu_tgt) {
		case DFU_GECKO: return "MCU";
		case DFU_MODEM: return "Modem";
		default:
			return "WiFi";
	}
}

int dfu_download(const struct dfu_file_t *dfu_file, enum dfu_tgts dfu_tgt)
{
	int ret;
	unsigned char sha1_output[20];
	char url[DFU_URL_LEN] = {0};

	ret = snprintf(url, sizeof(url) - 1, "%s%s", base_url_s, dfu_file->rfile);
	if (ret < 0) {
		printf("URL was truncated\n");
	}

	printf("\nDownloading %s firmware %s\n", dfu_target_str(dfu_tgt), dfu_file->desc);
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

void generate_mcu_filename(struct dfu_file_t *dfu_files_mcu, char *base, int slots, char *version)
{
    int total_files = (slots*2) + 1;

    sprintf(dfu_files_mcu[0].desc, "%s 1/%d", base, total_files);
	sprintf(dfu_files_mcu[0].lfile, "/tmo/zephyr.bin");
	sprintf(dfu_files_mcu[0].rfile, "%s.%s.bin",base,version);
	memset(dfu_files_mcu[0].sha1, 0, DFU_SHA1_LEN);

    for (int i = 1; i <= slots; i++) {

		/* BIN	*/
		sprintf(dfu_files_mcu[i].desc, "%s %d/%d", base ,i+1, total_files);
		sprintf(dfu_files_mcu[i].lfile, "/tmo/zephyr.slot%d.bin", i-1);
		sprintf(dfu_files_mcu[i].rfile, "%s.%s.slot%d.bin",base,version, i-1);
		memset(dfu_files_mcu[i].sha1, 0, DFU_SHA1_LEN);

		/* SHA1 */
		sprintf(dfu_files_mcu[i+slots].desc, "%s %d/%d", base,(i+slots)+1, total_files);
		sprintf(dfu_files_mcu[i+slots].lfile, "%s.sha1", dfu_files_mcu[i].lfile);
		sprintf(dfu_files_mcu[i+slots].rfile, "%s.sha1",dfu_files_mcu[i].rfile);
		memset(dfu_files_mcu[i+slots].sha1, 0, DFU_SHA1_LEN);
	}
}

int tmo_dfu_download(const struct shell *shell, enum dfu_tgts dfu_tgt, char *base, char *version)
{
	mbedtls_sha1_init(&sha1_ctx);
	const struct dfu_file_t *dfu_files = NULL;
	struct dfu_file_t dfu_files_mcu_gen[6];

	memset(dfu_files_mcu_gen,0,sizeof(struct dfu_file_t) * 6);

	switch (dfu_tgt) {
		case DFU_GECKO:
			if (base == NULL) {
				dfu_files = dfu_files_mcu;

				sprintf(base_url_s,"%slatest/",user_base_url_s);
			} else {
				generate_mcu_filename(dfu_files_mcu_gen,base,2, version);
				dfu_files = dfu_files_mcu_gen;
				
				sprintf(base_url_s,"%s%s/",user_base_url_s, version);
			}
			break;

		case DFU_MODEM:
			sprintf((char *)dfu_files_modem[0].desc, "%s",base);
			sprintf((char *)dfu_files_modem[0].rfile, "%s.ua",base);
			sprintf((char *)dfu_files_modem[0].lfile, "/tmo/%s.ua",base);
			dfu_files = dfu_files_modem;

			sprintf(base_url_s,"%smurata_1sc/",user_base_url_s);
			break;

		case DFU_9116W:
			sprintf((char *)dfu_files_rs9116w[0].desc, "%s",base);
			sprintf((char *)dfu_files_rs9116w[0].rfile, "%s.rps",base);
			dfu_files = dfu_files_rs9116w;

			sprintf(base_url_s,"%srs9116w/",user_base_url_s);

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
		total += dfu_download(&dfu_files[idx++], dfu_tgt);
	}
	printf("\nTotal size downloaded: %d\n", total);
	printf("Done!\n");

	return total;
}

int set_dfu_base_url(char *base_url)
{
	memset(user_base_url_s, 0, sizeof(user_base_url_s));
	strncpy(user_base_url_s, base_url, sizeof(user_base_url_s) - 1);

	return 0;
}

const char *get_dfu_base_url()
{
	return user_base_url_s;
}

int set_dfu_iface_type(int iface)
{
	if (iface < 1 || iface > 2) {
		printf("error: invalid iface\n");
		printf("use 1 for modem, 2 for wifi");
		return -1;
	}

	iface_s = iface;
	return 0;
}

int get_dfu_iface_type()
{
	return iface_s;
}
