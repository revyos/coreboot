/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "ddr_internal.h"

static unsigned int wait_count;

static void event(unsigned int phy, uint32_t word, uint16_t value)
{
	unsigned char bytes[7] = {
		phy, word, word >> 8, word >> 16, word >> 24, value, value >> 8,
	};

	assert(fwrite(bytes, sizeof(bytes), 1, stdout) == 1);
}

void write16(void *address, uint16_t value)
{
	uintptr_t addr = (uintptr_t)address;
	unsigned int phy = addr >= TH1520_DDR_PHY1_BASE;

	addr -= phy ? TH1520_DDR_PHY1_BASE : TH1520_DDR_PHY0_BASE;
	assert(!(addr & 1) && addr < 0x1000000);
	event(phy, addr / 2, value);
}

uint32_t read32(const void *address)
{
	assert((uintptr_t)address == TH1520_DDR_SYS_BASE);
	return 0;
}

int ddr_pmu_wait(unsigned int phy, unsigned int dimension)
{
	const char *fail = getenv("FAIL_WAIT");

	event(2, phy, dimension);
	wait_count++;
	return fail && wait_count == strtoul(fail, NULL, 0) ? -1 : 0;
}

int main(void)
{
	return ddr_phy_train() ? 1 : 0;
}
