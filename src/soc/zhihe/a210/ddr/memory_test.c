/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/mmio.h>
#include <soc/dram.h>
#include <soc/addressmap.h>

static uint32_t memory_pattern(uintptr_t address, unsigned int pass)
{
	return address ^ (address >> 32) ^
	       (pass ? 0xffffffff : 0x5a5aa5a5);
}

int a210_ddr_test(uint64_t size)
{
	const uintptr_t base = A210_DRAM_BASE;
	uintptr_t probes[64];
	unsigned int count = 0;

	if (size < (1ULL << 20) || size > (16ULL << 30))
		return -1;

	probes[count++] = base;
	for (uint64_t bit = 4; bit < size; bit <<= 1)
		probes[count++] = base + bit;
	probes[count++] = base + size - 4;
	for (unsigned int pass = 0; pass < 2; pass++) {
		for (unsigned int i = 0; i < count; i++)
			write32((void *)probes[i],
				memory_pattern(probes[i], pass));
		asm volatile("fence iorw, iorw" ::: "memory");
		for (unsigned int i = 0; i < count; i++) {
			if (read32((void *)probes[i]) !=
			    memory_pattern(probes[i], pass)) {
				printk(BIOS_ERR,
				       "A210: DDR address alias at %#lx\n",
				       probes[i]);
				return -1;
			}
		}
		for (uintptr_t addr = base; addr < base + 0x100000;
		     addr += 4)
			write32((void *)addr, memory_pattern(addr, pass));
		asm volatile("fence iorw, iorw" ::: "memory");
		for (uintptr_t addr = base; addr < base + 0x100000;
		     addr += 4)
			if (read32((void *)addr) !=
			    memory_pattern(addr, pass))
				return -1;
	}
	printk(BIOS_INFO,
	       "A210: %llu MiB DDR, address and 1 MiB data tests passed\n",
	       (unsigned long long)(size >> 20));
	return 0;
}
