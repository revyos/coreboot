/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <crc_byte.h>
#include <device/mmio.h>
#include <soc/storage.h>
#include <string.h>

static uint8_t block[4096] __aligned(4);
static uint8_t partial[1537] __aligned(4);

static uint32_t crc(const uint8_t *data, size_t size)
{
	uint32_t value = 0;

	for (size_t i = 0; i < size; i++)
		value = crc32_byte(value, data[i]);
	return value;
}

void th1520_storage_test(void)
{
	uint32_t boot_crc;

	printk(BIOS_INFO, "TH1520: read-only SPI/eMMC checks, BootROM OM=%u\n",
		read32((void *)0xffef018010) & 7);
	if (th1520_spi_read(0, sizeof(block), block))
		die("SPI read test failed");
	printk(BIOS_INFO, "SPI first 4 KiB CRC32-BE: %08x\n", crc(block, sizeof(block)));
	if (th1520_spi_read(37, 1536, partial + 1) || memcmp(partial + 1, block + 37, 1536))
		die("SPI unaligned read differs");
	if (th1520_emmc_read(1, 0, sizeof(block), block))
		die("eMMC boot0 read test failed");
	boot_crc = crc(block, sizeof(block));
	printk(BIOS_INFO, "eMMC boot0 first 4 KiB CRC32-BE: %08x\n", boot_crc);
	if (th1520_emmc_read(1, 37, 1536, partial + 1) || memcmp(partial + 1, block + 37, 1536))
		die("eMMC unaligned read differs");
	if (th1520_emmc_read(0, 0, sizeof(block), block))
		die("eMMC user read test failed");
	printk(BIOS_INFO, "eMMC user first 4 KiB CRC32-BE: %08x\n", crc(block, sizeof(block)));
	if (th1520_emmc_read(1, 0, sizeof(block), block) || crc(block, sizeof(block)) != boot_crc)
		die("eMMC partition switch readback differs");
	printk(BIOS_INFO, "TH1520: SPI/eMMC read and partition-switch checks passed\n");
}
