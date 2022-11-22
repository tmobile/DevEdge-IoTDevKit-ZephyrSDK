/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Device Firmware Update (DFU) support for SiLabs Pearl Gecko
 */

/* Only run if syncup wants DFU or if application is not in syncup at all. */
#if !defined(CONFIG_SYNCUP_SDK) || defined(CONFIG_SYNCUP_DFU)

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/fs/fs.h>
#include <mbedtls/sha1.h>
#include <zephyr/sys/byteorder.h>

#include "tmo_dfu_download.h"
#include "dfu_gecko.h"

extern const struct device *gecko_flash_dev;

/* Syncup defines some file system operations, copy them */
#ifndef CONFIG_SYNCUP_SDK
static uint8_t mxfer_buf[CONFIG_SYNCUP_FS_READ_BUF_SIZE];

int syncup_fs_stat(const char *file)
{
	struct fs_dirent entry;
	int err;

	err = fs_stat(file, &entry);
	if (err) {
		LOG_ERROR("Could not stat %s, err %d" ENDL, file, err);

		return -1;
	}

	return (entry.type == FS_DIR_ENTRY_FILE) ? entry.size : 0;
}

int syncup_fs_get_file_sha1(const char *file, uint8_t *out, uint8_t out_len)
{
	mbedtls_sha1_context gecko_sha1_ctx = {0};
	int ret;

	if (out_len < 20) {
		ret = -1;
		goto end_nofile;
	}

	/* Init the sha1 checksum */
	mbedtls_sha1_init(&gecko_sha1_ctx);
	memset(out, 0, 20);
	mbedtls_sha1_starts(&gecko_sha1_ctx);

	struct fs_file_t zfp_src;
	fs_file_t_init(&zfp_src);
	ret = fs_open(&zfp_src, file, FS_O_READ);
	if (ret) {
		LOG_ERROR("cannot open %s" ENDL, file);
		goto end_nofile;
	}

	while (1) {
		ret = fs_read(&zfp_src, mxfer_buf, CONFIG_SYNCUP_FS_READ_BUF_SIZE);
		if (ret < 0) {
			LOG_ERROR("Error reading from %s" ENDL, file);
			goto end;
		} else if (ret) {
			mbedtls_sha1_update(&gecko_sha1_ctx, (unsigned char *)mxfer_buf, ret);
		} else {
			mbedtls_sha1_finish(&gecko_sha1_ctx, out);
			break;
		}
	}
	ret = 0;
end:
	fs_close(&zfp_src);
end_nofile:
	return ret;
}
#else
/* Syncup internal logging */
#include "logging.h"
LOGGER_MODULE(SYNCUP_DFU_API_GECKO);
#endif

