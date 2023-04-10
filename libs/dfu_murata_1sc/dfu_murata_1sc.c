/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Device Firmware Update (DFU) support for Murata 1SC
 *
 * This module is based on Alt1250AtDeltaImgUpgTool.py provided by Murata
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/reboot.h>
#include <mbedtls/sha1.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/modem/murata-1sc.h>
#include <limits.h>

// #include "tmo_dfu_download.h"
#include "dfu_murata_1sc.h"
// #include "tmo_shell.h"
// #include "tmo_modem.h"

/* The Murata 1SC updates below are "delta files" for updating
 * between two FW versions. Early (Beta/Pilot) dev kits contain
 * "sample" versions of such firmware. Production dev kits
 * will use "golden" images. The first two below are for testing
 * between two "sample" images. The last two below are for testing
 * between "golden" images.
 */
struct dfu_file_t dfu_files_modem[] = {
	{"Murata 1SC: 20351 to 20161 Sample",
	 "/tmo/1sc_update.ua",
	 "",
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{"", "", "", ""}};

/***************************************
 *  Murata 1SC FW Update
 */

#define DFU_CHUNK_SIZE	    1024UL
#define DFU_IN_BETWEEN_FILE 0UL
#define DFU_START_OF_FILE   1UL
#define DFU_END_OF_FILE	    2UL
#define DFU_FW_VER_SIZE	    20UL

#define MODEM_FLASH_SECTOR    0x200000
#define AT_GET_FILE_MODE      100
#define AT_GET_CHKSUM_ABILITY 101
#define AT_INIT_FW_XFER	      102
#define AT_SEND_FW_HEADER     200
#define AT_SEND_FW_DATA	      300
#define AT_SEND_FW_DATA_DONE  400
#define AT_INIT_FW_UPGRADE    500
#define AT_RESET_MODEM	      600

static uint8_t const murata_20161_version_str[] = {"RK_03_02_00_00_20161_001"};
static uint8_t const murata_20351_version_str[] = {"RK_03_02_00_00_20351_001"};

struct init_fw_data_t init_xfer_params;
struct send_fw_data_t send_params;

typedef enum modem_app_state_e {
	MODEM_INITIAL_STATE = 0,
	MODEM_RADIO_INIT_STATE,
	MODEM_FW_UPGRADE,
	MODEM_FW_UPGRADE_DONE
} modem_app_state_t;

/* application control block */
typedef struct modem_app_cb_s {
	/* wlan application state */
	modem_app_state_t state;
} modem_app_cb_t;

/* application control block */
modem_app_cb_t modem_app_cb;

/* FW send variable, buffer */
static uint32_t chunk_cnt = 0u, chunk_check = 0u, offset = 0u, fw_image_size = 0u, remainder = 0u;
static uint8_t recv_buff_hdr[UA_HEADER_SIZE] = {0};
static uint8_t recv_buff_1k[1024] = {0};

mbedtls_sha1_context modem_sha1_ctx;
unsigned char modem_sha1_output[DFU_SHA1_LEN];

static struct fs_file_t modemfile = {0};
static int readbytes = 0;
static int totalreadbytes = 0;
static uint32_t crc32 = 0;

static uint32_t crctab[] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2,
	0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3,
	0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac,
	0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
	0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e,
	0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d, 0xd4326d90,
	0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a,
	0xec7dd02d, 0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c,
	0x2e003dc5, 0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13,
	0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1,
	0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba, 0xaca5c697, 0xa864db20,
	0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f,
	0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
	0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055,
	0xfef34de2, 0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632,
	0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
	0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e, 0x21dc2629, 0x2c9f00f0,
	0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91,
	0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e,
	0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604,
	0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615,
	0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a,
	0x8cf30bad, 0x81b02d74, 0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
	0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f,
	0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec, 0x3793a651,
	0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
	0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb,
	0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa,
	0xf9278673, 0xfde69bc4, 0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5,
	0x9e7d9662, 0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

/**
 * @brief Update the CRC32 value, intended for use on a block-wise basis
 *
 * @param CRC32 is the running calculation of CRC32
 * @param data points to file data from which to compute CRC32
 * @param len is the size of the data block in bytes
 *
 * @return the updated CRC32 value
 */
uint32_t murata_1sc_crc32_update(uint32_t crc32, const uint8_t *data, size_t len)
{
	for (int i = 0; i < len; i++) {
		uint8_t ch = data[i];
		int tabidx = (crc32 >> 24) ^ ch;
		crc32 = (crc32 << 8) ^ crctab[tabidx];
	}
	return crc32;
}

/**
 * @brief Use the file size to complete the CRC32 calculation
 *
 * @param crc32 is the total calculated by crc32_update
 * @param len is the file size minus header size (256 bytes)
 *
 * @return the updated CRC32 value
 */
uint32_t murata_1sc_crc32_finish(uint32_t crc32, size_t len)
{
	while (len) {
		uint8_t c = len & 0xff;
		len = len >> 8;
		crc32 = (crc32 << 8) ^ crctab[(crc32 >> 24) ^ c];
	};
	return (~crc32);
}

/**
 * @brief Determine the running fw_image_size, CRC32, and SHA1
 */
static int file_update_check(const struct dfu_file_t *dfu_file, uint32_t bytesToRead,
			     uint32_t *crc32)
{
	readbytes = fs_read(&modemfile, recv_buff_1k, bytesToRead);
	if (readbytes < 0) {
		printf("Could not read update file %s\n", dfu_file->lfile);
		return -1;
	}

	if (readbytes > 0) {
		totalreadbytes += readbytes;
		mbedtls_sha1_update(&modem_sha1_ctx, (unsigned char *)recv_buff_1k, readbytes);
	}

	printk(".");

	if (crc32 && (readbytes > 0)) {
		fw_image_size += readbytes;
		*crc32 = murata_1sc_crc32_update(*crc32, recv_buff_1k, readbytes);
	}

	if (readbytes == 0) {
		*crc32 = murata_1sc_crc32_finish(*crc32, fw_image_size);
		printf("\n\tfile size: %d, image size: %d, MCRC32: 0x%x (%u)", totalreadbytes,
		       fw_image_size, *crc32, (uint32_t)*crc32);
	}
	return readbytes;
}

static int file_read_flash_hdr(const struct dfu_file_t *dfu_file, uint32_t offset)
{
	readbytes = fs_read(&modemfile, recv_buff_hdr, UA_HEADER_SIZE);
	if (readbytes < 0) {
		printf("Could not read update file %s\n", dfu_file->lfile);
		return -1;
	}

	totalreadbytes += readbytes;
	return 0;
}

static int file_read_flash(const struct dfu_file_t *dfu_file, uint32_t bytesToRead)
{
	readbytes = fs_read(&modemfile, recv_buff_1k, bytesToRead);
	if (readbytes < 0) {
		printf("Could not read update file %s\n", dfu_file->lfile);
		return -1;
	}

	totalreadbytes += readbytes;
	return 0;
}

/**
 * @brief A helper to use an offload socket like a normal one
 *
 * @param family
 * @param type
 * @param proto
 * @param iface
 * @return int
 */
static inline int zsock_socket_ext(int family, int type, int proto, struct net_if *iface)
{
	if (iface->if_dev->offload && iface->if_dev->socket_offload != NULL) {
		return iface->if_dev->socket_offload(family, type, proto);
	} else {
		errno = EINVAL;

		return -1;
	}
}

#ifdef CONFIG_SOC_SERIES_EFM32PG12B

/**
 * @brief Passes a pointers data to fnctl which requires int.
 * This code is not portable, it expects the pointer to be representable
 * as a positive signed integer. This should not be an issue on the
 * EFM32PG12 since it does not address any RAM or ROM above 0x24000000
 *
 * Implementers of additional systems need to ensure this call is safe for their
 * platform.
 *
 * @param sock The sock to send to
 * @param cmd The command to send
 * @param ptr The pointer to send encoded as a positive signed integer.
 * @return int The fcntl return if the pointer can be cast, or -EOVERFLOW on error
 */
static int fcntl_ptr(int sock, int cmd, const void *ptr)
{
	uintptr_t uiptr = (uintptr_t)ptr;
	/* It's fine if this overflows */
	unsigned int flags = uiptr;

	if (uiptr > INT_MAX) {
		return -EOVERFLOW;
	}

	/* va_arg is defined for (unsigned T) -> (signed T) as long as the value can be represented
	 * by both types (C11 7.16.1.1) */
	return zsock_fcntl(sock, cmd, flags);
}

#else
static int fcntl_ptr(int sock, int cmd, const void *ptr)
{
	(void)sock;
	(void)cmd;
	(void)ptr;

	return -ENOTSUP;
}

#endif

int dfu_send_ioctl(int cmd, int numofbytes)
{
	int res = -1;
	struct net_if *iface = net_if_get_by_index(1);
	int sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);
	if (sd < 0) {
		return sd;
	}

