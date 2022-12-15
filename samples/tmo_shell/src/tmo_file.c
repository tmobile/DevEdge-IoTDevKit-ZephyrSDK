/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/shell/shell.h>
#include <mbedtls/sha1.h>
#include <zephyr/sys/crc.h>

#include "tmo_file.h"
#include "dfu_murata_1sc.h"

#define READ_SIZE 4096
extern uint8_t mxfer_buf[];
#define SHA_DIGEST_20   20

int tmo_cp(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 0;
	struct fs_dirent entry_src;

	if (argc != 3) {
		shell_error(shell, "usage: tmo file cp <source file> <destination file>");
		return -EINVAL;
	}

	char *src = argv[1];
	char *dst = argv[2];

	shell_print(shell, "tmo_cp: %s %s", src, dst);
	if (strcmp(src, dst) == 0) {
		shell_error(shell, "cannot copy file to itself");
		return -EINVAL;
	}

	ret = fs_stat(src, &entry_src);
	if (ret) {
		shell_error(shell, "cannot stat %s", src);
		return -EINVAL;
	}

	if (entry_src.type != FS_DIR_ENTRY_FILE) {
		shell_error(shell, "%s is not a file", src);
		return -EINVAL;
	}

	struct fs_file_t zfp_src;
	fs_file_t_init(&zfp_src);
	ret = fs_open(&zfp_src, src, FS_O_READ);
	if (ret) {
		shell_error(shell, "cannot open %s", src);
		return ret;
	}

	struct fs_file_t zfp_dst;
	fs_file_t_init(&zfp_dst);
	ret = fs_open(&zfp_dst, dst, FS_O_CREATE | FS_O_WRITE);
	if (ret) {
		shell_error(shell, "cannot open %s", dst);
		fs_close(&zfp_src);
		return ret;
	}

	while (1) {
		ret = fs_read(&zfp_src, mxfer_buf, READ_SIZE);
		if (ret < 0) {
			shell_error(shell, "error reading from %s", src);
			goto end;
		}

		if (ret == 0) {
			goto end;
		}

		int size = ret;
		ret = fs_write(&zfp_dst, mxfer_buf, size);
		if (ret < 0) {
			shell_error(shell, "error writing to %s", dst);
			goto end;
		}

		if (ret != size) {
			shell_error(shell, "size mismatch");
			ret = -ENOSPC;
			goto end;
		}
	}
end:
	fs_close(&zfp_src);
	fs_close(&zfp_dst);
	return ret;
}

int tmo_ll(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "usage: tmo file ll <folder>");
		return -EINVAL;
	}

	struct fs_dir_t zdp;
	fs_dir_t_init(&zdp);

	for (int i=1;i<argc;i++) {
		size_t total = 0;
		shell_print(shell, "Contents of %s:", argv[i]);
		if (fs_opendir(&zdp, argv[i]) == 0) {
			struct fs_dirent entry;

			// Find longest name
			int longest = 0;
			while (fs_readdir(&zdp, &entry) == 0 && entry.name[0] != 0) {
				longest = MAX(longest, strlen(entry.name));
			}
			fs_closedir(&zdp);
			fs_opendir(&zdp, argv[i]);
			while (fs_readdir(&zdp, &entry) == 0 && entry.name[0] != 0) {
				char spec[16];
				if (entry.type == FS_DIR_ENTRY_FILE) {
					snprintf(spec, sizeof(spec), "  %%-%ds %%d\n", longest);
					printf(spec, entry.name, entry.size);
					total += entry.size;
				} else if (entry.type == FS_DIR_ENTRY_DIR) {
					snprintf(spec, sizeof(spec), "  %%-%ds <dir>\n", longest);
					printf(spec, entry.name);
				}
			}
			fs_closedir(&zdp);
		}
		shell_print(shell, "Total: %u", total);
	}
	return 0;
}

int tmo_mv(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 0;

	if (argc < 3) {
		shell_error(shell, "usage: tmo file mv <source file> <destination file>");
		return -EINVAL;
	}

	char *src = argv[1];
	char *dst = argv[2];
	shell_print(shell, "Renaming %s to %s", src, dst);
	ret = fs_rename(src, dst);
	if (ret) {
		shell_error(shell, "fs_rename returned %d\n", ret);
	}
	return ret;
}

int cmd_sha1(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Missing required arguments");
		shell_print(shell, "Usage: tmo sha1 <filename>\n");
		return -EINVAL;
	}

	char *filename = argv[1];

	struct fs_file_t sha1file = {0};
	int readbytes = 0;
	int totalreadbytes = 0;

	mbedtls_sha1_context tmo_sha1_ctx;
	unsigned char tmo_sha1_output[SHA_DIGEST_20];

	uint32_t crc32 = 0;
	uint32_t mcrc32 = 0;

	mbedtls_sha1_init(&tmo_sha1_ctx);
	memset(tmo_sha1_output, 0, sizeof(tmo_sha1_output));
	mbedtls_sha1_starts(&tmo_sha1_ctx);

	readbytes = 0;
	totalreadbytes = 0;

	if (fs_open(&sha1file, filename, FS_O_READ) != 0) {
		shell_error(shell, "%s is missing", filename);
		return -EINVAL;
	}

	fs_seek(&sha1file, 0, FS_SEEK_SET);

	int notdone = 1;
	while (notdone)
	{
		readbytes = fs_read(&sha1file, mxfer_buf, UA_HEADER_SIZE);
		if (readbytes < 0) {
			shell_error(shell, "Could not read file %s", filename);
			fs_close(&sha1file);
			return -1;
		}
		if ((totalreadbytes == 0) && (readbytes != UA_HEADER_SIZE)) {
			shell_error(shell, "Error reading header, read %d bytes\n", readbytes);
			fs_close(&sha1file);
			return -1;
		}

		totalreadbytes += readbytes;
		if (readbytes == 0) {
			notdone = 0;
		}
		else {
			mbedtls_sha1_update(&tmo_sha1_ctx, (unsigned char *)mxfer_buf, readbytes);
			crc32 = crc32_ieee_update(crc32, mxfer_buf, readbytes);
			if (totalreadbytes > UA_HEADER_SIZE)
				mcrc32 = murata_1sc_crc32_update(mcrc32, mxfer_buf, readbytes);
		}
	}

	mbedtls_sha1_finish(&tmo_sha1_ctx, tmo_sha1_output);
	mcrc32 = murata_1sc_crc32_finish(mcrc32, totalreadbytes - UA_HEADER_SIZE);

	shell_print(shell, "  Size: %d bytes", (uint32_t) totalreadbytes);
	shell_print(shell, " CRC32: %x", crc32);
	shell_print(shell, "MCRC32: %x", mcrc32);

	printf("  SHA1:");
	for (int i = 0; i < SHA_DIGEST_20; i++) {
		printf(" %02x", tmo_sha1_output[i]);
	}
	printf("\n");

	fs_close(&sha1file);
	return 0;
}
