/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Device Firmware Update (DFU) support for SiLabs Pearl Gecko
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dfu_gecko_lib, LOG_LEVEL_INF);

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <soc.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/fs/fs.h>
#include <mbedtls/sha1.h>
#include <zephyr/sys/byteorder.h>
#include "dfu_gecko_lib.h"

// SHAs are set to 0 since they are unknown before a build
const struct dfu_file_t dfu_files_mcu[] = {
	{
		"Gecko MCU 1/4",
		"/tmo/zephyr.slot0.bin",
		"tmo_shell.tmo_dev_edge.slot0.bin",
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	},
	{
		"Gecko MCU 2/4",
		"/tmo/zephyr.slot1.bin",
		"tmo_shell.tmo_dev_edge.slot1.bin",
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	},
	{
		"Gecko MCU 3/4",
		"/tmo/zephyr.slot0.bin.sha1",
		"tmo_shell.tmo_dev_edge.slot0.bin.sha1",
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	},
	{
		"Gecko MCU 4/4",
		"/tmo/zephyr.slot1.bin.sha1",
		"tmo_shell.tmo_dev_edge.slot1.bin.sha1",
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	},

	{"","","",""}
};

#ifdef BOOT_SLOT

#define DFU_XFER_SIZE_2K    2048UL
#define DFU_CHUNK_SIZE      2048UL
#define DFU_IN_BETWEEN_FILE 0UL
#define DFU_START_OF_FILE   1UL
#define DFU_END_OF_FILE     2UL
#define DFU_FW_VER_SIZE     20UL

// slot partition addresses
#define GECKO_IMAGE_SLOT_0_SECTOR 0x10000
#define GECKO_IMAGE_SLOT_1_SECTOR 0x80000

typedef enum gecko_app_state_e {
	GECKO_INITIAL_STATE = 0,
	GECKO_INIT_STATE,
	GECKO_FW_UPGRADE,
	GECKO_FW_UPGRADE_DONE
} gecko_app_state_t;

// gecko FW update application control block
typedef struct gecko_app_cb_s {
	// gecko FW update application state
	gecko_app_state_t state;
} gecko_app_cb_t;

// application control block
gecko_app_cb_t gecko_app_cb;

// FW send variable , buffer
static uint32_t chunk_cnt = 0u, chunk_check = 0u, offset = 0u, fw_image_size = 0u;
static int32_t status = 0;
static uint8_t image_buffer[DFU_CHUNK_SIZE] = { 0 };
static uint8_t check_buf[DFU_CHUNK_SIZE + 1];
static int requested_slot_to_upgrade = -1;

#define GECKO_INCRE_PAGE 0
#define GECKO_INIT_PAGE 1
#define GECKO_FLASH_SECTOR 0x00000
extern int read_image_from_flash(uint8_t *flash_read_buffer, int readBytes, uint32_t flashStartSector, int ImageFileNum);

static struct fs_file_t geckofile = {0};
static int readbytes = 0;
static int totalreadbytes = 0;
static int totalwritebytes = 0;

static struct fs_file_t gecko_sha1_file = {0};
static mbedtls_sha1_context gecko_sha1_ctx;
static unsigned char gecko_sha1_output[DFU_SHA1_LEN];
static unsigned char gecko_expected_sha1[DFU_SHA1_LEN*2];
static unsigned char gecko_expected_sha1_final[DFU_SHA1_LEN];

extern const struct device *gecko_flash_dev;

static uint32_t crc32;
extern uint32_t crc32_ieee_update(uint32_t crc, const uint8_t * data, size_t len );

#define IMAGE_MAGIC                 0x96f3b83d
#define IMAGE_MAGIC_V1              0x96f3b83c
#define IMAGE_MAGIC_NONE            0xffffffff
#define IMAGE_TLV_INFO_MAGIC        0x6907
#define IMAGE_TLV_PROT_INFO_MAGIC   0x6908

