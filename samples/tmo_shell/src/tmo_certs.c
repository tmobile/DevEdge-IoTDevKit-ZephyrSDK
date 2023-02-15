/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/sys/base64.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <zephyr/shell/shell.h>

#include "tmo_shell.h"
#include "ca_certificate.h"
#include "tmo_http_request.h"

#define CERT_BIN_LOCATION "/tmo/certs/cert.bin"
#define CERT_BIN_FOLDER "/tmo/certs/"
#define HTTP_PREFIX  "http://"
#define HTTPS_PREFIX  "https://"

unsigned char ca_cert[2048] = {0};
int ca_cert_sz = 0;
int ca_cert_idx = 0;
extern uint8_t mxfer_buf[];
static char *dec_buf = &mxfer_buf[2000];
static int cert_cnt, success_cnt;
mbedtls_x509_crt ca_x509;

struct cert_record {
	char cert_cn[64];
	uint16_t cert_sz;
};


int cmd_tmo_cert_load(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2){
		shell_error(shell, "Missing required arguments");
		return -EINVAL;
	}
	int target = strtol(argv[1], NULL, 10);
	char cn_buf[65] = {0};
	struct cert_record rec = {0};
	int idx = 0, stat;
	char *filename = CERT_BIN_LOCATION;
	struct fs_file_t file = {0};
	stat = fs_open(&file, filename, FS_O_READ);
	if (stat) {
		shell_error(shell, "Failed to open file %s (%d)", filename, stat);
		return -EIO;
	}
	while (1){
		memset(&rec, 0, sizeof(rec));
		stat = fs_read(&file, &rec, sizeof(rec));
		if (stat == 0) {
			shell_error(shell, "Cert not found");
			fs_close(&file);
			return -EINVAL;
		} else if (stat != sizeof(rec)) {
			shell_error(shell, "Bad entry in %s", filename);
			fs_close(&file);
			return -EIO;
		}
		memcpy(cn_buf, rec.cert_cn, 64);
		if (idx != target) {
			fs_seek(&file, sys_be16_to_cpu(rec.cert_sz), FS_SEEK_CUR);
		} else {
			memcpy(cn_buf, rec.cert_cn, 64);
			fs_read(&file, ca_cert, sys_be16_to_cpu(rec.cert_sz));
			ca_cert_sz = sys_be16_to_cpu(rec.cert_sz);
			tls_credential_delete(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE);
			tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
					ca_cert, ca_cert_sz);
			if (strlen(cn_buf)){
				shell_print(shell, "Cert \"%s\" loaded (%d bytes)", cn_buf, ca_cert_sz);
			} else {
				shell_print(shell, "Cert loaded (%d bytes)", ca_cert_sz);   
			}
			fs_close(&file);
			ca_cert_idx = target;
			return 0;
		}
		idx++;
	}
}

