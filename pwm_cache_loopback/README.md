# pwm_cache_loopback

Detects the `pwm_nrfx` data cache flush bug (flush of `seq.length` bytes instead of
`seq.length * sizeof(uint16_t)`) by measuring the real PWM output.

PWM120 channels 0..3 are looped back to GPIO inputs with jumper wires on header P7:

| PWM out | Input |
|---------|-------|
| P7.0 (ch0) | P7.4 |
| P7.1 (ch1) | P7.5 |
| P7.6 (ch2) | P7.2 |
| P7.7 (ch3) | P7.3 |

PWM120 can only reach P7.0, P7.1, P7.6 and P7.7 (see the `ctrlsel_lookup` in
`gen_periphconf_entries.py`), hence the odd pin choice.

The app puts a pad (`CONFIG_SAMPLE_PAD_SIZE`, 28 bytes) in front of the driver's 8-byte sequence
buffer, so channels 0-1 and 2-3 are in different 32-byte cache lines. It fills the buffer in RAM
with a value that is requested for no channel, sets duty cycles 25/50/75/10 % (period 1 ms) and
samples P7 to measure the duty cycle on the inputs. With the bug the line holding channels 2-3 is
not flushed, so they output the poison value (~41 %) -> `RESULT: FAIL`.

    west build --sysbuild -b nrf54h20dk/nrf54h20/cpuapp pwm_cache_loopback

Sysbuild is required: it generates the UICR/PERIPHCONF image that grants the app core access to
the P7 pins. Flash with `west flash`. To test the unfixed driver build with
`ZEPHYR_BASE=<path to zephyr_prefix>`.

The pad layout depends on link order; if main() reports that the buffer is not right after the
pad, check `zephyr.map` and adjust `CONFIG_SAMPLE_PAD_SIZE`.
