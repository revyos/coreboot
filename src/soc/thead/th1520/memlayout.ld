/* SPDX-License-Identifier: GPL-2.0-only */
#include <memlayout.h>
#include <arch/header.ld>
#include <soc/addressmap.h>

SECTIONS
{
	DRAM_START(0)
	RAMSTAGE(16M, 2M)
	MEM_STACK(18M, 16K)
	/* Accessed only after native DDR initialization. */
	CBFS_CACHE(20M, 2M)
	REGION(ram_rom, TH1520_RAM_ROM_BASE, CONFIG_ROM_SIZE, 4K)

	SRAM_START(TH1520_SRAM_BASE)
	BOOTBLOCK(TH1520_SRAM_BASE, 256K)
	/* The complete 1 MiB RAM image stays below the live SRAM state. */
	CAR_STACK(TH1520_SRAM_BASE + 1M, 16K)
	PRERAM_CBMEM_CONSOLE(TH1520_SRAM_BASE + 1M + 16K, 16K)
	FMAP_CACHE(TH1520_SRAM_BASE + 1M + 32K, 2K)
	CBFS_MCACHE(TH1520_SRAM_BASE + 1M + 34K, 8K)
	REGION(ddr_size, TH1520_DDR_SIZE_ADDR, 8, 8)
	SRAM_END(TH1520_SRAM_BASE + 1152K)
}
