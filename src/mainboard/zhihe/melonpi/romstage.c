/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/encoding.h>
#include <cbmem.h>
#include <console/console.h>
#include <console/uart.h>
#include <delay.h>
#include <device/mmio.h>
#include <romstage_common.h>
#include <program_loading.h>
#include <soc/addressmap.h>
#include <soc/early.h>
#include <soc/storage.h>
#include <timer.h>
#include "board.h"

static void run_stage(const char *name, int (*initialize)(void))
{
	struct mono_time start, end;

	timer_monotonic_get(&start);
	if (initialize())
		die("A210: initialization failed\n");
	timer_monotonic_get(&end);
	printk(BIOS_INFO, "A210: %s took %lld us\n", name,
	       (long long)mono_time_diff_microseconds(&start, &end));
}

static int initialize_ocram(void)
{
	if (a210_ocram_init())
		return -1;
	return CONFIG(A210_FIRMWARE) || CONFIG(A210_BOOTSTRAP)
		   ? 0
		   : a210_ocram_test();
}

#if CONFIG(A210_DDR_TEST)
static int initialize_dram(void)
{
	uint64_t size;

	if (a210_ddr_init(&melonpi_dram, &size))
		return -1;
	if (CONFIG(A210_FIRMWARE) && CONFIG(A210_BOOT_RAM))
		a210_stage_ram_firmware();
	a210_dram_enable();
	if (CONFIG(A210_FIRMWARE)) {
		write32((void *)(A210_DIAGNOSTIC_STATUS + 8), size >> 20);
		return 0;
	}
	struct mono_time start, end;

	timer_monotonic_get(&start);
	if (a210_ddr_test(size))
		return -1;
	timer_monotonic_get(&end);
	printk(BIOS_INFO, "A210: DDR memory test took %lld us\n",
	       (long long)mono_time_diff_microseconds(&start, &end));
	write32((void *)(A210_DIAGNOSTIC_STATUS + 8), size >> 20);
	return 0;
}
#endif

void __noreturn romstage_main(void)
{
	struct mono_time start, end;

	write32((void *)(A210_DIAGNOSTIC_STATUS + 4), 2);
	printk(BIOS_INFO, "A210: coreboot BRAM entry, hart %lu\n",
	       read_csr(mhartid));
	printk(BIOS_INFO, "A210: marchid=%#lx misa=%#lx\n",
	       read_csr(marchid), read_csr(misa));
	timer_monotonic_get(&start);
	udelay(1000);
	timer_monotonic_get(&end);
	printk(BIOS_INFO, "A210: timer advanced %lld us\n",
	       (long long)mono_time_diff_microseconds(&start, &end));
	run_stage("board identification", melonpi_check_board);
	run_stage("power request control", a210_power_control_init);
	run_stage("PMIC", melonpi_power_init);
	run_stage("peripheral power", melonpi_peripheral_power_init);
	run_stage("fan", melonpi_fan_init);
	/* Preserve the RAM image already downloaded into OCRAM. */
	if (!CONFIG(A210_FIRMWARE) || !CONFIG(A210_BOOT_RAM))
		run_stage("OCRAM", initialize_ocram);
	if (CONFIG(A210_BOOTSTRAP))
		a210_load_ddr_stage();
	if (CONFIG(A210_EMMC_TEST))
		a210_storage_test();
#if CONFIG(A210_DDR_TEST)
	run_stage("DDR", initialize_dram);
	printk(BIOS_INFO, "A210: DDR training complete\n");
	if (CONFIG(A210_FIRMWARE)) {
		cbmem_initialize_empty();
		printk(BIOS_INFO, "A210: loading ramstage from %s\n",
		       CONFIG(A210_BOOT_RAM) ? "RAM CBFS" : "eMMC boot0");
		run_ramstage();
	}
#else
	printk(BIOS_INFO, "A210: PMIC/OCRAM initialization complete\n");
#endif
	write32((void *)(A210_DIAGNOSTIC_STATUS + 4), 3);
	uart_tx_flush(0);
	for (;;)
		asm volatile("wfi");
}
