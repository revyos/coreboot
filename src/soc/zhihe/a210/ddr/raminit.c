/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/early.h>
#include <soc/dram.h>
#include <timer.h>
#include "ddr.h"

#define DDRC_BASE A210_DDR_CTRL_BASE
#define CHANNEL_STRIDE A210_DDR_CHANNEL_STRIDE
#define DDR_PLL A210_SLC_DUAL_BASE

static void write_both(uintptr_t reg, uint32_t value)
{
	write32((void *)reg, value);
	write32((void *)(reg + CHANNEL_STRIDE), value);
}

static int wait_reg(uintptr_t reg, uint32_t mask, uint32_t expected)
{
	struct stopwatch deadline;

	stopwatch_init_msecs_expire(&deadline, 1000);
	while ((read32((void *)reg) & mask) != expected) {
		if (stopwatch_expired(&deadline)) {
			printk(BIOS_ERR,
			       "A210: DDR timeout at %#lx (%#x)\n", reg,
			       read32((void *)reg));
			return -1;
		}
	}
	return 0;
}

static int wait_both(uint32_t offset, uint32_t mask, uint32_t value)
{
	return wait_reg(DDRC_BASE + offset, mask, value) ||
	       wait_reg(DDRC_BASE + CHANNEL_STRIDE + offset, mask, value);
}

static int pll_init(void)
{
	/* This function, its callers and the stack must remain in BRAM. */
	clrsetbits32((void *)(DDR_PLL + DDR_PLL_CFG1), DDR_PLL_FRAC_MASK,
		     DDR_PLL_RESET | DDR_PLL_4266_FRAC);
	write32((void *)(DDR_PLL + DDR_PLL_CFG0), DDR_PLL_4266_CFG0);
	udelay(2);
	clrbits32((void *)(DDR_PLL + DDR_PLL_CFG1), DDR_PLL_RESET);
	if (wait_reg(DDR_PLL + DDR_PLL_STATUS, DDR_PLL_LOCK, DDR_PLL_LOCK))
		return -1;
	write32((void *)(DDR_PLL + DDR_PLL_STATUS), DDR_PLL_CLOCK_ENABLE);
	return 0;
}

static void dq_map(const struct a210_dram_config *config)
{
	for (unsigned int byte = 0; byte < 4; byte++)
		for (unsigned int bit = 0; bit < 8; bit++)
			a210_phy_write_both(0x100a0 + 0x1000 * byte + bit,
					    config->dq_map[byte][bit]);
}

static int mission_mode(void)
{
	write_both(DDRC_BASE + DDRC_SWCTL, 0);
	write_both(DDRC_BASE + DDRC_INIT0, 0xc0020002);
	write_both(DDRC_BASE + DDRC_SWCTL, 1);
	write_both(DDRC_BASE + DDRC_SWCTL, 0);
	write_both(DDRC_BASE + DDRC_DFIMISC, 0x34);
	if (wait_both(DDRC_DFISTAT, 1, 1))
		return -1;
	write_both(DDRC_BASE + DDRC_DFIMISC, 0x14);
	write_both(DDRC_BASE + DDRC_DFIMISC, 0x15);
	write_both(DDRC_BASE + DDRC_PWRCTL, 0x10b);
	write_both(DDRC_BASE + DDRC_SWCTL, 1);
	if (wait_both(DDRC_SWSTAT, 1, 1) || wait_both(DDRC_STAT, 7, 1))
		return -1;
	write_both(DDRC_BASE + DDRC_DFIPHYMSTR, 0x14000001);
	if (wait_both(DDRC_SWSTAT, 1, 1))
		return -1;
	write_both(DDRC_BASE + DDRC_DBG1, 0);
	write_both(DDRC_BASE + DDRC_PCTRL_0, 1);
	write_both(DDRC_BASE + DDRC_RFSHCTL3, 0);
	return 0;
}

static int mode_register(unsigned int channel, unsigned int mr,
			 uint8_t *value)
{
	uintptr_t base = DDRC_BASE + channel * CHANNEL_STRIDE;
	uint32_t ctrl = read32((void *)(base + DDRC_MRCTRL0));
	uint32_t data = read32((void *)(base + DDRC_MRCTRL1));
	uint32_t command = (ctrl & ~(DDR_MR_TRIGGER | DDR_MR_RANK_MASK)) |
			   DDR_MR_RANK0 | DDR_MR_READ;

	if (wait_reg(base + DDRC_MRSTAT, DDR_MR_BUSY, 0))
		return -1;
	write32((void *)(base + DDRC_MRCTRL0), command);
	write32((void *)(base + DDRC_MRCTRL1), mr << DDR_MR_ADDRESS_SHIFT);
	write32((void *)(base + DDRC_MRCTRL0), command | DDR_MR_TRIGGER);
	udelay(20);
	if (wait_reg(base + DDRC_MRSTAT, DDR_MR_BUSY, 0))
		return -1;
	/* DDR_SYSREG MRR_DATA0, not the unrelated +0x74 register. */
	uint32_t result =
	    read32((void *)(A210_DDR_SYSREG_BASE +
			    channel * CHANNEL_STRIDE + DDR_MRR_DATA0));

	write32((void *)(base + DDRC_MRCTRL0), ctrl);
	write32((void *)(base + DDRC_MRCTRL1), data);
	if ((result & 0xff) != ((result >> 16) & 0xff))
		return -1;
	*value = result & 0xff;
	return 0;
}

