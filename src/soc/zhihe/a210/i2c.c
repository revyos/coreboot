/* SPDX-License-Identifier: GPL-2.0-only */
#include <drivers/i2c/designware/dw_i2c.h>
#include <soc/early.h>
#include <soc/addressmap.h>
#include <soc/gpio.h>
#include <delay.h>
#include <timer.h>

#define AON_CLOCK_CONFIG (A210_AON_CLOCK_BASE + 0x00)
#define AON_CLOCK_GATE (A210_AON_CLOCK_BASE + 0x04)
#define AON_CLOCK_SOURCE_SHIFT 4
#define AON_CLOCK_SOURCE_MASK 3
#define AON_CLOCK_DIVIDER_MASK 7
#define AON_I2C1_CLOCKS ((1U << 20) | (1U << 21))
#define AON_I2C_RESET_CONFIG (A210_AON_RESET_BASE + 0x10)
#define AON_I2C1_RESET_N (1U << 1)
#define AON_I2C1_SCL_PAD 15
#define AON_I2C1_SDA_PAD 16
#define AON_PAD_CONFIG_WIDTH 16
#define AON_PAD_CONFIG_MASK 0x3ffU
#define AON_PAD_INPUT (1U << 9)
#define AON_PAD_SLEW (1U << 8)
#define AON_PAD_SCHMITT (1U << 7)
#define AON_PAD_DRIVE_25MA 15
#define IC_INTR_MASK 0x30
#define IC_CLR_INTR 0x40
#define IC_ENABLE 0x6c
#define IC_STATUS 0x70
#define IC_TXFLR 0x74
#define IC_RXFLR 0x78
#define IC_DMA_CR 0x88
#define IC_ENABLE_STATUS 0x9c
#define IC_ACTIVE ((1U << 5) | 1U)

int a210_pmic_i2c_release(void)
{
	void *base = (void *)A210_AON_I2C_BASE;
	struct stopwatch timeout;

	/* Do not hand off an outstanding transaction or discard unread data. */
	if ((read32(base + IC_STATUS) & IC_ACTIVE) ||
	    read32(base + IC_TXFLR) || read32(base + IC_RXFLR) ||
	    read32(base + IC_DMA_CR))
		return -1;
	/* The polling DesignWare driver leaves STOP/ABORT interrupts unmasked. */
	write32(base + IC_INTR_MASK, 0);
	write32(base + IC_ENABLE, 0);
	stopwatch_init_msecs_expire(&timeout, 20);
	while (read32(base + IC_ENABLE_STATUS) & 1) {
		if (stopwatch_expired(&timeout))
			return -1;
		udelay(1);
	}
	read32(base + IC_CLR_INTR);
	return read32(base + IC_INTR_MASK) ? -1 : 0;
}

uintptr_t dw_i2c_base_address(unsigned int bus)
{
	if (bus == 0)
		return A210_AON_I2C_BASE;
	if (bus == 1)
		return A210_AON_I2C1_BASE;
	return 0;
}

int a210_aon_i2c1_init(void)
{
	static const unsigned int source_hz[] = {
		110000000, 24000000, 26000000, 32768,
	};
	uint32_t clock = read32((void *)AON_CLOCK_CONFIG);
	unsigned int hz = source_hz[(clock >> AON_CLOCK_SOURCE_SHIFT) &
				   AON_CLOCK_SOURCE_MASK];
	hz /= (clock & AON_CLOCK_DIVIDER_MASK) + 1;
	if (hz < 1000000)
		return -1;
	struct dw_i2c_bus_config config = {
		.speed = I2C_SPEED_STANDARD,
		.speed_config = {{
			.speed = I2C_SPEED_STANDARD,
			.scl_hcnt = DIV_ROUND_UP(hz, 200000),
			.scl_lcnt = DIV_ROUND_UP(hz, 200000),
			.sda_hold = DIV_ROUND_UP(hz, 3000000),
		}},
	};

	/* AP-owned bus: preserve the PMIC bus and the AON clock source. */
	setbits32((void *)AON_CLOCK_GATE, AON_I2C1_CLOCKS);
	setbits32((void *)AON_I2C_RESET_CONFIG, AON_I2C1_RESET_N);
	for (unsigned int pad = AON_I2C1_SCL_PAD;
	     pad <= AON_I2C1_SDA_PAD; pad++) {
		unsigned int shift = (pad % 2) * AON_PAD_CONFIG_WIDTH;
		uintptr_t reg = A210_AON_PADCTRL_BASE + (pad / 2) * 4;
		uint32_t value = AON_PAD_INPUT | AON_PAD_SLEW |
			AON_PAD_SCHMITT | AON_PAD_DRIVE_25MA;

		/* Function zero on these two pads is AOI2C1. */
		a210_pad_function(A210_AON_PADCTRL_BASE, pad, 0);
		clrsetbits32((void *)reg, AON_PAD_CONFIG_MASK << shift,
			     value << shift);
	}
	return dw_i2c_init(1, &config);
}

int a210_aon_i2c_init(void)
{
	/* AON I2C uses the 24 MHz oscillator during early boot. */
	const struct dw_i2c_bus_config config = {
		.speed = I2C_SPEED_STANDARD,
		.speed_config = {{
			.speed = I2C_SPEED_STANDARD,
			.scl_hcnt = 120,
			.scl_lcnt = 120,
			.sda_hold = 8,
		}},
	};

	return dw_i2c_init(0, &config);
}