	switch (cmd) {
	case AT_GET_FILE_MODE:
		res = fcntl_ptr(sd, GET_FILE_MODE, recv_buff_hdr);
		if (res < 0) {
			printf("GET_FILE_MODE failed\n");
		}
		break;

	case AT_GET_CHKSUM_ABILITY:
		res = fcntl_ptr(sd, GET_CHKSUM_ABILITY, recv_buff_hdr);
		if (res < 0) {
			printf("GET_CHKSUM_ABILITY failed\n");
		}
		break;

	case AT_INIT_FW_XFER:
		init_xfer_params.imagename = "b:/update.ua";
		init_xfer_params.imagesize = (uint32_t)fw_image_size;
		init_xfer_params.imagecrc = (uint32_t)crc32;

		printf("\n\tSend INIT_FW_XFER with image filename %s image size of %u and image "
		       "crc32 %u\n",
		       init_xfer_params.imagename, init_xfer_params.imagesize,
		       init_xfer_params.imagecrc);

		res = fcntl_ptr(sd, INIT_FW_XFER, &init_xfer_params);
		if (res < 0) {
			printf("\tINIT_FW_XFER failed with update.ua, error %d\n", res);
		} else {
			printf("\tINIT_FW_XFER passed software upgrade step (image pre-check, "
			       "update, etc.)\n");
		}
		break;

	case AT_SEND_FW_HEADER:
		res = fcntl_ptr(sd, SEND_FW_HEADER, recv_buff_hdr);
		if (res < 0) {
			printf("\tSEND_FW_HEADER failed\n");
		} else if (recv_buff_hdr[0] == 0) {
			printf("SEND_FW_HEADER failed with empty response\n");
		} else {
			printf("\tSEND_FW_HEADER succeeded\n");
		}
		break;

	case AT_SEND_FW_DATA:
		send_params.data = recv_buff_1k;
		send_params.more = 1;
		send_params.len = numofbytes;

		res = fcntl_ptr(sd, SEND_FW_DATA, &send_params);

		if (res < 0) {
			printf("\tSEND_FW_DATA failed, error %d\n", res);
		}
		break;

	case AT_SEND_FW_DATA_DONE:
		send_params.data = recv_buff_1k;
		send_params.more = 0;
		send_params.len = numofbytes;

		res = fcntl_ptr(sd, SEND_FW_DATA, &send_params);
		if (res < 0) {
			printf("\tSEND_FW_DATADONE failed, error %d\n", res);
		} else {
			printf("\tSEND_FW_DATADONE succeeded %d\n", res);
		}
		break;

	case AT_INIT_FW_UPGRADE:
		res = fcntl_ptr(sd, INIT_FW_UPGRADE, "b:/update.ua");
		if (res < 0) {
			printf("\tAT_INIT_FW_UPGRADE failed with update.ua\n");
		} else {
			printf("\tAT_INIT_FW_UPGRADE: ");
			switch (res) {
			case 0:
				printf("0 - successfully finished software upgrade step (image "
				       "pre-check, update, etc.)\n");
				break;
			case 1:
				printf("1 - failed with general upgrade errors\n");
				break;
			case 2:
				printf("2 - failed delta image pre-check\n");
				break;
			case 3:
				printf("3 - failed image validation failure\n");
				break;
			case 4:
				printf("4 - failed to update\n");
				break;
			case 5:
				printf("5 - failed delta update Agent was not found\n");
				break;
			case 6:
				printf("6 - failed no upgrade result is found\n");
				break;
			default:
				printf("AT_INIT_FW_UPGRADE failed to confgure update.ua\n");
				break;
			}
		}
		break;

	case AT_RESET_MODEM:
		res = fcntl_ptr(sd, RESET_MODEM, NULL);
		if (res < 0) {
			printf("\tAT_RESET_MODEM failed\n");
		} else {
			if (res == 0) {
				printf("\tAT_RESET_MODEM succeeded\n");
			}
		}
		break;

	default:
		printf("error: unrecognized command %d\n", cmd);
		break;
	}

