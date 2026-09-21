/* SPDX-License-Identifier: GPL-2.0-only */
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <device/mmio.h>
#include <soc/addressmap.h>

uintptr_t cbmem_top_chipset(void)
{
	/* Keep firmware bookkeeping below the 32-bit DMA boundary. */
	return MIN(read64((void *)TH1520_DDR_SIZE_ADDR), 2ULL * GiB);
}
