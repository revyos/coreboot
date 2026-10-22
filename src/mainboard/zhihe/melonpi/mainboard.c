/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/device.h>
#include <soc/e902.h>
#include <soc/clock.h>
#include <soc/early.h>
#include "board.h"

#define MELONPI_CPU_BOOT_MHZ 1896

static void mainboard_init(void *unused)
{
	printk(BIOS_INFO, "Melon Pi: coreboot ramstage running\n");
	if (a210_peri1_dma_init())
		die("Melon Pi: PERI1 DMA access setup failed\n");
	if (a210_pcie_dma_init())
		die("Melon Pi: PCIe DMA access setup failed\n");
	if (a210_usb_dma_init())
		die("Melon Pi: USB DMA access setup failed\n");
	if (a210_display_init())
		printk(BIOS_WARNING,
		       "Melon Pi: display domain unavailable\n");
	if (melonpi_secondary_power_init())
		die("Melon Pi: C920 supply initialization failed\n");
	if (melonpi_performance_power_init())
		die("Melon Pi: CPU voltage initialization failed\n");
	if (a210_prepare_harts())
		die("Melon Pi: secondary hart preparation failed\n");
	if (a210_set_cpu_clock(0, MELONPI_CPU_BOOT_MHZ) ||
	    a210_set_cpu_clock(1, MELONPI_CPU_BOOT_MHZ))
		die("Melon Pi: CPU clock initialization failed\n");
	/* E902 adopts the initialized resources and owns subsequent changes. */
	if (a210_e902_start())
		die("Melon Pi: SCMI firmware startup failed\n");
}

struct chip_operations mainboard_ops = {
	.init = mainboard_init,
};
