/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <device/mmio.h>

/* Expected attribute registers from the mainline U-Boot policy table. */
static const uintptr_t expected[] = {
	0xfffc0280c0, 0xfffc0290c0, 0xfffc02a0c0, 0xfffc02e0c0,
	0xffffc210c0, 0xffffc220c0, 0xffffc370c0, 0xffff2200c0,
	0xffff2300c0, 0xffff2400c0, 0xfff40800c0, 0xfff40810c0,
	0xfff40820c0, 0xfff40830c0, 0xfffcc600c0, 0xfffcc610c0,
	0xfffcc620c0, 0xfffcc630c0, 0xffff01c0c0, 0xffff5200c0,
	0xffff5210c0, 0xffff5220c0, 0xfffc0010c0, 0xfffc0020c0,
	0xffffc200c0, 0xffff2500c0, 0xffff0580c0, 0xffff0590c0,
};
static uint32_t storage = 0x100, clock = 0x100, reset = 0x100;
static uint32_t vo = 0x20, seen;
static unsigned int barriers;
#define VO_GATES ((1U << 0) | (1U << 22) | (1U << 23) | \
	(1U << 24) | (1U << 25) | (1U << 27) | (1U << 28) | (1U << 29))

uint32_t read32(const void *address)
{
	switch ((uintptr_t)address) {
	case 0xffef010204: return storage;
	case 0xffef010220: return clock;
	case 0xffef014220: return reset;
	case 0xffef528050:
		assert(barriers == 2);
		assert(clock == 0x107 && reset == 0x10f);
		return vo;
	default: assert(0); return 0;
	}
}

void write32(void *address, uint32_t value)
{
	uintptr_t addr = (uintptr_t)address;

	switch (addr) {
	case 0xffef010204:
		assert(value == (0x100 | (1U << 30)));
		storage = value; return;
	case 0xffef010220:
		assert(value == 0x107); clock = value; return;
	case 0xffef014220:
		assert(barriers == 1);
		assert(clock == 0x107 && value == 0x10f);
		reset = value; return;
	case 0xffef528050:
		assert(clock == 0x107 && reset == 0x10f);
		assert(value == (VO_GATES | 0x20)); vo = value; return;
	}
	assert(storage == (0x100 | (1U << 30)));
	assert(clock == 0x107 && reset == 0x10f && vo == (VO_GATES | 0x20));
	assert(value == UINT32_MAX);
	assert(barriers == 3);
	for (unsigned int i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
		if (addr == expected[i]) {
			assert(!(seen & (1U << i)));
			seen |= 1U << i;
			return;
		}
	}
	assert(0);
}

static void setbits32(void *p, uint32_t bits)
{
	write32(p, read32(p) | bits);
}

void mb(void)
{
	barriers++;
}
#include "../../../src/soc/thead/th1520/iopmp.c"

int main(void)
{
	th1520_iopmp_init();
	assert(seen == (1U << 28) - 1);
	assert(barriers == 4);
	puts("PASS: all 28 IOPMP policies, clock/reset order and preserved bits");
}
