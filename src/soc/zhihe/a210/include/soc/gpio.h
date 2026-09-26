/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_GPIO_H
#define SOC_ZHIHE_A210_GPIO_H

#include <device/mmio.h>
#include <soc/addressmap.h>

#define A210_GPIO_DATA 0x00
#define A210_GPIO_DIRECTION 0x04
#define A210_PAD_MUX_OFFSET 0x400
#define A210_PAD_MUX_WIDTH 4
#define A210_PAD_MUX_MASK 0xfU
#define A210_PADS_PER_MUX 8
#define A210_PERI1_CLOCK_ENABLE (A210_PERI1_SYSREG_BASE + 0x200)
#define A210_PERI1_GPIO0_CLOCK (1U << 5)

static inline void a210_gpio_output(uintptr_t base, unsigned int pin)
{
	/* Set the output latch before enabling its driver. */
	setbits32((void *)(base + A210_GPIO_DATA), 1U << pin);
	setbits32((void *)(base + A210_GPIO_DIRECTION), 1U << pin);
}

static inline void a210_pad_function(uintptr_t base, unsigned int pad,
				     unsigned int function)
{
	unsigned int shift = pad % A210_PADS_PER_MUX * A210_PAD_MUX_WIDTH;
	uintptr_t reg = base + A210_PAD_MUX_OFFSET +
		pad / A210_PADS_PER_MUX * sizeof(uint32_t);

	clrsetbits32((void *)reg, A210_PAD_MUX_MASK << shift,
		     (function & A210_PAD_MUX_MASK) << shift);
}

static inline void a210_pad_gpio(uintptr_t base, unsigned int pad)
{
	a210_pad_function(base, pad, 0);
}

#endif
