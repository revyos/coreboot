/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017-2020 Alibaba Group Holding Limited
 * Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn>
 *
 * Vendor waitfwdone.c mailbox framing, with bounded polling and streaming
 * argument consumption instead of an unchecked fixed-size buffer.
 */
#include <console/console.h>
#include <timer.h>
#include "ddr_internal.h"

#define PMU_COMPLETE 0x07
#define PMU_STREAM 0x08
#define PMU_FAILED 0xff
#define PMU_TRAIN_TIMEOUT_MS 10000

static int mailbox_status(unsigned int phy, bool empty, struct stopwatch *limit)
{
	for (;;) {
		if (stopwatch_expired(limit))
			return -1;
		if (!!(ddr_phy_read(phy, PHY_MAIL_STATUS) & 1) == empty)
			return 0;
	}
}

static int mailbox_read(unsigned int phy, uint32_t *word, struct stopwatch *limit)
{
	if (mailbox_status(phy, false, limit))
		return -1;
	*word = ddr_phy_read(phy, PHY_MAIL_LOW);
	*word |= (uint32_t)ddr_phy_read(phy, PHY_MAIL_HIGH) << 16;
	ddr_phy_write(phy, PHY_MAIL_ACK, 0);
	if (mailbox_status(phy, true, limit))
		return -1;
	ddr_phy_write(phy, PHY_MAIL_ACK, 1);
	return 0;
}

int ddr_pmu_wait(unsigned int phy, unsigned int dimension)
{
	struct stopwatch limit;
	uint32_t word;

	stopwatch_init_msecs_expire(&limit, PMU_TRAIN_TIMEOUT_MS);
	for (;;) {
		if (mailbox_read(phy, &word, &limit))
			break;
		switch (word & 0xffff) {
		case PMU_COMPLETE:
			printk(BIOS_INFO, "DDR PHY%u %uD training complete\n",
				phy, dimension);
			return 0;
		case PMU_FAILED:
			printk(BIOS_ERR, "DDR PHY%u %uD training failed\n",
				phy, dimension);
			return -1;
		case PMU_STREAM:
			if (mailbox_read(phy, &word, &limit))
				goto timeout;
			unsigned int count = word & 0xffff;

			printk(BIOS_DEBUG, "DDR PHY%u: PMU message %#x, %u arguments\n",
				phy, word >> 16, count);
			/* Arguments may contain 0x07/0xff; they are not status codes. */
			for (unsigned int i = 0; i < count; i++) {
				if (mailbox_read(phy, &word, &limit))
					goto timeout;
			}
			break;
		default:
			break;
		}
	}
timeout:
	printk(BIOS_ERR, "DDR PHY%u %uD mailbox timeout\n", phy, dimension);
	return -1;
}
