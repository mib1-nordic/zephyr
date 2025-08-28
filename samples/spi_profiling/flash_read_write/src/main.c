#include <zephyr/pm/device_runtime.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define TEST_START_ADDRESS  0x10000
#define TEST_SECTOR_LENGTH  0x1000
#define TEST_SECTOR_COUNT   32

#define TEST_SMALL_READ_WRITE_SECTOR_COUNT 96

// const struct device *spi_bus = DEVICE_DT_GET(DT_NODELABEL(dut_spi));
const struct device *flash_dev = DEVICE_DT_GET(DT_ALIAS(test_flash));

static uint8_t test_write_data_buffer[TEST_SECTOR_LENGTH];
static uint8_t test_read_data_buffer[TEST_SECTOR_LENGTH];

static void fill_test_write_data_buffer(void) {
	for(int i = 0; i < TEST_SECTOR_LENGTH; i++) {
		test_write_data_buffer[i] = (uint8_t)i;
	}
}

static uint32_t benchmark_sector_erase_write(void) {
	int ret;
	uint32_t start = k_cycle_get_32();

	for (int i = 0; i < TEST_SECTOR_COUNT; i++) {
		uint32_t sector_address = TEST_START_ADDRESS + i * TEST_SECTOR_LENGTH;

		ret = flash_erase(flash_dev, sector_address, TEST_SECTOR_LENGTH);
		if (ret) {
			break;
		}

		ret = flash_write(flash_dev, sector_address, test_write_data_buffer, TEST_SECTOR_LENGTH);
		if (ret) {
			break;
		}
	}

	uint32_t end = k_cycle_get_32();

	if (ret) {
		printk("benchmark_sector_erase_write failed! %d\n", ret);
		return 0;
	}

	uint32_t cycles = end - start;
	uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
	return (uint32_t)(us / TEST_SECTOR_COUNT);
}

static uint32_t benchmark_sector_read(void) {
	int ret;
	uint32_t start = k_cycle_get_32();

	for (int i = 0; i < TEST_SECTOR_COUNT; i++) {
		uint32_t sector_address = TEST_START_ADDRESS + i * TEST_SECTOR_LENGTH;

		ret = flash_read(flash_dev, sector_address, test_read_data_buffer, TEST_SECTOR_LENGTH);
		if (ret) {
			break;
		}
	}

	uint32_t end = k_cycle_get_32();

	if (ret) {
		printk("benchmark_sector_read failed! %d\n", ret);
		return 0;
	}

	uint32_t cycles = end - start;
	uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
	return (uint32_t)(us / TEST_SECTOR_COUNT);
}

// writing without erasing - testing just write performance
static uint32_t benchmark_short_write(uint32_t offset, uint32_t length) {
	int ret;
	uint32_t start = k_cycle_get_32();

	for (int i = 0; i < TEST_SMALL_READ_WRITE_SECTOR_COUNT; i++) {
		uint32_t sector_address = TEST_START_ADDRESS + offset + i * TEST_SECTOR_LENGTH;

		ret = flash_write(flash_dev, sector_address, test_write_data_buffer, length);
		if (ret) {
			break;
		}
	}

	uint32_t end = k_cycle_get_32();

	if (ret) {
		printk("benchmark_short_write failed! %d\n", ret);
		return 0;
	}

	uint32_t cycles = end - start;
	uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
	return (uint32_t)(us / TEST_SMALL_READ_WRITE_SECTOR_COUNT);
}

static uint32_t benchmark_short_read(uint32_t offset, uint32_t length) {
	int ret;
	uint32_t start = k_cycle_get_32();

	for (int i = 0; i < TEST_SMALL_READ_WRITE_SECTOR_COUNT; i++) {
		uint32_t sector_address = TEST_START_ADDRESS + offset + i * TEST_SECTOR_LENGTH;

		ret = flash_read(flash_dev, sector_address, test_read_data_buffer, length);
		if (ret) {
			break;
		}
	}

	uint32_t end = k_cycle_get_32();

	if (ret) {
		printk("benchmark_read failed! %d\n", ret);
		return 0;
	}

	uint32_t cycles = end - start;
	uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
	return (uint32_t)(us / TEST_SMALL_READ_WRITE_SECTOR_COUNT);
}

int main(void)
{
	// (void)pm_device_runtime_get(flash_dev);

	fill_test_write_data_buffer();

	uint32_t sector_erase_write_us = benchmark_sector_erase_write();
	uint32_t sector_read_us = benchmark_sector_read();

	uint32_t short_write_8_us    = benchmark_short_write(0, 8);
	uint32_t short_write_64_us   = benchmark_short_write(8, 64);
	uint32_t short_write_256_us  = benchmark_short_write(128, 256);
	uint32_t short_write_1024_us = benchmark_short_write(1024, 1024);

	uint32_t short_read_8_us    = benchmark_short_read(0, 8);
	uint32_t short_read_64_us   = benchmark_short_read(8, 64);
	uint32_t short_read_256_us  = benchmark_short_read(128, 256);
	uint32_t short_read_1024_us = benchmark_short_read(1024, 1024);

	printk("Average sector erase and write time: %d us\n", sector_erase_write_us);
	printk("Average sector erase and write time: %d us\n", sector_read_us);

	printk("Short read/write times (length, write time [us], read time [us])\n");
	printk("8,%d,%d\n",    short_write_8_us,    short_read_8_us);
	printk("64,%d,%d\n",   short_write_64_us,   short_read_64_us);
	printk("256,%d,%d\n",  short_write_256_us,  short_read_256_us);
	printk("1024,%d,%d\n", short_write_1024_us, short_read_1024_us);

	// (void)pm_device_runtime_put(flash_dev);

	return 0;
}
