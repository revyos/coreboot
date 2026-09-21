/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
/* Based on the TH1520 U-Boot boot_cpu.c unused-PLL switching sequence. */
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/cpu.h>
#include <timer.h>

#define CLOCK_BASE 0xffef010000ULL
#define CPU_MUX ((void *)(CLOCK_BASE + 0x100))
#define PLL_STATUS ((void *)(CLOCK_BASE + 0x80))
#define PLL_RESET (1U << 29)
#define PLL_BYPASS (1U << 30)
#define PLL_INTEGER ((1U << 25) | (1U << 24))
#define PLL_SELECT (1U << 0)
#define OSC_SELECT (1U << 1)
#define BUS_RATIO (7U << 8)
#define BUS_SYNC (1U << 11)

static unsigned int cpu_hz(void)
{
	uint32_t mux = read32(CPU_MUX);
	uintptr_t base = CLOCK_BASE + ((mux & PLL_SELECT) ? 0x10 : 0);
	uint32_t cfg = read32((void *)base), control = read32((void *)(base + 4));
	unsigned int divisor = (cfg & 63) * ((cfg >> 20) & 7) * ((cfg >> 24) & 7);
	uint64_t feedback = (uint64_t)((cfg >> 8) & 4095) << 24;

	if ((!(mux & PLL_SELECT) && (mux & OSC_SELECT)) || (control & PLL_BYPASS))
		return 24000000;
	if (!divisor)
		return 0;
	if (!(control & (1U << 24)))
		feedback += control & 0xffffff;
	return ((feedback * 24000000) >> 24) / divisor;
}

static void select_pll(bool pll1)
{
	if (!pll1)
		clrbits32(CPU_MUX, OSC_SELECT);
	clrsetbits32(CPU_MUX, PLL_SELECT, pll1 ? PLL_SELECT : 0);
}

int th1520_cpu_set_clock(unsigned int mhz)
{
	unsigned int cfg, divider;

	switch (mhz) {
	case 300:
		cfg = 1 | (125U << 8) | (5U << 20) | (2U << 24);
		divider = 2;
		break;
	case 750:
		cfg = 1 | (125U << 8) | (4U << 20) | (1U << 24);
		divider = 5;
		break;
	case 1848:
		cfg = 1 | (77U << 8) | (1U << 20) | (1U << 24);
		divider = 3;
		break;
	default:
		return -1;
	}
	unsigned int target = mhz * 1000000U;
	unsigned int initial = cpu_hz();

	if (!initial)
		return -1;
	bool pll1 = !(read32(CPU_MUX) & PLL_SELECT);
	uintptr_t base = CLOCK_BASE + (pll1 ? 0x10 : 0);
	void *control = (void *)(base + 4);

	setbits32(control, PLL_RESET);
	udelay(3);
	write32((void *)base, cfg);
	clrsetbits32(control, PLL_BYPASS | 0xffffff, PLL_INTEGER);
	clrbits32(control, PLL_RESET);
	if (!wait_us(1000, read32(PLL_STATUS) & (pll1 ? (1U << 4) : (1U << 1)))) {
		printk(BIOS_WARNING, "TH1520: CPU PLL failed to lock; keeping %u MHz\n",
			initial / 1000000);
		return -1;
	}
	udelay(30);
	/* Lower CPU frequency first if the inherited clock is faster. */
	if (initial > target)
		select_pll(pll1);
	clrsetbits32(CPU_MUX, BUS_RATIO, (divider - 1) << 8);
	clrbits32(CPU_MUX, BUS_SYNC);
	udelay(1);
	setbits32(CPU_MUX, BUS_SYNC);
	udelay(1);
	if (initial <= target)
		select_pll(pll1);

	unsigned int frequency = cpu_hz();
	unsigned int ratio = ((read32(CPU_MUX) >> 8) & 7) + 1;

	printk(BIOS_INFO, "TH1520: CPU %u -> %u MHz, bus %u MHz\n",
		initial / 1000000, frequency / 1000000, frequency / ratio / 1000000);
	return frequency == target ? 0 : -1;
}

void th1520_cpu_clock_init(void)
{
	/* AON may restore 0.8 V while it initializes its PMIC policy. */
	if (CONFIG(TH1520_AON))
		th1520_cpu_set_clock(300);
	else if (CONFIG(TH1520_CPU_750MHZ))
		th1520_cpu_set_clock(750);
}
