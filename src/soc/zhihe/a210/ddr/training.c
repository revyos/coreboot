/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2024 Zhihe Computing Technology (Shenzhen) Co., Ltd. */
#include "ddr.h"
#include <console/console.h>
#include <device/mmio.h>
#include <timer.h>

static uintptr_t phy_reg(unsigned int channel, uint32_t reg)
{
	return A210_DDR_PHY_BASE + channel * A210_DDR_CHANNEL_STRIDE +
	       2 * reg;
}

void a210_phy_write_both(uint32_t reg, uint16_t value)
{
	for (unsigned int ch = 0; ch < 2; ch++)
		write16((void *)phy_reg(ch, reg), value);
}

static int mailbox(unsigned int channel, struct stopwatch *deadline,
		   uint32_t *message)
{
	while (read16((void *)phy_reg(channel, PHY_UCT_STATUS)) & 1)
		if (stopwatch_expired(deadline))
			return -1;
	*message = read16((void *)phy_reg(channel, PHY_UCT_MESSAGE));
	*message |=
	    (uint32_t)read16((void *)phy_reg(channel, PHY_UCT_DATA)) << 16;
	write16((void *)phy_reg(channel, PHY_UCT_ACK), 0);
	while (!(read16((void *)phy_reg(channel, PHY_UCT_STATUS)) & 1))
		if (stopwatch_expired(deadline))
			return -1;
	write16((void *)phy_reg(channel, PHY_UCT_ACK), 1);
	return 0;
}

static int wait_channel(unsigned int channel)
{
	struct stopwatch deadline;
	uint32_t message;

	stopwatch_init_msecs_expire(&deadline, 10000);
	while (!stopwatch_expired(&deadline)) {
		if (mailbox(channel, &deadline, &message))
			return -1;
		switch (message & 0xffff) {
		case 7:
			return 0;
		case 0xff:
			printk(BIOS_ERR, "A210: PHY%u training failed\n",
			       channel);
			return -1;
		case 8:
			if (mailbox(channel, &deadline, &message))
				return -1;
			unsigned int count = message & 0xffff;

			/* Drain messages without a fixed-size buffer. */
			for (unsigned int i = 0; i < count; i++)
				if (mailbox(channel, &deadline, &message))
					return -1;
			break;
		}
	}
	return -1;
}

int a210_ddr_wait_training(unsigned int two_dimensional)
{
	for (unsigned int ch = 0; ch < 2; ch++) {
		if (wait_channel(ch)) {
			printk(BIOS_ERR,
			       "A210: PHY%u %uD training incomplete\n", ch,
			       two_dimensional + 1);
			return -1;
		}
		printk(BIOS_INFO, "A210: PHY%u %uD training passed\n", ch,
		       two_dimensional + 1);
	}
	return 0;
}
