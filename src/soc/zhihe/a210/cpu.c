/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd. */
#include <console/console.h>
#include <device/mmio.h>
#include <soc/early.h>
#include <soc/cpu.h>
#include <soc/power.h>
#include <timer.h>

static int power_on(uintptr_t bpc, uintptr_t pcu, uintptr_t ppu)
{
	struct stopwatch deadline;
	uint32_t status;

	if ((read32((void *)(ppu + PPU_POWER_STATUS)) & PPU_POWER_MASK) ==
	    PPU_POWER_ON)
		return 0;
	/* Hardware BPC and static PPU, as in A210 pctrl_cfg.c. */
	write32((void *)(bpc + BPC_SW_CONTROL), BPC_HARDWARE_CONTROL);
	write32((void *)(pcu + PCU_DEVICE_ENABLE_LOW2HIGH),
		PCU_DEVICE_ENABLE);
	clrsetbits32((void *)(ppu + PPU_CONTROL), PPU_CONFIG_MASK,
		     PPU_MEMORY_DEEP_SLEEP_ENABLE);
	write32((void *)(ppu + PPU_SW_STATE), PPU_POWER_ON);
	write32((void *)(ppu + PPU_SW_REQUEST), PPU_REQUEST);
	stopwatch_init_msecs_expire(&deadline, 100);
	do {
		status = read32((void *)(ppu + PPU_SW_RESPONSE)) &
			 PPU_RESPONSE_MASK;
		if (status == PPU_RESPONSE_DENY ||
		    stopwatch_expired(&deadline)) {
			printk(BIOS_ERR, "A210: PPU %#lx status %#x\n",
			       ppu, status);
			return -1;
		}
	} while (status != PPU_RESPONSE_ACCEPT);
	return (read32((void *)(ppu + PPU_POWER_STATUS)) &
		PPU_POWER_MASK) == PPU_POWER_ON
		   ? 0
		   : -1;
}

int a210_prepare_harts(void)
{
	/* OpenSBI installs vectors before releasing hart resets. */
	if ((read32((void *)A210_C908_RESET_CTRL) & CPU_RESET_MASK) !=
		(CPU_CLUSTER_RESET_RELEASE | CPU_CORE_RESET_RELEASE(0)) ||
	    (read32((void *)A210_C920_RESET_CTRL) & CPU_CORE_RESET_MASK)) {
		printk(BIOS_ERR,
		       "A210: secondary hart already out of reset\n");
		return -1;
	}
	if (power_on(A210_CLUSTER_BPC(A210_C920_CLUSTER),
		     A210_CLUSTER_PCU(A210_C920_CLUSTER),
		     A210_CLUSTER_PPU(A210_C920_CLUSTER)))
		return -1;
	for (unsigned int hart = 1; hart < A210_HART_COUNT; hart++) {
		if (power_on(A210_CORE_BPC(hart), A210_CORE_PCU(hart),
			     A210_CORE_PPU(hart)))
			return -1;
	}
	printk(BIOS_INFO, "A210: secondary hart power domains ready\n");
	return 0;
}
