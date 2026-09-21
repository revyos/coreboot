/* SPDX-License-Identifier: GPL-2.0-only */
#include <boot_device.h>
#include <cbfs.h>
#include <console/console.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/storage.h>
#include <string.h>
#include <timer.h>

static unsigned int source;
static bool initialized;
static const char *const source_names[] = { "RAM download", "SPI NOR", "eMMC boot0" };
static const struct mem_region_device ram =
	MEM_REGION_DEV_RO_INIT(TH1520_RAM_ROM_BASE, CONFIG_ROM_SIZE);

static ssize_t readat(const struct region_device *rd, void *dest, size_t offset, size_t size)
{
	struct stopwatch watch;

	stopwatch_init(&watch);
	int ret = source == TH1520_SOURCE_SPI ?
		th1520_spi_read(offset, size, dest) : th1520_emmc_read(1, offset, size, dest);

	if (size >= 64 * KiB)
		printk(BIOS_DEBUG, "TH1520: %s read %zu bytes in %lld ms\n",
			source_names[source], size, (long long)stopwatch_duration_msecs(&watch));
	return ret ? -1 : (ssize_t)size;
}

static const struct region_device_ops ops = {
	.mmap = mmap_helper_rdev_mmap,
	.munmap = mmap_helper_rdev_munmap,
	.readat = readat,
};
static struct mmap_helper_region_device storage =
	MMAP_HELPER_DEV_INIT(&ops, 0, CONFIG_ROM_SIZE, &cbfs_cache);

void boot_device_init(void)
{
	if (initialized)
		return;
	if (CONFIG(TH1520_BOOT_RAM))
		source = TH1520_SOURCE_RAM;
	else if (CONFIG(TH1520_BOOT_SPI))
		source = TH1520_SOURCE_SPI;
	else if (CONFIG(TH1520_BOOT_EMMC))
		source = TH1520_SOURCE_EMMC;
	else {
		/* Vendor board_boot_order(): OM[2:0] encodes the ROM boot medium. */
		unsigned int mode = read32((void *)0xffef018010) & 7;

		if (mode < 4)
			source = TH1520_SOURCE_RAM;
		else if (mode == 4)
			source = TH1520_SOURCE_EMMC;
		else if (mode == 7)
			source = TH1520_SOURCE_SPI;
		else
			die("TH1520: unsupported BootROM medium");
	}
	if (ENV_BOOTBLOCK && source == TH1520_SOURCE_RAM) {
		/* First CBFS access follows DDR training; ramstage reuses this copy. */
		void *image = (void *)TH1520_RAM_ROM_BASE;
		const void *download = (void *)TH1520_SRAM_BASE;

		memcpy(image, download, CONFIG_ROM_SIZE);
		if (memcmp(image, download, CONFIG_ROM_SIZE))
			die("TH1520: RAM image staging verification failed");
		printk(BIOS_INFO, "TH1520: RAM download staged at %p (%u bytes)\n",
			image, CONFIG_ROM_SIZE);
	}
	if (source == TH1520_SOURCE_SPI && th1520_spi_init())
		die("TH1520: SPI NOR initialization failed");
	if (source == TH1520_SOURCE_EMMC &&
	    th1520_emmc_capacity(1) < CONFIG_ROM_SIZE)
		die("TH1520: eMMC boot0 unavailable or too small");
	printk(BIOS_INFO, "TH1520: CBFS source %s\n", source_names[source]);
	initialized = true;
}

const struct region_device *boot_device_ro(void)
{
	boot_device_init();
	return source == TH1520_SOURCE_RAM ? &ram.rdev : &storage.rdev;
}
