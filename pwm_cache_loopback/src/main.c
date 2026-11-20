#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/cache.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/linker/devicetree_regions.h>

#define PWM_NODE	DT_ALIAS(pwm_test)
#define DMA_REGION	DT_PHANDLE(PWM_NODE, memory_regions)
#define CHANNELS	4
#define PERIOD		PWM_MSEC(1)
#define SAMPLES		50000
#define TOLERANCE_PCT	5
/* Compare value (~41 % of the 20000-tick period) that is not requested for any channel. */
#define POISON		(0x8000 | 8192)

/* The linker sorts the region by alignment: this align-4 pad comes before the driver's
 * align-2 sequence buffer, which therefore starts right after it.
 */
static volatile uint32_t pad[CONFIG_SAMPLE_PAD_SIZE / 4] __used
	__attribute__((section(LINKER_DT_NODE_REGION_NAME(DMA_REGION))));

static const struct device *const pwm = DEVICE_DT_GET(PWM_NODE);
static const struct device *const gpio = DEVICE_DT_GET(DT_NODELABEL(gpio7));

/* P7 input pin that senses each PWM channel, and the duty cycle requested for it. */
static const uint8_t sense_pin[CHANNELS] = {4, 5, 2, 3};
static const uint32_t duty_pct[CHANNELS] = {25, 50, 75, 10};

int main(void)
{
	volatile uint16_t *seq = (volatile uint16_t *)((uintptr_t)pad + sizeof(pad));
	uint32_t high[CHANNELS] = {0};
	int failed = 0;

	if (!device_is_ready(pwm) || !device_is_ready(gpio)) {
		printk("PWM or GPIO device not ready\n");
		return 0;
	}

	for (int ch = 0; ch < CHANNELS; ch++) {
		gpio_pin_configure(gpio, sense_pin[ch], GPIO_INPUT);
	}

	/* RAM survives a reset, so it may still hold the values requested below by a previous
	 * run. Put a value that is requested for no channel into RAM; this is what the PWM
	 * reads for every channel whose update is not flushed.
	 */
	for (int ch = 0; ch < CHANNELS; ch++) {
		seq[ch] = POISON;
	}
	sys_cache_data_flush_range((void *)seq, CHANNELS * sizeof(seq[0]));

	/* A new playback request is ignored until the current period ends, so wait between calls. */
	for (int ch = 0; ch < CHANNELS; ch++) {
		int err = pwm_set(pwm, ch, PERIOD, PERIOD / 100 * duty_pct[ch], 0);

		if (err != 0) {
			printk("pwm_set(ch %d) failed: %d\n", ch, err);
			return 0;
		}
		k_msleep(5);
	}

	/* Check that the sequence buffer is where the pad assumes (the cached view shows what
	 * the driver wrote there).
	 */
	if (seq[0] == POISON || seq[0] == 0) {
		printk("Driver buffer is not right after the pad, adjust SAMPLE_PAD_SIZE\n");
		return 0;
	}
	printk("sequence buffer at %p\n", (void *)seq);

	for (int i = 0; i < SAMPLES; i++) {
		gpio_port_value_t v;

		gpio_port_get_raw(gpio, &v);
		for (int ch = 0; ch < CHANNELS; ch++) {
			high[ch] += (v >> sense_pin[ch]) & 1U;
		}
	}

	for (int ch = 0; ch < CHANNELS; ch++) {
		int measured = high[ch] * 100 / SAMPLES;
		bool ok = abs(measured - (int)duty_pct[ch]) <= TOLERANCE_PCT;

		printk("ch %d: expected %u%%, measured %d%% -> %s\n", ch, (unsigned int)duty_pct[ch],
		       measured, ok ? "OK" : "WRONG");
		failed += !ok;
	}

	printk("RESULT: %s\n", failed ? "FAIL" : "PASS");
	return 0;
}
