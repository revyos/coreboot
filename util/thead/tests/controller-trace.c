/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "ddr_internal.h"

static uint32_t ctrl[0x2000 / 4], sys[8];
static bool started;

void write32(void *address, uint32_t value)
{
	uintptr_t addr = (uintptr_t)address;
	unsigned char bytes[12];

	for (unsigned int i = 0; i < 8; i++)
		bytes[i] = addr >> (8 * i);
	for (unsigned int i = 0; i < 4; i++)
		bytes[8 + i] = value >> (8 * i);
	assert(fwrite(bytes, sizeof(bytes), 1, stdout) == 1);
	if (addr >= TH1520_DDR_SYS_BASE && addr < TH1520_DDR_SYS_BASE + sizeof(sys)) {
		sys[(addr - TH1520_DDR_SYS_BASE) / 4] = value;
		return;
	}
	assert(addr >= TH1520_DDR_CTRL_BASE && addr < TH1520_DDR_CTRL_BASE + sizeof(ctrl));
	ctrl[(addr - TH1520_DDR_CTRL_BASE) / 4] = value;
	if (addr == TH1520_DDR_CTRL_BASE + 0x1b0 && value == 0x30)
		started = true;
}

uint32_t read32(const void *address)
{
	uintptr_t addr = (uintptr_t)address;
	const char *fail = getenv("FAIL_REG");

	if (fail && addr == strtoull(fail, NULL, 0))
		return 0xdeadbeef;
	if (addr == TH1520_DDR_SYS_BASE + 0x18)
		return 1;
	if (addr >= TH1520_DDR_SYS_BASE && addr < TH1520_DDR_SYS_BASE + sizeof(sys))
		return sys[(addr - TH1520_DDR_SYS_BASE) / 4];
	assert(addr >= TH1520_DDR_CTRL_BASE && addr < TH1520_DDR_CTRL_BASE + sizeof(ctrl));
	unsigned int reg = addr - TH1520_DDR_CTRL_BASE;

	if (reg == 4 || reg == 0x1b04)
		return started ? 1 : 0;
	if (reg == 0x1bc || reg == 0x1cbc || reg == 0x324)
		return 1;
	return ctrl[reg / 4];
}

void udelay(unsigned int us)
{
	assert(us == 2);
}

#ifndef VENDOR_TRACE
int main(int argc, char **argv)
{
	assert(argc == 2);
	uint64_t capacity = strtoull(argv[1], NULL, 0) * GiB;

	return ddr_controller_prepare(capacity) || ddr_controller_enable();
}
#endif
