/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017-2020 Alibaba Group Holding Limited
 * Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn>
 *
 * Cold-start stages from vendor lpddr4_init.c and boundary probing from
 * spl.c. No retention-state handoff is needed for this cold-boot path.
 */
#include <arch/barrier.h>
#include <console/console.h>
#include <soc/ddr.h>
#include "ddr_internal.h"

static int train_capacity(uint64_t capacity)
{
	printk(BIOS_INFO, "TH1520: LPDDR4X-3733 dual-rank, %llu GiB training map\n",
		(unsigned long long)(capacity / GiB));
	if (ddr_controller_prepare(capacity) || ddr_phy_train() ||
	    ddr_controller_enable())
		return -1;

	/* The BootROM DRAM protection stays in place until training completes. */
	write32((void *)0xffdc020000, 0);
	asm volatile ("fence.i" ::: "memory");
	return 0;
}

static bool distinct_addresses(uintptr_t first, uintptr_t second)
{
	uint32_t saved_first = read32((void *)first);
	uint32_t saved_second = read32((void *)second);
	bool distinct;

	write32((void *)first, 0x55aa33cc);
	write32((void *)second, 0xaa55cc33);
	/* Finish both stores before checking for aliased physical addresses. */
	mb();
	distinct = read32((void *)first) == 0x55aa33cc &&
		read32((void *)second) == 0xaa55cc33;
	write32((void *)second, saved_second);
	write32((void *)first, saved_first);
	/* Restore probe locations before changing the address-map candidate. */
	mb();
	return distinct;
}

static bool capacity_matches(uint64_t capacity)
{
	/* Like vendor boundary_verify(), first check quarter/half boundaries. */
	if (!distinct_addresses(0x10000, 0x10000 + capacity / 4) ||
	    !distinct_addresses(0x10000, 0x10000 + capacity / 2))
		return false;
	for (uint64_t offset = 4; offset < capacity; offset <<= 1) {
		if (!distinct_addresses(0x10000, 0x10000 + offset))
			return false;
	}
	return true;
}

uint64_t th1520_ddr_init(void)
{
	/* The 16 GiB map trains both ranks before probing their capacity. */
	if (train_capacity(16ULL * GiB))
		die("TH1520 DDR initialization failed");
	if (capacity_matches(16ULL * GiB)) {
		printk(BIOS_INFO, "TH1520: 16 GiB DDR detected\n");
		return 16ULL * GiB;
	}

	/* Vendor U-Boot changes the controller map without retraining PHY. */
	printk(BIOS_INFO, "TH1520: probing 8 GiB DDR address map\n");
	if (ddr_controller_set_capacity(8ULL * GiB) ||
	    !capacity_matches(8ULL * GiB))
		die("TH1520: unsupported DDR capacity or address map");
	printk(BIOS_INFO, "TH1520: 8 GiB DDR detected\n");
	return 8ULL * GiB;
}

void th1520_ddr_test(uint64_t size)
{
	for (uint64_t bank = 0; bank < size; bank += GiB) {
		uintptr_t start = bank + 0x10000;

		for (unsigned int pass = 0; pass < 2; pass++) {
			for (unsigned int offset = 0; offset < 4096; offset += 4) {
				uint32_t value = (start + offset) ^ (bank >> 32) ^ 0x55aa33cc;

				write32((void *)(start + offset), pass ? ~value : value);
			}
			/* Ensure verification reads observe the completed pattern. */
			mb();
			for (unsigned int offset = 0; offset < 4096; offset += 4) {
				uint32_t value = (start + offset) ^ (bank >> 32) ^ 0x55aa33cc;

				if (read32((void *)(start + offset)) != (pass ? ~value : value))
					die("TH1520 DDR bank sample test failed");
			}
		}
	}
	printk(BIOS_INFO, "TH1520: DDR bank sample test passed\n");
}
