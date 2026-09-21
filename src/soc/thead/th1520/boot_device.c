/* SPDX-License-Identifier: GPL-2.0-only */
#include <boot_device.h>
#include <console/console.h>
#include <soc/addressmap.h>
#include <string.h>

static bool initialized;
static const struct mem_region_device ram =
	MEM_REGION_DEV_RO_INIT(TH1520_RAM_ROM_BASE, CONFIG_ROM_SIZE);

void boot_device_init(void)
{
	if (initialized)
		return;
	if (ENV_BOOTBLOCK) {
		/* First CBFS access follows DDR training; ramstage reuses this copy. */
		void *image = (void *)TH1520_RAM_ROM_BASE;
		const void *download = (void *)TH1520_SRAM_BASE;

		memcpy(image, download, CONFIG_ROM_SIZE);
		if (memcmp(image, download, CONFIG_ROM_SIZE))
			die("TH1520: RAM image staging verification failed");
		printk(BIOS_INFO, "TH1520: RAM download staged at %p (%u bytes)\n",
			image, CONFIG_ROM_SIZE);
	}
	initialized = true;
}

const struct region_device *boot_device_ro(void)
{
	boot_device_init();
	return &ram.rdev;
}