	zsock_close(sd);
	return res;
}

static int32_t dfu_modem_write_image(const struct dfu_file_t *dfu_file)
{
	int32_t status = 0;

	printf("\nThis procedure can take up to 10 minutes. System will reboot when done\n");
	printf("DO NOT REBOOT OR POWER OFF DURING THIS PROCEDURE\n");
	printf("Otherwise the FW update may fail\n");

	uint8_t modemFwUpgradeDone = 0;
	while (!modemFwUpgradeDone) {
		switch (modem_app_cb.state) {
		case MODEM_INITIAL_STATE: {
			printf("\nStage 1: Image update pre-checks (compute SHA1, crc32, etc) (~15 "
			       "seconds)\n");

			printf("\tChecking for %s to be present\n", dfu_file->lfile);
			if (fs_open(&modemfile, dfu_file->lfile, FS_O_READ) != 0) {
				printf("\tThe file %s is missing\n", dfu_file->lfile);
				return -1;
			} else {
				printf("\tThe required file %s is present\n", dfu_file->lfile);
			}

			mbedtls_sha1_init(&modem_sha1_ctx);
			memset(modem_sha1_output, 0, sizeof(modem_sha1_output));
			mbedtls_sha1_starts(&modem_sha1_ctx);

			readbytes = 0;
			totalreadbytes = 0;
			fw_image_size = 0;
			crc32 = 0;

			printf("\tCalculating filesize, imagesize, MCRC32 and SHA1\n");

			int notdone = 1;
			int bytes_read = 0;
			bytes_read = file_update_check(dfu_file, UA_HEADER_SIZE, NULL);
			if (bytes_read == UA_HEADER_SIZE) {
				while (notdone) {
					bytes_read = file_update_check(dfu_file, 1024, &crc32);
					if (bytes_read == 0) {
						notdone = 0;
					}
				}
			} else {
				printf("Error reading file header\n");
				return -1;
			}

			int res = dfu_send_ioctl(AT_GET_FILE_MODE, 0);

			res = dfu_send_ioctl(AT_INIT_FW_XFER, 0);
			printf("\tGet init_fw_xfer results %d\n", res);

			res = dfu_send_ioctl(AT_GET_CHKSUM_ABILITY, 0);
			printf("\tGet file chksum ability results %d\n", res);

			mbedtls_sha1_init(&modem_sha1_ctx);
			memset(modem_sha1_output, 0, sizeof(modem_sha1_output));
			mbedtls_sha1_starts(&modem_sha1_ctx);

			/* update modem application state */
			modem_app_cb.state = MODEM_FW_UPGRADE;
		}

			/* no break */
		case MODEM_FW_UPGRADE: {
			printf("\nStage 2: Write upgrade data to modem flash (~2-3 minutes)\n");

			/* Reset the file pointers to begin the update */
			fs_seek(&modemfile, 0, FS_SEEK_SET);

			readbytes = 0;
			totalreadbytes = 0;
			crc32 = 0;

			int readsize = 0;
			if (file_read_flash_hdr(dfu_file, offset) != 0) {
				printf("file system flash read failed\n");
				return (-1);
			}

			printf("\tSending image header\n");
			int res = dfu_send_ioctl(AT_SEND_FW_HEADER, UA_HEADER_SIZE);
			if (res < 0) {
				return -1;
			}

			/* Calculate the total number of chunks */
			/* file_update_check determined the fw_image_size previous to here */
			chunk_check = (fw_image_size / DFU_CHUNK_SIZE);
			remainder = fw_image_size % DFU_CHUNK_SIZE;
			if (fw_image_size % DFU_CHUNK_SIZE) {
				chunk_check += 1;
			}

			printf("\tfw_image_size %d num of chunks %d - remainder %d\n",
			       fw_image_size, chunk_check, remainder);

			/* Loop until all the chunks are read and written */
			while (offset <= fw_image_size) {
				if (chunk_cnt < (chunk_check - 1)) {
					readsize = DFU_CHUNK_SIZE;
				} else {
					readsize = remainder;
				}

				if (readsize > 0) {
					if (file_read_flash(dfu_file, readsize) != 0) {
						printf("file system flash read failed\n");
						return (-1);
					}
				} else {
					printf("Flash read of zero bytes ?\n");
				}

				if (chunk_cnt == 0) {
					printf("\tModem FW update first chunk\n");
					dfu_send_ioctl(AT_SEND_FW_DATA, readsize);
					if (status != 0) {
						printf("\nError %d in modem FW update final "
						       "chunk\n",
						       status);
						return (-1);
					}
				} else if (chunk_cnt == (chunk_check - 1)) {
					printf("\n\tFinalizing remainder chunk %d, (2-3 minutes)\n",
					       readsize);
					dfu_send_ioctl(AT_SEND_FW_DATA_DONE, readsize);
					if (status != 0) {
						printf("\nError %d in modem FW update final "
						       "chunk\n",
						       status);
						break;
					}
					modem_app_cb.state = MODEM_FW_UPGRADE_DONE;
				} else {
					printk(".");
					dfu_send_ioctl(AT_SEND_FW_DATA, readsize);
					if (status != 0) {
						printf("\nError %d in modem FW update in-between "
						       "chunks\n",
						       status);
						break;
					}
				}
				offset += DFU_CHUNK_SIZE;
				memset(recv_buff_1k, 0, sizeof(recv_buff_1k));
				chunk_cnt++;
			} /* end While Loop */
		}	  /* End case of  */
		break;

		case MODEM_FW_UPGRADE_DONE: {
			printf("\nStage 3: Issuing INIT_FW_UPGRADE (finalizing) (~3 minutes)\n");
			dfu_send_ioctl(AT_INIT_FW_UPGRADE, 0);

			printf("\tIssuing AT_RESET_MODEM, and waiting for modem to finish "
			       "updating\n");
			dfu_send_ioctl(AT_RESET_MODEM, 0);

#define MODEM_UPDATE_TIME 180
			for (int i = 0; i < MODEM_UPDATE_TIME; i++) {
				printk(".");
				k_sleep(K_SECONDS(1));
			}

			modemFwUpgradeDone = 1;
			printf("\n\tMurata 1SC FW upgrade completed, rebooting system...\n");

			fs_close(&modemfile);
			sys_reboot(SYS_REBOOT_COLD);
		} break;

		default:
			printf("\nerror: dfu_modem_write_image: default case\n");
			fs_close(&modemfile);
			break;
		} /* end of switch */
	}
	return status;
} /* end of routine */