#define IMAGE_HEADER_SIZE           32

struct image_version {
	uint8_t iv_major;
	uint8_t iv_minor;
	uint16_t iv_revision;
	uint32_t iv_build_num;
};

/** Image header.  All fields are in little endian byte order. */
struct image_header {
	uint32_t ih_magic;
	uint32_t ih_load_addr;
	uint16_t ih_hdr_size;           /* Size of image header (bytes). */
	uint16_t ih_protect_tlv_size;   /* Size of protected TLV area (bytes). */
	uint32_t ih_img_size;           /* Does not include header. */
	uint32_t ih_flags;              /* IMAGE_F_[...]. */
	struct image_version ih_ver;
	uint32_t _pad1;
};

static int compare_sha1(int slot_to_upgrade)
{
	printf("Comparing SHA1 for file zephyr.slot%d.bin\n", slot_to_upgrade);

	printf("\tExpected SHA1:\n\t\t");
	for (int i = 0; i < DFU_SHA1_LEN; i++) {
		printf("%02x ", gecko_expected_sha1_final[i]);
	}

	int sha1_fails = 0;
	for (int i = 0; i < DFU_SHA1_LEN; i++) {
		if (gecko_sha1_output[i] != gecko_expected_sha1_final[i]) {
			sha1_fails++;
			break;
		}
	}

	if (sha1_fails) {
		printf("Error: The computed SHA1 doesn't match expected\n");
		return -1;
	}
	else
	{
		printf("SHA1 matches\n");
		return 0;
	}
}

static int slot_version_cmp(struct image_version *ver1,
		struct image_version *ver2)
{
	/* Compare major version numbers */
	if (ver1->iv_major > ver2->iv_major) {
		LOG_DBG("Slot image header Major version compare 1. %u vs %u\n", ver1->iv_major, ver2->iv_major);
		return 0;
	}
	if (ver1->iv_major < ver2->iv_major) {
		LOG_DBG("Slot image header Major version compare 2. %u vs %u\n", ver1->iv_major, ver2->iv_major);
		return 1;
	}

	/* Compare minor version numbers */
	if (ver1->iv_minor > ver2->iv_minor) {
		LOG_DBG("Slot image header Minor version compare 3. %u vs %u\n", ver1->iv_minor, ver2->iv_minor);
		return 0;
	}
	if (ver1->iv_minor < ver2->iv_minor) {
		LOG_DBG("Slot image header Minor version compare 4. %u vs %u\n", ver1->iv_minor, ver2->iv_minor);
		return 1;
	}

	/* Compare revision numbers */
	if (ver1->iv_revision > ver2->iv_revision) {
		LOG_DBG("Slot image header revision version compare 5. %u vs %u\n", ver1->iv_revision, ver2->iv_revision );
		return 0;
	}
	if (ver1->iv_revision < ver2->iv_revision) {
		LOG_DBG("Slot image header revision version compare 6. %u vs %u\n", ver1->iv_revision, ver2->iv_revision );
		return 1;
	}

	LOG_DBG("Slot image header versions are equal\n");
	return -1;
}

int get_gecko_fw_version(int slot, char *version, int max_len)
{
	uint8_t read_buf[IMAGE_HEADER_SIZE];
	struct image_header slot_hdr;

	if (slot < 0 || slot > 1)
		return -1;

	/* Read slot header */
	flash_read(gecko_flash_dev, slot ? GECKO_IMAGE_SLOT_1_SECTOR : GECKO_IMAGE_SLOT_0_SECTOR,
			read_buf, IMAGE_HEADER_SIZE);
	memcpy(&slot_hdr, &read_buf, IMAGE_HEADER_SIZE);

	if (slot_hdr.ih_magic != IMAGE_MAGIC) {
		return -1;
	}
	snprintf(version, max_len, "%u.%u.%u+%u",
			slot_hdr.ih_ver.iv_major,
			slot_hdr.ih_ver.iv_minor,
			slot_hdr.ih_ver.iv_revision,
			slot_hdr.ih_ver.iv_build_num);

	return 0;
}


