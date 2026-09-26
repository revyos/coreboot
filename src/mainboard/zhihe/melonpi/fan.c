/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/i2c_simple.h>
#include <soc/early.h>
#include <soc/gpio.h>
#include "board.h"

#define MELONPI_FAN_ENABLE_PIN 30
#define MELONPI_V1_EXPANDER_BUS 1
#define MELONPI_V1_EXPANDER_ADDRESS 0x20
#define AW9535_OUTPUT0 0x02
#define AW9535_CONFIG0 0x06
#define MELONPI_V1_FAN_ENABLE (1U << 2)

static int v1_fan_init(void)
{
	uint8_t output, config;
	const unsigned int bus = MELONPI_V1_EXPANDER_BUS;
	const unsigned int address = MELONPI_V1_EXPANDER_ADDRESS;

	if (a210_aon_i2c1_init() ||
	    i2c_readb(bus, address, AW9535_OUTPUT0, &output) ||
	    i2c_readb(bus, address, AW9535_CONFIG0, &config))
		return -1;
	/* Set the latch before driving U31 P02; preserve other outputs. */
	if (i2c_writeb(bus, address, AW9535_OUTPUT0,
		       output | MELONPI_V1_FAN_ENABLE) ||
	    i2c_writeb(bus, address, AW9535_CONFIG0,
		       config & ~MELONPI_V1_FAN_ENABLE) ||
	    i2c_readb(bus, address, AW9535_OUTPUT0, &output) ||
	    i2c_readb(bus, address, AW9535_CONFIG0, &config))
		return -1;
	if (!(output & MELONPI_V1_FAN_ENABLE) ||
	    (config & MELONPI_V1_FAN_ENABLE))
		return -1;
	printk(BIOS_INFO, "Melon Pi V1: fan power enabled\n");
	return 0;
}

int melonpi_fan_init(void)
{
	if (CONFIG(MELONPI_V1))
		return v1_fan_init();
	setbits32((void *)A210_PERI1_CLOCK_ENABLE,
		  A210_PERI1_GPIO0_CLOCK);
	a210_gpio_output(A210_GPIO0_BASE, MELONPI_FAN_ENABLE_PIN);
	a210_pad_gpio(A210_PERI1_PADCTRL_BASE, MELONPI_FAN_ENABLE_PIN);
	printk(BIOS_INFO, "Melon Pi: fan power enabled\n");
	return 0;
}
