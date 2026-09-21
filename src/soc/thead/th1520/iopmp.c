/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 Yao Zi <ziyao@disroot.org>
 * Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn>
 *
 * TH1520 access policy from mainline U-Boot harts_early_init().
 */
#include <arch/barrier.h>
#include <console/console.h>
#include <device/mmio.h>
#include <soc/iopmp.h>

static const struct {
	uintptr_t reg;
	const char *name;
} iopmp[] = {
	{0xfffc0280c0, "eMMC"},
	{0xfffc0290c0, "SDIO0"},
	{0xfffc02a0c0, "SDIO1"},
	{0xfffc02e0c0, "USB0"},
	{0xffffc210c0, "AO"},
	{0xffffc220c0, "AUD"},
	{0xffffc370c0, "CHIP_DBG"},
	{0xffff2200c0, "EIP120I"},
	{0xffff2300c0, "EIP120II"},
	{0xffff2400c0, "EIP120III"},
	{0xfff40800c0, "ISP0"},
	{0xfff40810c0, "ISP1"},
	{0xfff40820c0, "DW200"},
	{0xfff40830c0, "VIPRE"},
	{0xfffcc600c0, "VENC"},
	{0xfffcc610c0, "VDEC"},
	{0xfffcc620c0, "G2D"},
	{0xfffcc630c0, "FCE"},
	{0xffff01c0c0, "NPU"},
	{0xffff5200c0, "DPU0"},
	{0xffff5210c0, "DPU1"},
	{0xffff5220c0, "GPU"},
	{0xfffc0010c0, "GMAC0"},
	{0xfffc0020c0, "GMAC1"},
	{0xffffc200c0, "DMAC"},
	{0xffff2500c0, "TEE_DMAC"},
	{0xffff0580c0, "DSP0"},
	{0xffff0590c0, "DSP1"},
};

void th1520_iopmp_init(void)
{
	/* Use the active AP aliases for storage and VI/VO/DSP clocks. */
	setbits32((void *)0xffef010204ULL, 1U << 30);
	setbits32((void *)0xffef010220ULL, 7);
	mb();
	/* Release VP, VO, VI and DSP resets before accessing their policies. */
	setbits32((void *)0xffef014220ULL, 15);
	mb();
	/* VO bus and local DPU/GPU IOPMP APB/AXI gates. */
	setbits32((void *)0xffef528050ULL, (1U << 0) | (1U << 22) |
		(1U << 23) | (1U << 24) | (1U << 25) |
		(1U << 27) | (1U << 28) | (1U << 29));
	mb();
	for (size_t i = 0; i < ARRAY_SIZE(iopmp); i++) {
		printk(BIOS_DEBUG, "TH1520: configure %s IOPMP\n", iopmp[i].name);
		write32((void *)iopmp[i].reg, UINT32_MAX);
	}
	mb();
}