int cmd_tmo_cert_list(const struct shell *shell, size_t argc, char **argv)
{
	char *search_string = "";
	if (argc >= 2) {
		search_string = argv[1];
	}
	char cn_buf[65] = {0};
	struct cert_record rec = {0};
	int idx = 0, stat;
	char *filename = CERT_BIN_LOCATION;
	struct fs_file_t file = {0};
	stat = fs_open(&file, filename, FS_O_READ);
	if (stat) {
		shell_error(shell, "Failed to open file %s (%d)", filename, stat);
		return -EIO;
	}
	while (1){
		memset(cn_buf, 0, sizeof(cn_buf));
		memset(&rec, 0, sizeof(rec));
		stat = fs_read(&file, &rec, sizeof(rec));
		if (stat == 0) {
			fs_close(&file);
			return 0;
		} else if (stat != sizeof(rec)) {
			shell_error(shell, "Bad entry in %s", filename);
			fs_close(&file);
			return -EIO;
		}
		memcpy(cn_buf, rec.cert_cn, 64);
		if (strlen(cn_buf)){
			if (strstr(cn_buf, search_string))
				shell_print(shell, "%03d: %s", idx, cn_buf);
			fs_seek(&file, sys_be16_to_cpu(rec.cert_sz), FS_SEEK_CUR);
		} else {
			char *subject_dn, *subject_eol;
			fs_read(&file, dec_buf, sys_be16_to_cpu(rec.cert_sz));
			mbedtls_x509_crt_init(&ca_x509);
			mbedtls_x509_crt_parse_der_nocopy(&ca_x509, dec_buf, sys_be16_to_cpu(rec.cert_sz));
			mbedtls_x509_crt_info(mxfer_buf, 2000, "", &ca_x509);
			mbedtls_x509_crt_free(&ca_x509);
			memset(dec_buf, 0, 3000);
			subject_dn = strstr(mxfer_buf, "subject name");
			subject_dn = strstr(subject_dn, ":") + 2;
			subject_eol = strstr(subject_dn, "\n");
			memcpy(dec_buf, subject_dn, subject_eol - subject_dn);
			if (strstr(dec_buf, search_string))
				shell_print(shell, "%03d: Subject name:%s", idx, dec_buf);
		}
		idx++;
	}
}

int cmd_tmo_cert_info(const struct shell *shell, size_t argc, char **argv)
{
	if (!ca_cert_sz) {
		shell_error(shell, "No cert loaded");
		return -EINVAL;
	}
	memset(mxfer_buf, 0, 2048);
	mbedtls_x509_crt_init(&ca_x509);
	mbedtls_x509_crt_parse_der_nocopy(&ca_x509, ca_cert, ca_cert_sz);
	mbedtls_x509_crt_info(mxfer_buf, 2048, "  ", &ca_x509);
	mbedtls_x509_crt_free(&ca_x509);
	shell_print(shell, "Loaded cert info:\n%s", mxfer_buf);
	return 0;
}

size_t parse_cert_sz = 0, buf_idx = 0;
enum {
	outside_cert,
	in_cert_tag,
	in_begin_tag,
	in_cert,
} parse_state;

static char pchr;

static inline bool isspace(char c) 
{
	return (c == '\t' || c == '\n' ||
			c == '\v' || c == '\f' || c == '\r' || c == ' ' ? 1 : 0);
}

static void parse(struct fs_file_t *file, char * fragment, size_t fragment_len) 
{
	char chr;
	// char dec_buf[96] = {0};
	for (int i = 0; i < fragment_len; i++){
		chr = fragment[i];
		if (chr == '-'){
			if (parse_state == in_cert) {
				parse_state = outside_cert;
				memset(ca_cert, 0, sizeof(ca_cert));
				base64_decode(ca_cert, sizeof(ca_cert), &parse_cert_sz, dec_buf, buf_idx);
				//Write to file
				struct cert_record rec = {0};
				rec.cert_sz = sys_cpu_to_be16(parse_cert_sz);
				//fs_write(file, (uint8_t*)&sz_be, 2);
				mbedtls_x509_crt_init(&ca_x509);
				mbedtls_x509_crt_parse_der_nocopy(&ca_x509, ca_cert, parse_cert_sz);
				mbedtls_x509_crt_info(dec_buf, 2048, "", &ca_x509);
				int ver;
				char *ver_start = strstr(dec_buf, "cert. version     : ");
				ver_start += sizeof("cert. version     : ") - 1;
				ver = strtol(ver_start, NULL, 10);
				cert_cnt++;
				if (ver != 0){
					char *cn, *eol; 
					cn = strstr(dec_buf, "subject name");
					eol = strstr(cn, "\n");
					cn = strstr(cn, "CN=");
					if (cn && strstr(cn, ",") && strstr(cn, ",") < eol) {
						eol = strstr(cn, ",");
					}
					if (!cn || cn > eol) {
						cn = "";
					} else {
						cn += 3;
						memcpy(rec.cert_cn, cn, MIN(eol-cn, 64));
					}
					fs_write(file, &rec, sizeof(rec));
					// printk("Installed cert: %s\n%s\n", rec.cert_cn, dec_buf);
					if (strlen(rec.cert_cn)){
						printk("Installed cert: %s\n", rec.cert_cn);
					} else {
						printk("Installed cert: %s\n", "<NO CN SPECIFIED>");
					}
					fs_write(file, ca_cert, parse_cert_sz);
					success_cnt++;
				} else {
					printk("Cert parse failure.\n");
				}
				mbedtls_x509_crt_free(&ca_x509);
				memset(dec_buf, 0, 3000);
				buf_idx = 0;
			} else if (pchr == '\n') {
				parse_state = in_cert_tag;
			}
		}
		if (chr == '\n' && parse_state == in_begin_tag) {
			parse_state = in_cert;
		} else if (chr == 'B' && parse_state == in_cert_tag){
			parse_state = in_begin_tag;
		} else if (parse_state == in_cert) {
			if (!isspace(chr)){
				dec_buf[buf_idx] = chr;
				buf_idx++;
			}
		}
		pchr = chr;
	}
}

