/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (c) 2025 Yao Zi <ziyao@disroot.org> */
#include <bootblock_common.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/cpu.h>

void bootblock_soc_early_init(void)
{
	/* Stop BootROM USB DMA before its SRAM buffers are reused. */
	clrbits32((void *)0xffec02c014, 7);
	udelay(10);
}

void bootblock_soc_init(void)
{
	/* Cold boot releases hart 0 only. Join the coreboot SMP rendezvous. */
	for (unsigned int hart = 1; hart < CONFIG_MAX_CPUS; hart++) {
		write32((void *)(0xffef018050 + 8UL * hart),
			(uint32_t)TH1520_SRAM_BASE);
		write32((void *)(0xffef018054 + 8UL * hart),
			TH1520_SRAM_BASE >> 32);
	}
	setbits32((void *)0xffef014004, 0x1c);
}