int dfu_modem_get_version(char *dfu_murata_version_str)
{
	struct net_if *iface = net_if_get_by_index(1);
	int sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);
	if (sd == -1) {
		return 0;
	}
	memset(dfu_murata_version_str, 0, DFU_MODEM_FW_VER_SIZE);
	strcpy(dfu_murata_version_str, "VERSION");
	int res = fcntl_ptr(sd, GET_ATCMD_RESP, dfu_murata_version_str);
	zsock_close(sd);

	if (res < 0) {
		printf("VERSION failed\n");
	} else if (dfu_murata_version_str[0] == 0) {
		printf("VERSION failed with empty response\n");
	} else {
		printf("The current Murata 1SC FW version is: %s\n", dfu_murata_version_str);
	}

	return res;
}

int dfu_modem_is_golden()
{
	char dfu_golden_str[MDM_GOLDEN_LEN] = "GOLDEN";
	struct net_if *iface = net_if_get_by_index(1);
	int sd = zsock_socket_ext(AF_INET, SOCK_STREAM, IPPROTO_TCP, iface);
	if (sd == -1) {
		return 0;
	}
	int res = fcntl_ptr(sd, GET_ATCMD_RESP, dfu_golden_str);
	zsock_close(sd);

	if (res < 0) {
		errno = res;
		return -1;
	}
	printf("The current image type is %s\n", dfu_golden_str);
	if (strncmp(dfu_golden_str, "GOLDEN", sizeof("GOLDEN")) == 0) {
		return 1;
	}
	if (strncmp(dfu_golden_str, "SAMPLE", sizeof("SAMPLE")) == 0) {
		return 0;
	}
	printf("error: unknown image type %s\n", dfu_golden_str);
	return -1;
}