static size_t http_total_received;

int tmo_cert_dld(int devid, char *url) 
{
	tls_credential_delete(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE);
	tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
			digicert_ca, sizeof(digicert_ca));
	int ret = -1;
	struct fs_file_t file = {0};
	struct fs_file_t tmp_file = {0};
	struct fs_dirent dirent = {0};

	http_total_received = 0;

	// Assume fs is already mounted
	char *filename = CERT_BIN_LOCATION;
	printf("Opening file %s\n", filename);

	if (fs_stat(CERT_BIN_FOLDER, &dirent) == -ENOENT) {
		fs_mkdir(CERT_BIN_FOLDER);
	}

	ret = fs_open(&file, filename, FS_O_CREATE | FS_O_WRITE);
	if (ret != 0) {
		printf("Error: could not open file %s\n", filename);
		goto exit;
	}

	ret = fs_truncate(&file, 0);
	if (ret != 0) {
		printf("Could not truncate file %s\n", filename);
		goto exit;
	}

	http_total_received = tmo_http_download(devid, url, "/tmo/certs.tmp");
	
	if (http_total_received <= 0) {
		goto exit;
	}

	ret = fs_open(&tmp_file, "/tmo/certs.tmp", FS_O_READ);
	if (ret != 0) {
		printf("Error: could not open file %s\n", "/tmo/certs.tmp");
		goto exit;
	}

	ssize_t read;
	char frag_buf[64];

	cert_cnt = 0;
	success_cnt = 0;

	printf("\n");
	do {
		read = fs_read(&tmp_file, frag_buf, 64);
		parse(&file, frag_buf, read);
	} while (read);

	fs_close(&tmp_file);
	fs_unlink("/tmo/certs.tmp");
	
	printf("Downloaded %d certs, installed %d sucessfully\n", cert_cnt, success_cnt);
exit:
	fs_close(&file);
	fs_close(&tmp_file);
	if (ret < 0) {
		return ret;
	} else {
		return http_total_received;
	}
}

int cmd_tmo_cert_dld(const struct shell *shell, size_t argc, char **argv)
{
	int ret = -1;

	if (argc < 2 || argc > 3) {
		shell_error(shell, "Missing required argument");
		shell_print(shell, "Usage: tmo certs dld <devid> <URL>"
				"       devid: 1 for modem, 2 for wifi\n");
		// z_shell_help_subcmd_print_selitem(shell);
		return -EINVAL;
	}

	int devid = strtol(argv[1], NULL, 10);
	if (argc < 3) {
		ret = tmo_cert_dld(devid, "https://ccadb-public.secure.force.com/mozilla/IncludedRootsPEMTxt?TrustBitsInclude=Websites");
	} else {
		ret = tmo_cert_dld(devid, argv[2]);
	}
	if (ret < 0) {
		shell_error(shell, "tmo_http_download returned %d", ret);
	}
	return ret;
}
