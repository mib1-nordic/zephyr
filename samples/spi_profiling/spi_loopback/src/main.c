#include <zephyr/drivers/spi.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdarg.h>

#define REPEAT_COUNT 1000UL
#define BUFFER_COUNT 1
#define BUFFER_SIZE 1024
#define LENGTH_STRIDE 16
#define TRANSFER_LENGTH_MIN 16
#define TRANSFER_LENGTH_MAX BUFFER_SIZE

#define FRAME_SIZE COND_CODE_1(CONFIG_SPI_LOOPBACK_16BITS_FRAMES, (16), (8))
#define MODE_LOOP  COND_CODE_1(CONFIG_SPI_LOOPBACK_MODE_LOOP, (SPI_MODE_LOOP), (0))

#define SPI_OP(frame_size)                                                 \
    SPI_OP_MODE_MASTER | SPI_MODE_CPOL | MODE_LOOP | SPI_MODE_CPHA |       \
        SPI_WORD_SET(frame_size) | SPI_LINES_SINGLE

#define SPI_DEV_NODE DT_NODELABEL(dut_spi)
static struct spi_dt_spec spi_dt = SPI_DT_SPEC_GET(SPI_DEV_NODE, SPI_OP(FRAME_SIZE), 0);

static uint8_t tx_buf[BUFFER_COUNT * BUFFER_SIZE];
static struct spi_buf spi_tx_bufs[BUFFER_COUNT];

static struct spi_buf_set tx_bufs = {
    .buffers = spi_tx_bufs,
    .count = BUFFER_COUNT,
};

static uint8_t rx_buf[BUFFER_COUNT * BUFFER_SIZE];
struct spi_buf spi_rx_bufs[BUFFER_COUNT];
struct spi_buf_set rx_bufs = {
    .buffers = spi_rx_bufs,
    .count = BUFFER_COUNT,
};

static void setup_buffers(void)
{
    // fill tx buffer with some data
    for (int i = 0; i < sizeof(tx_buf); i++) {
        tx_buf[i] = (uint8_t)i;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        spi_tx_bufs[i].buf = tx_buf + (uint32_t)(i * BUFFER_SIZE);
        spi_tx_bufs[i].len = 1;

        spi_rx_bufs[i].buf = rx_buf + (uint32_t)(i * BUFFER_SIZE);
        spi_rx_bufs[i].len = 1;
    }
}

// nrf54l15dk/nrf54l15/cpuapp
// xg24_rb4187c

static uint64_t spi_benchmark_time_per_spi_transceive(size_t transfer_length)
{
    for (int i = 0; i < BUFFER_COUNT; i++) {
        spi_rx_bufs[i].len = transfer_length;
        spi_tx_bufs[i].len = transfer_length;
    }
    uint32_t start = k_cycle_get_32();

    for (int i = 0; i < REPEAT_COUNT; i++) {
        spi_transceive_dt(&spi_dt, &tx_bufs, &rx_bufs);
        // spi_write_dt(&spi_dt, &tx_bufs);
    }
    uint32_t end = k_cycle_get_32();

    uint32_t cycles = end - start;
    uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
    return (uint32_t)(us / REPEAT_COUNT);
}

static uint64_t spi_benchmark_time_per_spi_read(size_t transfer_length)
{
    for (int i = 0; i < BUFFER_COUNT; i++) {
        spi_rx_bufs[i].len = transfer_length;
        spi_tx_bufs[i].len = transfer_length;
    }
    uint32_t start = k_cycle_get_32();

    for (int i = 0; i < REPEAT_COUNT; i++) {
        spi_read_dt(&spi_dt, &rx_bufs);
        // spi_write_dt(&spi_dt, &tx_bufs);
    }
    uint32_t end = k_cycle_get_32();

    uint32_t cycles = end - start;
    uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
    return (uint32_t)(us / REPEAT_COUNT);
}

static uint64_t spi_benchmark_time_per_spi_write(size_t transfer_length)
{
    for (int i = 0; i < BUFFER_COUNT; i++) {
        spi_rx_bufs[i].len = transfer_length;
        spi_tx_bufs[i].len = transfer_length;
    }
    uint32_t start = k_cycle_get_32();

    for (int i = 0; i < REPEAT_COUNT; i++) {
        spi_write_dt(&spi_dt, &tx_bufs);
        // spi_write_dt(&spi_dt, &tx_bufs);
    }
    uint32_t end = k_cycle_get_32();

    uint32_t cycles = end - start;
    uint64_t us = ((uint64_t)cycles * 1000000ULL) / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;
    return (uint32_t)(us / REPEAT_COUNT);
}

int main(void)
{
    setup_buffers();
    // (void)pm_device_runtime_get(spi_dt.bus);
    spi_transceive_dt(&spi_dt, &tx_bufs, &rx_bufs);

    uint32_t results[(TRANSFER_LENGTH_MAX - TRANSFER_LENGTH_MIN) / LENGTH_STRIDE + 1];
    // for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
    //     size_t length = TRANSFER_LENGTH_MIN + i * LENGTH_STRIDE;
    //     results[i] = spi_benchmark_time_per_spi_transceive(length);
    // }

    // for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
    //     uint32_t length = TRANSFER_LENGTH_MIN + i * LENGTH_STRIDE;
    //     printk("%u,%u\n", length, results[i]);
    // }

    for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
        size_t length = TRANSFER_LENGTH_MIN + i * LENGTH_STRIDE;
        results[i] = spi_benchmark_time_per_spi_read(length);
    }

    for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
        uint32_t length = TRANSFER_LENGTH_MIN + i * LENGTH_STRIDE;
        printk("%u,%u\n", length, results[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
        size_t length = TRANSFER_LENGTH_MIN + i * LENGTH_STRIDE;
        results[i] = spi_benchmark_time_per_spi_write(length);
    }

    for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
        uint32_t length = TRANSFER_LENGTH_MIN + i * LENGTH_STRIDE;
        printk("%u,%u\n", length, results[i]);
    }

    // (void)pm_device_runtime_put(spi_dt.bus);

    return 0;
}