static uint64_t dram_capacity(void)
{
	static const uint8_t density_gbit[] = {2, 3, 4, 6, 8, 12, 16};
	uint8_t mr8[2], manufacturer;

	for (unsigned int ch = 0; ch < 2; ch++) {
		if (mode_register(ch, 5, &manufacturer) ||
		    mode_register(ch, 8, &mr8[ch]))
			return 0;
		printk(BIOS_INFO, "A210: DDR%u MR5=%#x MR8=%#x\n", ch,
		       manufacturer, mr8[ch]);
	}
	/* This timing supports one rank of x16 LPDDR4X devices. */
	unsigned int density = (mr8[0] >> 2) & 0xf;

	if (mr8[0] != mr8[1] || (mr8[0] & 0xc3) ||
	    density >= ARRAY_SIZE(density_gbit))
		return 0;
	/* Two x16 devices per 32-bit controller, two controllers. */
	return (uint64_t)density_gbit[density] << 29;
}

int a210_ddr_init(const struct a210_dram_config *config, uint64_t *size)
{
	printk(BIOS_INFO,
	       "A210: LPDDR4X-4266 single-rank initialization\n");
	if (!config || !size || config->profile != A210_LPDDR4X_4266_1R)
		return -1;
	*size = 0;
	/* Reset both PHYs before changing their clock. */
	write_both(A210_DDR_SYSREG_BASE + DDR_SYS_CONFIG, 0);
	write32((void *)AON_DDR_POWER_OK, 0);
	udelay(10);
	if (pll_init())
		return -1;
	for (unsigned int ch = 0; ch < 2; ch++) {
		setbits32((void *)(DDR_PLL + SLC_CLOCK_CONTROL(ch)),
			  SLC_AXI_CLOCK_ENABLE);
		setbits32((void *)(DDR_PLL + SLC_CLOCK_CONTROL(ch)),
			  SLC_SOFT_RESET_N);
		setbits32((void *)(DDR_PLL + DDR_CLOCK_CONTROL(ch)),
			  DDR_SOFT_RESET_N);
	}
	write_both(A210_SLC_SYSREG_BASE + SLC_SYSTEM_RESET,
		   SLC_WRAP_RESET_N);
	write32((void *)AON_DDR_POWER_OK, 0);
	udelay(1);
	write32((void *)AON_DDR_POWER_OK, AON_DDR_POWER_OK_BOTH);
	udelay(1);
	write_both(A210_DDR_SYSREG_BASE + DDR_SYS_CONFIG, DDR_PHY_RESET_N);
	write_both(A210_DDR_SYSREG_BASE + DDR_SYS_CONFIG,
		   DDR_PHY_RESET_N | DDR_APB_RESET_N);
	for (size_t i = 0; i < a210_ddrc_setting_count; i++)
		write_both(DDRC_BASE + a210_ddrc_4266_1r[i].offset,
			   a210_ddrc_4266_1r[i].value);
	write_both(A210_DDR_SYSREG_BASE + DDR_SYS_CONFIG,
		   DDR_PHY_RESET_N | DDR_APB_RESET_N | DDR_CTRL_RESET_N);
	write_both(A210_DDR_SYSREG_BASE + DDR_SYS_CONFIG,
		   DDR_PHY_RESET_N | DDR_APB_RESET_N | DDR_CTRL_RESET_N);
	write_both(A210_DDR_SYSREG_BASE + DDR_SYS_CONFIG,
		   DDR_PHY_RESET_N | DDR_APB_RESET_N | DDR_CTRL_RESET_N |
		       DDR_AXI_RESET_N);
	write_both(A210_DDR_SYSREG_BASE + DDR_SYS_CONFIG,
		   DDR_PHY_RESET_N | DDR_APB_RESET_N | DDR_CTRL_RESET_N |
		       DDR_AXI_RESET_N);
	dq_map(config);
	if (a210_ddr_phy_train() || mission_mode())
		return -1;
	printk(BIOS_INFO,
	       "A210: both DDR controllers entered normal mode\n");
	*size = dram_capacity();
	if (!*size)
		return -1;
	a210_dram_map();
	return 0;
}