int print_gecko_slot_info(void)
{
	int slot0_has_image = 0;
	int slot1_has_image = 0;
	int active_slot = -1;
	uint8_t read_buf[IMAGE_HEADER_SIZE];
	struct image_header slot0_hdr;
	struct image_header slot1_hdr;

	/* Read Slot 0 header */
	flash_read(gecko_flash_dev, GECKO_IMAGE_SLOT_0_SECTOR, read_buf, IMAGE_HEADER_SIZE);
	memcpy(&slot0_hdr, &read_buf, IMAGE_HEADER_SIZE);

	if (slot0_hdr.ih_magic == IMAGE_MAGIC) {
		slot0_has_image = 1;
	}

	/* Read Slot 1 header */
	flash_read(gecko_flash_dev, GECKO_IMAGE_SLOT_1_SECTOR, read_buf, IMAGE_HEADER_SIZE);
	memcpy(&slot1_hdr, &read_buf, IMAGE_HEADER_SIZE);

	if (slot1_hdr.ih_magic == IMAGE_MAGIC) {
		slot1_has_image = 1;
	}

	/* Determine the active slot */
	if (slot0_has_image && slot1_has_image) {
		active_slot = slot_version_cmp(&slot0_hdr.ih_ver, &slot1_hdr.ih_ver);

		if (active_slot < 0) {
			printf("NOTE: Image versions are the same\n");
			active_slot = 0;
		}
	} else if (slot0_has_image) {
		active_slot = 0;
	} else if (slot1_has_image) {
		active_slot = 1;
	}

	/* Print the results */
	printf("Slot Bootable Active Version\n");
	if (slot0_hdr.ih_magic == IMAGE_MAGIC) {
		printf("0    Yes      %-6s %u.%u.%u+%u\n",
			        active_slot == 0 ? "Yes": "No ",
				slot0_hdr.ih_ver.iv_major,
				slot0_hdr.ih_ver.iv_minor,
				slot0_hdr.ih_ver.iv_revision,
				slot0_hdr.ih_ver.iv_build_num);
	} else {
		printf("0    No       N/A    N/A\n");
	}
	if (slot1_hdr.ih_magic == IMAGE_MAGIC) {
		printf("1    Yes      %-6s %u.%u.%u+%u\n",
			        active_slot == 1 ? "Yes": "No ",
				slot1_hdr.ih_ver.iv_major,
				slot1_hdr.ih_ver.iv_minor,
				slot1_hdr.ih_ver.iv_revision,
				slot1_hdr.ih_ver.iv_build_num);
	} else {
		printf("1    No       N/A    N/A\n");
	}
	return 0;
}

// This function gets the size of the Gecko zephyr firmware
static uint32_t get_gecko_fw_size(void)
{
	int notdone = 1;
	totalreadbytes = 0;

	while (notdone)
	{
		readbytes = fs_read(&geckofile, image_buffer, DFU_XFER_SIZE_2K);
		if (readbytes < 0) {
			printf("Could not read file /tmo/zephyr.bin\n");
			return -1;
		}

		totalreadbytes += readbytes;
		/* Compute the SHA1 for this image while we get the size */
		mbedtls_sha1_update(&gecko_sha1_ctx, (unsigned char *)image_buffer, readbytes);

		if (readbytes == 0) {
			notdone = 0;
		}
	}

	mbedtls_sha1_finish(&gecko_sha1_ctx, gecko_sha1_output);
	printf("GECKO zephyr image size = %d\n", (uint32_t)totalreadbytes);

	printf("\tComputed File SHA1:\n\t\t");
	for (int i = 0; i < DFU_SHA1_LEN; i++) {
		printf("%02x ", gecko_sha1_output[i]);
	}

	return totalreadbytes;
}

