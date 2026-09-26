/* SPDX-License-Identifier: GPL-2.0-only */
#include <device/mmio.h>
#include <soc/clock.h>

static uint64_t pll_feedback(uint32_t cfg0, uint32_t cfg1)
{
	uint64_t value =
	    (uint64_t)((cfg0 >> PLL_FBDIV_SHIFT) & PLL_FBDIV_MASK)
	    << PLL_FRAC_BITS;

	if (!(cfg1 & PLL_INTEGER_MODE))
		value += cfg1 & PLL_FRAC_MASK;
	return (value * A210_OSC_HZ) >> PLL_FRAC_BITS;
}

unsigned int a210_emmc_refclk(void)
{
	uintptr_t base =
	    (read32((void *)TOP_PERI_CLK_CFG4) & TOP_EMMC_VIDEO_PLL_SELECT)
		? A210_VIDEO_PLL
		: A210_AUDIO0_PLL;
	uint32_t cfg0 = read32((void *)(base + PLL_CFG0));
	uint32_t cfg1 = read32((void *)(base + PLL_CFG1));
	uint32_t refdiv = cfg0 & PLL_REFDIV_MASK;
	uint32_t divider =
	    read32((void *)TOP_PERI_CLK_CFG3) & TOP_EMMC_DIV_MASK;

	if (!refdiv || !divider || (cfg1 & PLL_RESET) ||
	    !(read32((void *)(base + PLL_STATUS)) & PLL_LOCK))
		return 0;
	return pll_feedback(cfg0, cfg1) /
	       (refdiv * divider * EMMC_REFERENCE_DIVIDER);
}

unsigned int a210_uart_refclk(void)
{
	if (read32((void *)TOP_PERI_CLK_CFG3) & TOP_UART_OSC_SELECT)
		return A210_OSC_HZ;
	uint32_t cfg0 = read32((void *)(A210_GMAC_PLL + PLL_CFG0));
	uint32_t cfg1 = read32((void *)(A210_GMAC_PLL + PLL_CFG1));
	uint32_t divisor =
	    (cfg0 & PLL_REFDIV_MASK) *
	    ((cfg0 >> PLL_POSTDIV1_SHIFT) & PLL_POSTDIV_MASK) *
	    ((cfg0 >> PLL_POSTDIV2_SHIFT) & PLL_POSTDIV_MASK) *
	    UART_REFERENCE_DIVIDER;

	if (!divisor || (cfg1 & PLL_RESET))
		return 0;
	if (cfg1 & PLL_BYPASS)
		return A210_OSC_HZ / UART_REFERENCE_DIVIDER;
	return pll_feedback(cfg0, cfg1) / divisor;
}
