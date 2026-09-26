/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 * ADC setup from A210 U-Boot adc.c.
 */
#include <console/console.h>
#include <device/mmio.h>
#include <soc/early.h>
#include <timer.h>

#include <soc/adc.h>
#include <soc/clock.h>

int a210_adc_read_mv(unsigned int channel)
{
	unsigned int sum = 0;

	if (channel >= ADC_CHANNEL_COUNT)
		return -1;

	for (unsigned int sample = 0; sample < ADC_AVERAGE_SAMPLES;
	     sample++) {
		struct stopwatch deadline;
		uint32_t value;

		write32((void *)(A210_ADC_BASE + ADC_OPERATION_CONTROL),
			ADC_CHANNEL_ENABLE(channel) | ADC_SINGLE_MODE);
		write32((void *)(A210_ADC_BASE + ADC_PHY_CONTROL),
			ADC_PHY_ENABLE);
		write32((void *)(A210_ADC_BASE + ADC_SINGLE_START),
			ADC_SINGLE_MODE);
		stopwatch_init_msecs_expire(&deadline, 100);
		do {
			value = read32(
			    (void *)(A210_ADC_BASE + ADC_SAMPLE_DATA));
			if (stopwatch_expired(&deadline)) {
				write32((void *)(A210_ADC_BASE +
						 ADC_PHY_CONTROL),
					ADC_PHY_RESET);
				return -1;
			}
		} while (!(value & ADC_SAMPLE_VALID));
		write32((void *)(A210_ADC_BASE + ADC_PHY_CONTROL), 0);
		sum += value & ADC_SAMPLE_MASK;
	}
	write32((void *)(A210_ADC_BASE + ADC_PHY_CONTROL), ADC_PHY_RESET);
	return (sum / ADC_AVERAGE_SAMPLES) * ADC_REFERENCE_MV /
	       ADC_RESOLUTION;
}

void a210_adc_init(void)
{
	setbits32((void *)PERI3_CLOCK_ENABLE, PERI3_ADC_CLOCK);
	setbits32((void *)PERI3_RESET_N, PERI3_ADC_RESET_N);
	write32((void *)(A210_ADC_BASE + ADC_PHY_CONTROL), ADC_PHY_RESET);
	write32((void *)(A210_ADC_BASE + ADC_PHY_CONTROL), 0);
	write32((void *)(A210_ADC_BASE + ADC_PHY_CONFIG),
		ADC_PHY_REFERENCE_CONFIG);
	write32((void *)(A210_ADC_BASE + ADC_OPERATION_CONTROL),
		ADC_SINGLE_MODE);
	write32((void *)(A210_ADC_BASE + ADC_ACTUAL_INTERRUPT_MASK),
		ADC_USED_INTERRUPT_MASK);
	write32((void *)(A210_ADC_BASE + ADC_DELTA_INTERRUPT_MASK),
		ADC_USED_INTERRUPT_MASK);
	write32((void *)(A210_ADC_BASE + ADC_CLOCK_CONTROL),
		ADC_CLOCK_DEFAULT);
	write32((void *)(A210_ADC_BASE + ADC_START_TIME),
		ADC_START_CYCLES);
	write32((void *)(A210_ADC_BASE + ADC_SAMPLE_TIME),
		ADC_SAMPLE_CYCLES);
}