/* Convert SHA1 ASCII hex to binary */
static void sha_hex_to_bin(char *sha_hex_in, char *sha_bin_out, int len)
{
	size_t i;
	char sha_ascii;
	unsigned char tempByte ;
	for (i = 0; i < len ; i++) {
		sha_ascii = *sha_hex_in;
		if (sha_ascii >= 97) {
			tempByte = sha_ascii - 97 + 10;
		} else if (sha_ascii >= 65) {
			tempByte = sha_ascii - 65 + 10;
		} else {
			tempByte = sha_ascii - 48;
		}
		/* In this ascii to binary encode, loop implementation
		 * the even SHA1 ascii characters are processed in the first pass,
		 * and the odd SHA1 characters are processed in the second pass
		 * of the current output byte
		 */
		if (i%2 == 0) {
			sha_bin_out[i/2] = tempByte << 4;
		} else {
			sha_bin_out[i/2] |= tempByte;
		}
		sha_hex_in++;
	}
}

// This function gets the sha1 of the Gecko zephyr firmware
static int get_gecko_sha1(void)
{
	readbytes = fs_read(&gecko_sha1_file, gecko_expected_sha1, DFU_SHA1_LEN*2);
	if ((readbytes < 0) || (readbytes != DFU_SHA1_LEN*2)) {
		printf("Could not read file /tmo/zephyr.bin.sha1\n");
		return -1;
	}

	sha_hex_to_bin(gecko_expected_sha1, gecko_expected_sha1_final, DFU_SHA1_LEN*2);
	return 0;
}

static int erase_image(uint32_t start_sector)
{
        if (flash_erase(gecko_flash_dev, start_sector, DFU_XFER_SIZE_2K) != 0) {
                printf("\nGecko 2K page erase failed\n");
		return -1;
        }
        return 0;
}

static int write_image_chunk_to_flash(int imageBytes, uint8_t* writedata, uint32_t startSector, int pageReset)
{
	int ret = 0;
	static uint32_t page = 0;

	if (pageReset) {
		page = 0;
		return 0;
	}

	uint32_t page_addr = startSector + (page * DFU_XFER_SIZE_2K);

	page++;

	// printf("\n1. readbytes %d page_addr %x\n", imageBytes, page_addr);
	if (flash_erase(gecko_flash_dev, page_addr, DFU_XFER_SIZE_2K) != 0) {
		printf("\nGecko 2K page erase failed\n");
	}

	/* This will also zero pad out the last 2K page write with the image remainder bytes. */
	if (flash_write(gecko_flash_dev, page_addr, writedata, DFU_XFER_SIZE_2K) != 0) {
		printf("Gecko flash write internal ERROR!");
		return -EIO;
	}

	flash_read(gecko_flash_dev, page_addr, check_buf, imageBytes);
	if (memcmp(writedata, check_buf, imageBytes) != 0) {
		printf("\nGecko flash erase-write-read ERROR!\n");
		return -EIO;
	}

	totalwritebytes += imageBytes;
	// printf("2. write flash addr %x total %d\n", page_addr, totalwritebytes);
	return ret;
}

static int file_read_flash(uint32_t offset)
{
	readbytes = fs_read(&geckofile, image_buffer, DFU_XFER_SIZE_2K);
	if (readbytes < 0) {
		printf("Could not read file /tmo/zephyr.slotx.bin\n");
		status = -1;
		return -1;
	}

	totalreadbytes += readbytes;
	//printf("\nreadbytes %d totalreadbytes %d\n", readbytes, totalreadbytes);

	if (readbytes > 0) {
		if (requested_slot_to_upgrade == 0) {
			write_image_chunk_to_flash(readbytes, image_buffer, GECKO_IMAGE_SLOT_0_SECTOR, GECKO_INCRE_PAGE);
		}
		else {
			write_image_chunk_to_flash(readbytes, image_buffer, GECKO_IMAGE_SLOT_1_SECTOR, GECKO_INCRE_PAGE);
		}

		crc32 = crc32_ieee_update(crc32, image_buffer, readbytes);
	}
	status = 0;
	return 0;
}

