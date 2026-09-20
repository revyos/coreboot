/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
#include <arch/barrier.h>
#include <arch/encoding.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/mmio.h>
#include <drivers/riscv/clint.h>
#include <string.h>

#define CLINT_MTIMECMP 0x4000
#define CLINT_MTIME 0xbff8

struct clint_quirks {
	bool clint_without_mtime;
	bool has_64bit_mmio;
};

static const struct clint_quirks sifive_clint_quirks = {
	.has_64bit_mmio = true,
};

/* Same IP distinction as OpenSBI's thead_clint_quirks. */
static const struct clint_quirks thead_clint_quirks = {
	.clint_without_mtime = true,
	.has_64bit_mmio = false,
};

static const struct {
	const char *compatible;
	const struct clint_quirks *quirks;
} clint_match[] = {
	{ "thead,c900-clint", &thead_clint_quirks },
	{ "riscv,clint0", &sifive_clint_quirks },
	{ "sifive,clint0", &sifive_clint_quirks },
	{ "sifive,clint2", &sifive_clint_quirks },
};

static const struct clint_quirks *clint_quirks(const struct riscv_clint *clint)
{
	for (size_t i = 0; i < ARRAY_SIZE(clint_match); i++) {
		if (!strcmp(clint->compatible, clint_match[i].compatible))
			return clint_match[i].quirks;
	}
	die("Unsupported CLINT compatible");
}

uint64_t riscv_clint_read_time(const struct riscv_clint *clint)
{
	const struct clint_quirks *quirks = clint_quirks(clint);
	void *mtime = (void *)(clint->base + CLINT_MTIME);

	if (quirks->clint_without_mtime) {
#if __riscv_xlen == 64
		return read_csr(time);
#else
		uint32_t hi, lo;

		do {
			hi = read_csr(timeh);
			lo = read_csr(time);
		} while (hi != read_csr(timeh));
		return ((uint64_t)hi << 32) | lo;
#endif
	}
	if (quirks->has_64bit_mmio && __riscv_xlen == 64)
		return read64(mtime);

	uint32_t hi, lo;

	do {
		hi = read32(mtime + 4);
		lo = read32(mtime);
	} while (hi != read32(mtime + 4));
	return ((uint64_t)hi << 32) | lo;
}

void riscv_clint_set_timer(const struct riscv_clint *clint,
	unsigned int hart, uint64_t when)
{
	const struct clint_quirks *quirks = clint_quirks(clint);
	void *cmp = (void *)(clint->base + CLINT_MTIMECMP + 8UL * hart);

	if (quirks->has_64bit_mmio && __riscv_xlen == 64) {
		write64(cmp, when);
		/* Finish programming the comparator before returning. */
		mb();
		return;
	}
	/* Keep a partial update from creating an earlier compare value. */
	write32(cmp, UINT32_MAX);
	/* Publish the low-word guard before changing the high word. */
	mb();
	write32(cmp + 4, when >> 32);
	/* Publish the high word before committing the final low word. */
	mb();
	write32(cmp, when);
	/* Finish programming the comparator before returning. */
	mb();
}

void riscv_clint_set_msip(const struct riscv_clint *clint,
	unsigned int hart, bool pending)
{
	/* Publish shared hart state before sending the IPI. */
	mb();
	write32((void *)(clint->base + 4UL * hart), pending);
	/* Complete the MMIO request before returning. */
	mb();
}
