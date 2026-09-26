/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/clock.h>
#include <timer.h>

static int wait_lock(uintptr_t pll)
{
	struct stopwatch timeout;

	stopwatch_init_msecs_expire(&timeout, 10);
	while (!(read32((void *)(pll + PLL_STATUS)) & PLL_LOCK)) {
		if (stopwatch_expired(&timeout))
			return -1;
		udelay(1);
	}
	return 0;
}

int a210_set_cpu_clock(unsigned int cluster, unsigned int mhz)
{
	uintptr_t pll, ccu, mux, backup;
	uint32_t saved_backup, cfg0, monitor, measured;

	/* 24 MHz * 79 / 1: the shared 1.896 GHz operating point. */
	if (cluster > 1 || mhz != 1896)
		return -1;
	pll = cluster ? A210_C920_PLL : A210_C908_PLL;
	ccu = cluster ? A210_C920_CCU : A210_C908_CCU;
	mux = A210_CPU_CLOCK_MUX + cluster * sizeof(uint32_t);
	backup = TOP_CPU_CLOCK_BACKUP(cluster);
	/* Secondary harts must still be held in reset. */
	if (!(read32((void *)(ccu + CCU_CONTROL)) &
	      CCU_SOFTWARE_CONTROL) ||
	    (read32((void *)(ccu + CCU_LOW_POWER_CONTROL)) &
	     (CCU_HIGH_SPEED | CCU_CLOCK_ENABLE)) !=
		(CCU_HIGH_SPEED | CCU_CLOCK_ENABLE))
		return -1;

	cfg0 = PLL_CFG0_INTEGER(79, 1, 1, 1);
	saved_backup = read32((void *)backup);
	clrsetbits32((void *)backup, TOP_CPU_BACKUP_MUX_MASK,
		     TOP_CPU_BACKUP_DIV0);
	setbits32((void *)mux, CPU_CLOCK_FROM_TOP);
	if (!(read32((void *)mux) & CPU_CLOCK_FROM_TOP))
		return -1;
	udelay(10);
	write32((void *)(pll + PLL_CFG1),
		PLL_RESET | PLL_INTEGER_MODE | PLL_DAC_POWER_DOWN);
	write32((void *)(pll + PLL_CFG0), cfg0);
	write32((void *)(pll + PLL_CFG1),
		PLL_INTEGER_MODE | PLL_DAC_POWER_DOWN);
	if (wait_lock(pll)) {
		printk(BIOS_ERR, "A210: CPU%u PLL lock timed out\n",
		       cluster);
		/* Keep the TOP clock if the CPU PLL is unusable. */
		return -1;
	}
	clrsetbits32((void *)(ccu + CCU_RATIO_NORMAL), CCU_RATIO_MASK, 0);
	clrbits32((void *)mux, CPU_CLOCK_FROM_TOP);
	udelay(10);
	if (read32((void *)(pll + PLL_CFG0)) != cfg0 ||
	    (read32((void *)mux) & CPU_CLOCK_FROM_TOP) ||
	    (read32((void *)(ccu + CCU_RATIO_NORMAL)) & CCU_RATIO_MASK))
		return -1;
	write32((void *)backup, saved_backup);

	monitor = (cluster ? CPU_MONITOR_C920 : CPU_MONITOR_C908)
		  << CPU_MONITOR_SELECT_SHIFT;
	write32((void *)CPU_CLOCK_MONITOR, monitor);
	write32((void *)CPU_CLOCK_MONITOR, monitor | CPU_MONITOR_ENABLE);
	mdelay(2);
	measured = read32((void *)CPU_CLOCK_MEASURED_KHZ);
	write32((void *)CPU_CLOCK_MONITOR, monitor);
	printk(BIOS_INFO, "A210: C%u target %u MHz, measured %u kHz\n",
	       cluster ? 920 : 908, mhz, measured);
	/* Measure the clock independently of PLL readback. */
	if (measured < mhz * 990 || measured > mhz * 1010)
		return -1;
	return 0;
}
