/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <soc/storage.h>
#include <soc/addressmap.h>
#include <string.h>
#include <timer.h>

void a210_storage_test(void)
{
	uint8_t *data = (void *)A210_OCRAM_BASE;
	uint8_t *unaligned = data + 4096;
	struct stopwatch watch;

	if (a210_emmc_init())
		die("A210: eMMC initialization failed\n");
	for (unsigned int part = 0; part < 3; part++) {
		if (a210_emmc_read(part, 0, 4096, data) ||
		    a210_emmc_read(part, 511, 1025, unaligned) ||
		    memcmp(data + 511, unaligned, 1025))
			die("A210: eMMC partition read failed\n");
		printk(
		    BIOS_INFO,
		    "A210: eMMC partition %u, %llu KiB, head "
		    "%02x%02x%02x%02x\n",
		    part,
		    (unsigned long long)(a210_emmc_capacity(part) >> 10),
		    data[0], data[1], data[2], data[3]);
	}
	stopwatch_init(&watch);
	for (unsigned int i = 0; i < 16; i++)
		if (a210_emmc_read(1, i * 65536, 65536, data))
			die("A210: eMMC boot0 sequential read failed\n");
	printk(BIOS_INFO, "A210: eMMC boot0 read 1 MiB in %lld ms\n",
	       (long long)stopwatch_duration_msecs(&watch));
}
