/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <device/mmio.h>
#undef CONFIG
#define CONFIG(x) CONFIG_##x
#define CONFIG_TH1520_AON 0
#define CONFIG_TH1520_CPU_750MHZ 1

static uint32_t regs[0x104 / 4];
static unsigned int initial_pll, bus_limit, mux_writes;
static bool locked;

static unsigned int rate(uint32_t mux)
{
	unsigned int index = (mux & 1) ? 4 : 0;
	uint32_t cfg = regs[index];

	if ((!(mux & 1) && (mux & 2)) || (regs[index + 1] & (1U << 30)))
		return 24000000;
	return (uint64_t)24000000 * ((cfg >> 8) & 4095) /
		((cfg & 63) * ((cfg >> 20) & 7) * ((cfg >> 24) & 7));
}

uint32_t read32(const void *address)
{
	unsigned int off = (uintptr_t)address - 0xffef010000ULL;

	assert(off < sizeof(regs));
	if (off == 0x80)
		return locked ? 0x12 : 0;
	return regs[off / 4];
}

void write32(void *address, uint32_t value)
{
	unsigned int off = (uintptr_t)address - 0xffef010000ULL;

	assert(off == 0 || off == 4 || off == 0x10 || off == 0x14 || off == 0x100);
	if (off == 0x100) {
		unsigned int frequency = rate(value);

		assert(locked);
		assert(frequency / (((value >> 8) & 7) + 1) <= bus_limit);
		mux_writes++;
	} else {
		/* The PLL supplying the inherited CPU clock must not be reset. */
		assert(off / 0x10 != initial_pll);
	}
	regs[off / 4] = value;
}

static void setbits32(void *address, uint32_t set)
{
	write32(address, read32(address) | set);
}

static void clrbits32(void *address, uint32_t clear)
{
	write32(address, read32(address) & ~clear);
}

static void clrsetbits32(void *address, uint32_t clear, uint32_t set)
{
	write32(address, (read32(address) & ~clear) | set);
}

void udelay(unsigned int us) { (void)us; }
#include "../../../src/soc/thead/th1520/cpu_clock.c"

static void run(uint32_t mux, uint32_t cfg, bool lock, unsigned int mhz)
{
	memset(regs, 0, sizeof(regs));
	initial_pll = mux & 1;
	regs[0] = regs[4] = cfg;
	regs[1] = regs[5] = 0x03000000;
	regs[0x100 / 4] = mux;
	bus_limit = rate(mux) / (((mux >> 8) & 7) + 1);
	unsigned int target_bus = mhz == 1848 ? 616000000 : 150000000;
	if (bus_limit < target_bus)
		bus_limit = target_bus;
	locked = lock;
	mux_writes = 0;
	int ret = th1520_cpu_set_clock(mhz);
	assert(ret == (lock ? 0 : -1));
	assert(regs[initial_pll * 4] == cfg);
	if (lock) {
		assert(cpu_hz() == mhz * 1000000U);
		assert(((regs[0x100 / 4] >> 8) & 7) == (mhz == 1848 ? 2 : mhz == 750 ? 4 : 1));
		assert((regs[0x100 / 4] & 1) != initial_pll);
		assert(mux_writes);
	} else {
		assert(regs[0x100 / 4] == mux);
		assert(!mux_writes);
	}
}

int main(void)
{
	for (unsigned int parent = 0; parent < 2; parent++) {
		run(0x900 | parent, 0x02507d01, true, 750);  /* 300 MHz -> 750 MHz. */
		run(0xa00 | parent, 0x01207d01, true, 750);  /* 1500 MHz -> 750 MHz. */
		run(0x900 | parent, 0x02507d01, false, 750); /* Lock timeout. */
		run(0x900 | parent, 0x02507d01, true, 1848);
		run(0xa00 | parent, 0x01104d01, true, 300);
		run(0x900 | parent, 0x02507d01, false, 1848);
	}
	run(0x902, 0x02507d01, true, 750); /* Oscillator parent. */
	assert(th1520_cpu_set_clock(2000) == -1);
	memset(regs, 0, sizeof(regs));
	mux_writes = 0;
	assert(th1520_cpu_set_clock(1848) == -1 && !mux_writes);
	puts("PASS: CPU PLL parent selection, safe bus ordering and lock failure");
}