static uint8_t fw_upgrade_done = 0;
int32_t dfu_gecko_write_image(int slot_to_upgrade, char *bin_file, char *sha_file)
{
	char requested_binary_file[DFU_FILE_LEN];
	char requested_sha_file[DFU_FILE_LEN];
	requested_slot_to_upgrade = slot_to_upgrade;
	
	strcpy(requested_binary_file, bin_file);
	strcpy(requested_sha_file, sha_file);

	printf("Checking for presence of correct Slot %d image file\n", slot_to_upgrade);
	if (slot_to_upgrade == 0) {
		if (fs_open(&geckofile, requested_binary_file, FS_O_READ) != 0) {
			printf("The Gecko FW file %s is missing\n", requested_binary_file);
			return 1;
		}
		else {
			printf("The required Gecko FW file %s is present\n", requested_binary_file);
		}

		if (fs_open(&gecko_sha1_file, requested_sha_file, FS_O_READ) != 0) {
			printf("The SHA1 digest file %s is missing\n",requested_sha_file);
			return 1;
		}
		else {
			printf("The required SHA1 file %s is present\n", requested_sha_file);
		}
	} else if (slot_to_upgrade == 1) {
		if (fs_open(&geckofile, requested_binary_file, FS_O_READ) != 0) {
			printf("The file %s is missing\n", requested_binary_file);
			return 1;
		}
		else {
			printf("The required Gecko FW file %s is present\n", requested_binary_file);
		}

		if (fs_open(&gecko_sha1_file, requested_sha_file, FS_O_READ) != 0) {
			printf("The Gecko FW file %s is missing\n", requested_sha_file);
			return 1;
		}
		else {
			printf("The required SHA1 file %s is present\n", requested_sha_file);
		}
	} else {
		printf("Incorrect slot provided\n");
		return -1;
	}

	/* We do a dummy call here to init (reset) the incrementing page address var */
	write_image_chunk_to_flash(readbytes, image_buffer, GECKO_FLASH_SECTOR, GECKO_INIT_PAGE);

	readbytes = 0;
	totalreadbytes = 0;
	totalwritebytes = 0;

	while (!fw_upgrade_done) {
		switch (gecko_app_cb.state) {
			case GECKO_INITIAL_STATE:
				{
					printf("GECKO FW update started\n");
					/* update wlan application state */
					gecko_app_cb.state = GECKO_FW_UPGRADE;
					mbedtls_sha1_init(&gecko_sha1_ctx);
					memset(gecko_sha1_output, 0, sizeof(gecko_sha1_output));
					mbedtls_sha1_starts(&gecko_sha1_ctx);
				}
				/* no break */

			case GECKO_FW_UPGRADE:
				{
					/* Send the first chunk to extract header */
					fw_image_size = get_gecko_fw_size();
					if ((fw_image_size == 0) || (fw_image_size < DFU_CHUNK_SIZE)) {
						printf("ERROR: GECKO FW is too small\n");
						return -1;
					}

					int sha1_exist = get_gecko_sha1();
					if (sha1_exist != 0) {
						printf("ERROR: GECKO SHA1 is missing!\n");
						return -1;
					}

					int sha1_is_good = compare_sha1(slot_to_upgrade);
					if (sha1_is_good != 0) {
						printf("ERROR: GECKO SHA1 is miscompares!\n");
						return -1;
					}

					/* Calculate the total number of chunks */
					chunk_check = (fw_image_size / DFU_CHUNK_SIZE);
					if (fw_image_size % DFU_CHUNK_SIZE) {
						chunk_check += 1;
					}

					printf("image size: %d, 2048 byte chunks: %d\n", fw_image_size, chunk_check);
					fs_seek(&geckofile, 0, FS_SEEK_SET);

					readbytes = 0;
					totalreadbytes = 0;
					totalwritebytes = 0;

					/* Loop until all the chunks are read and written */
					while (offset <= fw_image_size) {
						if (chunk_cnt != 0) {
							if (file_read_flash(offset) != 0) {
								printf("file system flash read failed\n");
								return (-1);
							}
							//printf("chunk_cnt: %d\n", chunk_cnt);
						}
						if (chunk_cnt == 0) {
							printf("GECKO FW update - starts here with - 1st Chunk\n");
							if (status != 0) {
								printf("1st Chunk GECKO_ERROR: %d\n", status);
								return (-1);
							}
						} else if (chunk_cnt == (chunk_check -1)) {
							printf("Writing last chunk\n");
							if (file_read_flash(offset) != 0) {
								printf("file system flash read failed\n");
								return (-1);
							}
							if (status != 0) {
								printf("Last Chunk GECKO_ERROR: %d\n", status);
								break;
							}
							printf("GECKO FW update success\n");
							gecko_app_cb.state = GECKO_FW_UPGRADE_DONE;
							break;
						} else   {
							printk(".");
							//printf("Gecko FW continuing with in-between Chunks\n");
							if (status != 0) {
								printf("in-between Chunks GECKO_ERROR: %d\n", status);
								break;
							}
						}
						offset += readbytes;
						memset(image_buffer, 0, sizeof(image_buffer));
						chunk_cnt++;
					}       /* end While Loop */
				}               /* End case of  */
				break;

			case GECKO_FW_UPGRADE_DONE:
				{
					fw_upgrade_done = 1;
					fs_close(&geckofile);

					printf("\tCalculated program CRC32 is %x\n", crc32);
					printf("\tTotal bytes read       = %d bytes\n", totalreadbytes);
					printf("GECKO FW update has completed, rebooting now\n");
					k_sleep(K_SECONDS(3));
					sys_reboot(SYS_REBOOT_COLD);
				}
				break;

			default:
				printf("\nerror: dfu_gecko_write_image: default case\n");
				break;
		} /* end of switch */
	}
	return status;
} /* end of routine */

