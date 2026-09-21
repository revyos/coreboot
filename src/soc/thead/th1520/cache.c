/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
#include <arch/encoding.h>
#include <program_loading.h>
#include <soc/cpu.h>

#if ENV_RAMSTAGE
void exit_car(void)
{
	/* Bootblock invalidated the caches and kept D-cache disabled through DDR. */
	asm volatile("fence rw, rw" ::: "memory");
	/* I/D cache, write allocation/back, return stack and branch prediction. */
	write_csr(0x7c1, 0x17f);
	asm volatile("fence.i" ::: "memory");
}
#endif

void th1520_clean_data_cache(void)
{
	if (read_csr(0x7c1) & 2)
		asm volatile(".long 0x0010000b; .long 0x0190000b" ::: "memory");
}

void platform_segment_loaded(uintptr_t start, size_t size, int flags)
{
	th1520_clean_data_cache();
	/* Bootblock writes are uncached and only need instruction synchronization. */
	asm volatile("fence.i" ::: "memory");
}
