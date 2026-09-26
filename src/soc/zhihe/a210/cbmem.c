/* SPDX-License-Identifier: GPL-2.0-only */
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <device/mmio.h>
#include <soc/addressmap.h>

uintptr_t cbmem_top_chipset(void)
{
	uint64_t size = (uint64_t)read32(
		(void *)(A210_DIAGNOSTIC_STATUS + 8)) << 20;

	/* Firmware bookkeeping remains in the first 2 GiB of DRAM. */
	return (uint64_t)A210_DRAM_BASE + MIN(size, 2ULL * GiB);
}
