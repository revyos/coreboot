/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/encoding.h>
#include <drivers/riscv/clint.h>
#include <mcall.h>
#include <soc/addressmap.h>
#include <timer.h>

/* SoC binding; the driver matches the same compatible as the OS DTB. */
static const struct riscv_clint clint = {
	.compatible = "thead,c900-clint",
	.base = TH1520_CLINT_BASE,
};

void mtime_init(void)
{
	/* T-Head CLINT has comparators, but no memory-mapped MTIME. */
	HLS()->time = NULL;
	HLS()->timecmp = (void *)(TH1520_CLINT_BASE + 0x4000 +
		8 * read_csr(mhartid));
}

void mtime_set(uint64_t when)
{
	riscv_clint_set_timer(&clint, read_csr(mhartid), when);
}

void set_msip(int hartid, int value)
{
	riscv_clint_set_msip(&clint, hartid, !!value);
}

void timer_monotonic_get(struct mono_time *mt)
{
	/* The TH1520 system counter advances at 3 MHz. */
	mono_time_set_usecs(mt, riscv_clint_read_time(&clint) / 3);
}
