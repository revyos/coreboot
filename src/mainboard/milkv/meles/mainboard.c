/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/device.h>

static void mainboard_init(void *chip_info)
{
	printk(BIOS_INFO, "Milk-V Meles: coreboot ramstage running\n");
}

struct chip_operations mainboard_ops = {
	.init = mainboard_init,
};