int select_modem_file(char *filename)
{
	char *token;
	char *rest = filename;
	int idx = 0;
	char from[10];

	while ((token = strtok_r(rest, ".", &rest))) {
		if (idx == 1) {
			strcpy(from, token);
		} else if (idx == 2) {
			if (!strcmp(from, "20161") && !strcmp(token, "20351")) {
				return 1;
			}
		}
		idx++;
	}
	return 0;
}

int dfu_modem_firmware_upgrade(const struct dfu_file_t *dfu_file)
{
	int ret = 0;
	char dfu_murata_version_str[DFU_MODEM_FW_VER_SIZE];
	int selected_murata_file = 0;

	int status = dfu_modem_get_version(dfu_murata_version_str);
	if (status != 0) {
		printf("Murata 1SC FW update has aborted - reading the Murata 1SC FW version has "
		       "failed !\n");
	}

	selected_murata_file = select_modem_file((char *)dfu_file->rfile);

	int is_golden = dfu_modem_is_golden();

	if (is_golden && (selected_murata_file == 0 || selected_murata_file == 1)) {
		printf("Cannot update current golden image to sample image\n");
		return -1;
	}

	if (!is_golden && (selected_murata_file == 2 || selected_murata_file == 3)) {
		printf("Cannot update current sample image to golden image\n");
		return -1;
	}

	switch (selected_murata_file) {
	case 0:
	case 2:
		printf("Case 2 %s %s\n", dfu_murata_version_str, murata_20161_version_str);
		if (strcmp(dfu_murata_version_str, murata_20161_version_str) == 0) {
			printf("Invalid FW update selection - the Murata 1SC FW version is already "
			       "%s\n",
			       dfu_murata_version_str);
			return (-1);
		}
		printf("Requesting FW update from %s to %s\n", dfu_murata_version_str,
		       murata_20161_version_str);
		break;

	case 1:
	case 3:
		printf("Case 3 %s %s\n", dfu_murata_version_str, murata_20351_version_str);
		if (strcmp(dfu_murata_version_str, murata_20351_version_str) == 0) {
			printf("case 3 if\n");
			printf("Invalid FW update selection - the Murata 1SC FW version is already "
			       "%s\n",
			       dfu_murata_version_str);
			return (-1);
		}
		printf("Requesting FW update from %s to %s\n", dfu_murata_version_str,
		       murata_20351_version_str);
		break;

	default:
		printf("Unknown file\n");
		return -1;
	}

	printf("write image to modem\n");
	ret = dfu_modem_write_image(dfu_file);
	return ret;
}