int is_bootloader_running(void)
{
#ifdef BOOT_SLOT
	return true;
#else
	return false;
#endif
}

int erase_image_slot(int slot)
{
        int flash_sector = 0;

	if (slot == 0) {
                flash_sector = GECKO_IMAGE_SLOT_0_SECTOR;
        }
	else if (slot == 1) {
                flash_sector = GECKO_IMAGE_SLOT_1_SECTOR;
        } else {
		printf("error: invalid slot %d\n", slot);
		return -1;
	}
	return erase_image(flash_sector);
}

int dfu_mcu_firmware_upgrade(int slot_to_upgrade, char *bin_file, char *sha_file)
{
	int ret = 0;

	printf("*** Performing the Pearl Gecko FW update ***\n");
	ret = dfu_gecko_write_image(slot_to_upgrade, bin_file, sha_file);
	return ret;
}

/* Convert the desired type to system endianness and icnrement the buffer. This is just a wrapper to
 * avoid writing the following a ton of times: sys_le32_to_cpu(*((uint32_t*)var));
 * var=((uint8_t*)var)+sizeof(uint32_t);
 */
#define POP(var, sz)                                                                               \
	sys_le##sz##_to_cpu(*((uint##sz##_t *)var));                                               \
	var = ((uint8_t *)var) + sizeof(uint##sz##_t);

#ifdef BOOT_SLOT
int get_current_slot()
{
	return strtol(BOOT_SLOT, NULL, 10);
}

int get_unused_slot()
{
	return strtol(BOOT_SLOT, NULL, 10) ? 0 : 1;
}

bool slot_is_safe_to_erase(int slot)
{
	if (slot != 0 && slot != 1) {
		return false;
	}

	if (slot == strtol(BOOT_SLOT, NULL, 10)) {
		return false;
	}

	return true;
}
#endif
#endif
