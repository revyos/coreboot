/* SPDX-License-Identifier: GPL-2.0-only */
#include <cbmem.h>
#include <console/console.h>
#include <device/mmio.h>
#include <program_loading.h>
#include <romstage_common.h>
#include <soc/addressmap.h>
#include <soc/ddr.h>

void __noreturn romstage_main(void)
{
	uint64_t size = th1520_ddr_init();

	th1520_ddr_test(size);
	write64((void *)TH1520_DDR_SIZE_ADDR, size);
	cbmem_initialize_empty();
	printk(BIOS_INFO, "TH1520: loading ramstage from SRAM CBFS\n");
	run_ramstage();
}
