/*
 * Copyright (c) 2025 Endress+Hauser AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ff.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/pm/device_runtime.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define AUTOMOUNT_NODE DT_NODELABEL(ffs1)
FS_FSTAB_DECLARE_ENTRY(AUTOMOUNT_NODE);

#define FILE_DIR_PATH "/FLASH:/"
#define FILE_COUNT 4

static const struct device* flash_dev = DEVICE_DT_GET(DT_ALIAS(test_flash));

#define MAX_DATA_LEN 2048

static uint8_t data[MAX_DATA_LEN];

static void fill_data_buffer(void) {
	for (int i = 0; i < MAX_DATA_LEN; i++) {
		data[i] = (uint8_t)i;
	}
}

static void format_fs(void) {
	struct fs_mount_t *mp = &FS_FSTAB_ENTRY(AUTOMOUNT_NODE);
	int rc;

	/* First, unmount if already mounted */
	rc = fs_unmount(mp);
		if (rc != 0) {
		LOG_WRN("Unmount failed (rc=%d), maybe not mounted yet", rc);
	}

	/* Format (create new FAT filesystem) */
	rc = fs_mkfs(FS_FATFS, (uintptr_t)"FLASH:", NULL, 0);
	if (rc != 0) {
		LOG_ERR("Failed to format filesystem (rc=%d)", rc);
		return;
	}
	LOG_INF("Filesystem formatted successfully");

	/* Now mount again */
	rc = fs_mount(mp);
	if (rc != 0) {
		LOG_ERR("Failed to mount after format (rc=%d)", rc);
		return;
	}
	LOG_INF("Filesystem mounted at %s", mp->mnt_point);
}

static void generate_file_name(char* file_name, uint32_t file_size, int i) {
	sprintf(file_name, "/FLASH:/%d_%d", file_size, i);
}

static uint32_t benchmark_file_write(uint32_t file_size) {
	int ret;
	uint32_t cycles = 0;

	for (int i = 0; i < FILE_COUNT; i++) {
		// generate a name for the file based on the size on the index, e.g. 32_0, 32_1...
		char file_name[32];
		generate_file_name(file_name, file_size, i);

		struct fs_file_t file;
		fs_file_t_init(&file);

		ret = fs_open(&file, file_name, FS_O_CREATE | FS_O_WRITE);
		if (ret != 0) {
			LOG_ERR("Accessing filesystem failed");
			return 0;
		}

		uint32_t start = k_cycle_get_32();

		ret = fs_write(&file, data, file_size);
		// fs_sync(&file);
		uint32_t end = k_cycle_get_32();
		cycles += end - start;

		if (ret != file_size) {
			LOG_ERR("Writing filesystem failed");
			return 0;
		}

		ret = fs_close(&file);
		if (ret != 0) {
			LOG_ERR("Closing file failed");
			return 0;
		}
	}

	uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
	return (uint32_t)(us / FILE_COUNT);
}

static uint32_t benchmark_file_read(uint32_t file_size) {
	int ret;
	uint32_t cycles = 0;

	for (int i = 0; i < FILE_COUNT; i++) {
		// regenerate the filename based on size on index, e.g. 32_0, 32_1...
		char file_name[32];
		generate_file_name(file_name, file_size, i);

		struct fs_file_t file;
		fs_file_t_init(&file);

		ret = fs_open(&file, file_name, FS_O_READ);
		if (ret != 0) {
			LOG_ERR("Accessing filesystem failed (%s)", file_name);
			return 0;
		}

		uint32_t start = k_cycle_get_32();

		ret = fs_read(&file, data, file_size);

		uint32_t end = k_cycle_get_32();
		cycles += end - start;

		if (ret != file_size) {
			LOG_ERR("Reading filesystem failed");
			return 0;
		}

		ret = fs_close(&file);
		if (ret != 0) {
			LOG_ERR("Closing file failed");
			return 0;
		}
	}

	uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
	return (uint32_t)(us / FILE_COUNT);
}

int main(void)
{
	pm_device_runtime_get(flash_dev);

	format_fs();
	fill_data_buffer();

	uint32_t file_write_512_us  = benchmark_file_write(512);
	uint32_t file_write_1024_us = benchmark_file_write(1024);
	uint32_t file_write_2048_us = benchmark_file_write(2048);
	uint32_t file_write_32_us   = benchmark_file_write(32);
	uint32_t file_write_64_us   = benchmark_file_write(64);
	uint32_t file_write_128_us  = benchmark_file_write(128);
	uint32_t file_write_256_us  = benchmark_file_write(256);

	uint32_t file_read_512_us  = benchmark_file_read(512);
	uint32_t file_read_1024_us = benchmark_file_read(1024);
	uint32_t file_read_2048_us = benchmark_file_read(2048);
	uint32_t file_read_32_us   = benchmark_file_read(32);
	uint32_t file_read_64_us   = benchmark_file_read(64);
	uint32_t file_read_128_us  = benchmark_file_read(128);
	uint32_t file_read_256_us  = benchmark_file_read(256);

	printk("32,%u,%u\n", file_write_32_us, file_read_32_us);
	printk("64,%u,%u\n", file_write_64_us, file_read_64_us);
	printk("128,%u,%u\n", file_write_128_us, file_read_128_us);
	printk("256,%u,%u\n", file_write_256_us, file_read_256_us);
	printk("512,%u,%u\n", file_write_512_us, file_read_512_us);
	printk("1024,%u,%u\n", file_write_1024_us, file_read_1024_us);
	printk("2048,%u,%u\n", file_write_2048_us, file_read_2048_us);

	return 0;
}