/* Needed by tmo_dfu_download.c */
const struct dfu_file_t dfu_files_mcu[] = {
	{"Gecko MCU 1/4",
	 "/tmo/zephyr.slot0.bin",
	 "tmo_shell.tmo_dev_edge.slot0.bin",
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{"Gecko MCU 2/4",
	 "/tmo/zephyr.slot1.bin",
	 "tmo_shell.tmo_dev_edge.slot1.bin",
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{"Gecko MCU 3/4",
	 "/tmo/zephyr.slot0.bin.sha1",
	 "tmo_shell.tmo_dev_edge.slot0.bin.sha1",
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{"Gecko MCU 4/4",
	 "/tmo/zephyr.slot1.bin.sha1",
	 "tmo_shell.tmo_dev_edge.slot1.bin.sha1",
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},

	{"", "", "", ""}};

static char *slot_file_name[2][2] = {{CONFIG_SYNCUP_DFU_SLOT0_FILE, CONFIG_SYNCUP_DFU_SLOT0_SHA1},
				     {CONFIG_SYNCUP_DFU_SLOT1_FILE, CONFIG_SYNCUP_DFU_SLOT1_SHA1}};

static uint8_t image_buffer[CONFIG_SYNCUP_DFU_CHUNK_SIZE] = {0};
static uint8_t check_buf[CONFIG_SYNCUP_DFU_CHUNK_SIZE + 1];
static unsigned char sha1_output[SHA1_LEN];

/**
 * @brief Compare 2 slot versions from header info
 *
 * @param slot0_ver The first version to compare
 * @param slot1_ver The second version to compare
 * @return int 0 if slot0_ver is newer or versions are equal, 1 if slot1_ver is newer.
 */
static int slot_version_cmp(struct image_version *slot0_ver, struct image_version *slot1_ver)
{
	/* Compare major version numebrs */
	if (slot0_ver->iv_major > slot1_ver->iv_major) {
		return 0;
	}

	if (slot0_ver->iv_major < slot1_ver->iv_major) {
		return 1;
	}

	/* The major version numbers are equal, compare minor */
	if (slot0_ver->iv_minor > slot1_ver->iv_minor) {
		return 0;
	}

	if (slot0_ver->iv_minor < slot1_ver->iv_minor) {
		return 1;
	}

	/* The minor version numbers are equal, compare revision */
	if (slot0_ver->iv_revision > slot1_ver->iv_revision) {
		return 0;
	}

	if (slot0_ver->iv_revision < slot1_ver->iv_revision) {
		return 1;
	}

	/* The two are equal */
	return 0;
}

/* Convert the desired type to system endianness and icnrement the buffer. This is just a wrapper to
 * avoid writing the following a ton of times: sys_le32_to_cpu(*((uint32_t*)var));
 * var=((uint8_t*)var)+sizeof(uint32_t);
 */
#define POP(var, sz)                                                                               \
	sys_le##sz##_to_cpu(*((uint##sz##_t *)var));                                               \
	var = ((uint8_t *)var) + sizeof(uint##sz##_t);

/**
 * @brief Deserialize the magic header once read from flash
 *
 * @param dst The dest structure to write into
 * @param read_buf The buffer read from flash
 */
static void deserialize_magic_hdr(struct image_header *dst, uint8_t *read_buf)
{
	if (!dst || !read_buf) {
		return;
	}

	dst->ih_magic = POP(read_buf, 32);
	dst->ih_load_addr = POP(read_buf, 32);
	dst->ih_hdr_size = POP(read_buf, 16);
	dst->ih_protect_tlv_size = POP(read_buf, 16);
	dst->ih_img_size = POP(read_buf, 32);
	dst->ih_flags = POP(read_buf, 32);

	dst->ih_ver.iv_major = read_buf[0];
	read_buf++;
	dst->ih_ver.iv_minor = read_buf[0];
	read_buf++;
	dst->ih_ver.iv_revision = POP(read_buf, 16);
	dst->ih_ver.iv_build_num = POP(read_buf, 32);
}

#undef POP

/**
 * @brief Get the oldest slot number, invalid slots are always considered the oldest, choses 0 if a
 * tie
 *
 * @return int 0 or 1 if a determination could be made, -1 otherwise
 */
int get_oldest_slot()
{
	struct image_header slot0_hdr;
	struct image_header slot1_hdr;
	uint8_t read_buf[CONFIG_SYNCUP_DFU_IMAGE_HDR_LEN];
	bool slot0_has_image = false;
	bool slot1_has_image = false;
	int oldest_slot = 0;

	flash_read(gecko_flash_dev, CONFIG_SYNCUP_DFU_SLOT0_FLASH_ADDR, read_buf,
		   CONFIG_SYNCUP_DFU_IMAGE_HDR_LEN);
	deserialize_magic_hdr(&slot0_hdr, read_buf);

	flash_read(gecko_flash_dev, CONFIG_SYNCUP_DFU_SLOT1_FLASH_ADDR, read_buf,
		   CONFIG_SYNCUP_DFU_IMAGE_HDR_LEN);
	deserialize_magic_hdr(&slot1_hdr, read_buf);

	if (slot0_hdr.ih_magic == CONFIG_SYNCUP_DFU_IMAGE_MAGIC) {
		LOG_DEBUG("%s Slot 0 FW Version = %u.%u.%u+%u" ENDL, CONFIG_SYNCUP_MCU_NAME,
			  slot0_hdr.ih_ver.iv_major, slot0_hdr.ih_ver.iv_minor,
			  slot0_hdr.ih_ver.iv_revision, slot0_hdr.ih_ver.iv_build_num);
		slot0_has_image = true;
		oldest_slot = 1;
	} else {
		LOG_DEBUG("No bootable image/version found for %s slot 0\n",
			  CONFIG_SYNCUP_MCU_NAME);
	}

	if (slot1_hdr.ih_magic == CONFIG_SYNCUP_DFU_IMAGE_MAGIC) {
		LOG_DEBUG("%s Slot 1 FW Version = %u.%u.%u+%u" ENDL, CONFIG_SYNCUP_MCU_NAME,
			  slot1_hdr.ih_ver.iv_major, slot1_hdr.ih_ver.iv_minor,
			  slot1_hdr.ih_ver.iv_revision, slot1_hdr.ih_ver.iv_build_num);
		slot1_has_image = true;
		oldest_slot = 0;
	} else {
		LOG_DEBUG("No bootable image/version found for %s slot 1" ENDL,
			  CONFIG_SYNCUP_MCU_NAME);
	}

	if (slot0_has_image && slot1_has_image) {
		LOG_DEBUG("%s slot 0 and slot 1 contain a bootable active image" ENDL,
			  CONFIG_SYNCUP_MCU_NAME);
		oldest_slot = slot_version_cmp(&slot0_hdr.ih_ver, &slot1_hdr.ih_ver);
		if (oldest_slot < 0) {
			return -1;
		}
		/* The given function finds the *newest* version, flip that */
		oldest_slot = (oldest_slot == 1) ? 0 : 1;
	} else if (!slot0_has_image && !slot1_has_image) {
		/* This should never happen and usually means no bootloader or an invalid image is
		 * running. */
		LOG_ERROR("No valid %s slots found, defaulting to slot 0 (S0 magic: %zu, S1 magic: "
			  "%zu)" ENDL,
			  CONFIG_SYNCUP_MCU_NAME, slot0_hdr.ih_magic, slot1_hdr.ih_magic);
		/* TODO Return whichever slot is not being used. */
		oldest_slot = 0;
	}

	return oldest_slot;
}

/* Convert SHA1 ASCII hex to binary */
static void sha_hex_to_bin(char *sha_hex_in, char *sha_bin_out, int len)
{
	size_t i;
	char sha_ascii;
	unsigned char tempByte;

	for (i = 0; i < len; i++) {
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
		if (i % 2 == 0) {
			sha_bin_out[i / 2] = tempByte << 4;
		} else {
			sha_bin_out[i / 2] |= tempByte;
		}
		sha_hex_in++;
	}
}

// This function gets the sha1 of the Gecko zephyr firmware
/**
 * @brief Check a sha1 files contents against a SHA1 byte stream
 *
 * @param gecko_sha1_file The file that contains a sha1 sum (as ASCII text)
 * @param gecko_sha1_output The buffer to compare against
 * @return true If the hashes match
 * @return false If the hashes do not match or on error
 */
static bool check_fw_sha1(struct fs_file_t *gecko_sha1_file, uint8_t *gecko_sha1_output)
{
	bool sha1_fail = false;
	uint8_t gecko_expected_sha1[SHA1_LEN * 2];
	uint8_t gecko_expected_sha1_final[SHA1_LEN];
	int readbytes;

	readbytes = fs_read(gecko_sha1_file, gecko_expected_sha1, SHA1_LEN * 2);
	if ((readbytes < 0) || (readbytes != SHA1_LEN * 2)) {
		LOG_ERROR("Could not read sha1 file" ENDL);

		return false;
	}

	sha_hex_to_bin(gecko_expected_sha1, gecko_expected_sha1_final, SHA1_LEN * 2);

	for (int i = 0; i < SHA1_LEN; i++) {
		if (gecko_sha1_output[i] != gecko_expected_sha1_final[i]) {
			sha1_fail = true;
			break;
		}
	}

	return !sha1_fail;
}

/**
 * @brief Writes an image chunk to flash, keeping track of prior writes and writing to the next
 * unwritten address
 *
 * @param num_bytes How many bytes to write
 * @param buf The data buffer to write
 * @param slot_sector The starting sector
 * @param reset True to reset the write counter and exist (no write will be performed)
 * @return int Bytes written on success, -err on failure
 */
static int write_image_chunk_to_flash(int num_bytes, uint8_t *buf, uint32_t slot_sector, bool reset)
{
	static uint32_t page = 0;

	if (reset) {
		page = 0;

		return 0;
	}

	uint32_t page_addr = slot_sector + (page * CONFIG_SYNCUP_DFU_CHUNK_SIZE);

	page++;

	if (flash_erase(gecko_flash_dev, page_addr, CONFIG_SYNCUP_DFU_CHUNK_SIZE) != 0) {
		LOG_ERROR(ENDL "Gecko 2K page erase failed" ENDL);
	}

	/* This will also zero pad out the last 2K page write with the image remainder bytes. */
	if (flash_write(gecko_flash_dev, page_addr, buf, CONFIG_SYNCUP_DFU_CHUNK_SIZE) != 0) {
		LOG_ERROR(ENDL "Gecko flash write internal ERROR!" ENDL);

		return -EIO;
	}

	flash_read(gecko_flash_dev, page_addr, check_buf, num_bytes);
	if (memcmp(buf, check_buf, num_bytes) != 0) {
		LOG_ERROR(ENDL "Gecko flash erase-write-read ERROR!" ENDL);

		return -EIO;
	}

	return 0;
}

/**
 * @brief Writes a chunk of a file to flash
 *
 * @param geckofile A file handler to read from
 * @param slot The slot to write into in flash
 * @param len The maximum length of the transfer
 * @return int number of bytes written on success, -1 on failure. Should only return less than len
 * on the final call
 */
static int file_chunk_to_flash(struct fs_file_t *geckofile, int slot, size_t len)
{
	int readbytes = fs_read(geckofile, image_buffer, len);

	if (readbytes < 0) {
		return -1;
	}

	if (readbytes > 0) {
		if (slot == 0) {
			write_image_chunk_to_flash(readbytes, image_buffer,
						   CONFIG_SYNCUP_DFU_SLOT0_FLASH_ADDR, false);
		} else {
			write_image_chunk_to_flash(readbytes, image_buffer,
						   CONFIG_SYNCUP_DFU_SLOT1_FLASH_ADDR, false);
		}
	}

	return readbytes;
}

int dfu_gecko_write_image(int slot_to_upgrade)
{
	struct fs_file_t geckofile = {0};
	struct fs_file_t gecko_sha1_file = {0};
	int status = 0;
	int readbytes = 0;
	int totalreadbytes = 0;
	uint32_t chunk_cnt = 0, chunk_check = 0, fw_image_size = 0;

	/* Open the firmware and sha1 files */
	if (fs_open(&geckofile, slot_file_name[slot_to_upgrade][FILE_FW], FS_O_READ) != 0) {
		LOG_ERROR("The Gecko FW file %s is missing" ENDL,
			  slot_file_name[slot_to_upgrade][FILE_FW]);

		goto end;
	}

	if (fs_open(&gecko_sha1_file, slot_file_name[slot_to_upgrade][FILE_SHA1], FS_O_READ) != 0) {
		LOG_ERROR("The SHA1 digest file %s is missing" ENDL,
			  slot_file_name[slot_to_upgrade][FILE_SHA1]);

		goto end_close_fw;
	}

	/* Reset the writing process */
	write_image_chunk_to_flash(0, NULL, 0, true);

	/* STAGE 1: Set up */
	LOG_INFO("GECKO FW update started" ENDL);
	memset(sha1_output, 0, sizeof(sha1_output));

	/* STAGE 2: Verify files and write to flash */
	fw_image_size = syncup_fs_stat(slot_file_name[slot_to_upgrade][FILE_FW]);
	if ((fw_image_size == 0) || (fw_image_size < CONFIG_SYNCUP_DFU_CHUNK_SIZE)) {
		LOG_ERROR("ERROR  - GECKO FW is too small" ENDL);
		status = -1;
		goto end_closefiles;
	}

	/* Compute the sha1 hash of the slot file */
	status = syncup_fs_get_file_sha1(slot_file_name[slot_to_upgrade][FILE_FW], sha1_output,
					 sizeof(sha1_output));
	if (status) {
		LOG_ERROR("Could not get file sha1 for %s, err=%d" ENDL,
			  slot_file_name[slot_to_upgrade][FILE_FW], status);
	}

	/* Compare the computed sha1 to the sha1 file*/
	bool sha1_is_good = check_fw_sha1(&gecko_sha1_file, sha1_output);
	if (!sha1_is_good) {
		LOG_ERROR("Failed to match SHA1" ENDL);
		status = -1;
		goto end_closefiles;
	}

	/* Calculate the total number of chunks */
	chunk_check = (fw_image_size / CONFIG_SYNCUP_DFU_CHUNK_SIZE);
	if (fw_image_size % CONFIG_SYNCUP_DFU_CHUNK_SIZE) {
		chunk_check += 1;
	}

	LOG_INFO("image_size = %d, num of 2048 chunks = %d" ENDL, fw_image_size, chunk_check);
	/* TODO Implement a read FS API that is useful here and eliminate the need for any file
	 * pointer ops in this interface */
	fs_seek(&geckofile, 0, FS_SEEK_SET);

	/* Loop until all the chunks are read and written */
	for (int i = 0; i < chunk_check; i++) {
		/* Write the next chunk to flash */
		readbytes = file_chunk_to_flash(&geckofile, slot_to_upgrade,
						CONFIG_SYNCUP_DFU_CHUNK_SIZE);

		if (readbytes <= 0) {
			LOG_ERROR("Something failed to flash" ENDL);
			status = -1;
			goto end_closefiles;
		}

		/* A partial transfer is an error unless its the last chunk
		 * TODO Can this ever happen under normal conditions? If so handle the error without
		 * failing*/
		if (readbytes != CONFIG_SYNCUP_DFU_CHUNK_SIZE && i != (chunk_check - 1)) {
			LOG_ERROR("Incomplete chunk written to flash %d/%d" ENDL, readbytes,
				  CONFIG_SYNCUP_DFU_CHUNK_SIZE);
			status = -1;
			goto end_closefiles;
		}

		totalreadbytes += readbytes;
		memset(image_buffer, 0, sizeof(image_buffer));
		LOG_DEBUG("Chunk %d Wrote %d bytes (%d/%d)" ENDL, chunk_cnt, readbytes,
			  totalreadbytes, fw_image_size);
		chunk_cnt++;
	}

	/* STAGE 3: Clean up and reboot */
	fs_close(&geckofile);
	fs_close(&gecko_sha1_file);

	LOG_DEBUG("\ttotal bytes read       = %d bytes" ENDL, totalreadbytes);
	LOG_DEBUG("GECKO FW update has completed - rebooting now" ENDL);
	k_sleep(K_SECONDS(3));
	sys_reboot(SYS_REBOOT_COLD);

end_closefiles:
	fs_close(&gecko_sha1_file);
end_close_fw:
	fs_close(&geckofile);
end:

	return status;
}

int get_gecko_fw_version(void)
{
	return get_oldest_slot() == -1 ? -1 : 0;
}

int dfu_mcu_firmware_upgrade(int slot_to_upgrade)
{
	int ret = 0;
	printf("*** Performing the Pearl Gecko FW update ***\n");
	ret = dfu_gecko_write_image(slot_to_upgrade);
	return ret;
}
#endif /* CONFIG_SYNCUP_DFU */