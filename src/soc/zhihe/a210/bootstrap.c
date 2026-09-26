/* SPDX-License-Identifier: GPL-2.0-only */
#include <cbfs.h>
#include <console/console.h>
#include <soc/addressmap.h>
#include <soc/storage.h>
#include <string.h>

extern char a210_transfer_start[], a210_transfer_end[];
void __noreturn a210_return_to_rom(void);

static void __noreturn ram_download(void)
{
	uintptr_t (*resolve)(unsigned int) = (void *)A210_BOOTROM_API;
	void (*use_ocram)(void) = (void *)resolve(2);
	void (*stage_ready)(void) = (void *)resolve(6);

	if ((uintptr_t)use_ocram >= A210_BOOTROM_SIZE || !use_ocram ||
	    (uintptr_t)stage_ready >= A210_BOOTROM_SIZE || !stage_ready)
		die("A210: BootROM RAM service unavailable\n");
	/* ROM exports: 2 selects OCRAM, 6 marks the stage ready. */
	use_ocram();
	stage_ready();
	printk(BIOS_INFO, "A210: OCRAM ready for BootROM RAM download\n");
	a210_return_to_rom();
}

void __noreturn a210_load_ddr_stage(void)
{
	if (CONFIG(A210_BOOT_RAM))
		ram_download();
	void *stage = (void *)A210_DDR_STAGING_BASE;
	void *transfer = (void *)A210_TRANSFER_CODE;
	size_t size =
	    cbfs_load("fallback/ddr", stage, A210_DDR_STAGE_LIMIT);

	if (!size || size < 4 || size % 4)
		die("A210: cannot load the DDR stage from eMMC boot0\n");
	printk(BIOS_INFO, "A210: eMMC boot0 DDR stage, %zu bytes\n", size);
	/* The loader must leave BRAM before overwriting its own code. */
	memcpy(transfer, a210_transfer_start,
	       a210_transfer_end - a210_transfer_start);
	asm volatile("fence iorw, iorw; fence.i" ::: "memory");
	void (*enter)(const void *, void *, size_t) = transfer;

	enter(stage, (void *)A210_DDR_ENTRY, size);
	__builtin_unreachable();
}
