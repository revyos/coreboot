/* SPDX-License-Identifier: GPL-2.0-only */
#include <boot_device.h>
#include <cbfs.h>
#include <console/console.h>
#include <soc/addressmap.h>
#include <soc/storage.h>
#include <string.h>

void a210_stage_ram_firmware(void)
{
	void *destination = (void *)A210_RAM_ROM_BASE;
	const void *source = (void *)A210_RAM_IMAGE_BASE;

	memcpy(destination, source, CONFIG_ROM_SIZE);
	if (memcmp(destination, source, CONFIG_ROM_SIZE))
		die("A210: RAM firmware staging verification failed\n");
	printk(BIOS_INFO, "A210: RAM firmware staged at %p\n",
		destination);
}

#if CONFIG(A210_BOOT_RAM)
static const struct mem_region_device ram = MEM_REGION_DEV_RO_INIT(
	ENV_BOOTBLOCK && CONFIG(A210_BOOTSTRAP) ?
	A210_RAM_IMAGE_BASE : A210_RAM_ROM_BASE, CONFIG_ROM_SIZE);

const struct region_device *boot_device_ro(void)
{
	return &ram.rdev;
}
#elif CONFIG(A210_FIRMWARE) || CONFIG(A210_BOOTSTRAP)
static ssize_t readat(const struct region_device *rd, void *dest,
		      size_t offset, size_t size)
{
	return a210_emmc_read(1, A210_ROM_OFFSET + offset, size, dest) ?
		-1 : (ssize_t)size;
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
	if (a210_emmc_capacity(1) < A210_ROM_OFFSET + CONFIG_ROM_SIZE)
		die("A210: eMMC boot0 is unavailable or too small\n");
}

const struct region_device *boot_device_ro(void)
{
	boot_device_init();
	return &storage.rdev;
}
#else
static const struct mem_region_device bram =
	MEM_REGION_DEV_RO_INIT(A210_BRAM_BASE, 128 * 1024);

const struct region_device *boot_device_ro(void)
{
	return &bram.rdev;
}
#endif
