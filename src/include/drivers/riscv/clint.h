/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DRIVERS_RISCV_CLINT_H
#define DRIVERS_RISCV_CLINT_H

#include <types.h>

struct riscv_clint {
	const char *compatible;
	uintptr_t base;
};

uint64_t riscv_clint_read_time(const struct riscv_clint *clint);
void riscv_clint_set_timer(const struct riscv_clint *clint,
	unsigned int hart, uint64_t when);
void riscv_clint_set_msip(const struct riscv_clint *clint,
	unsigned int hart, bool pending);

#endif
