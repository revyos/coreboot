/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/encoding.h>
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/early.h>
#include <soc/cpu.h>
#include <soc/ddr_regs.h>
#include <soc/ncore.h>
#include <soc/power.h>
#include <timer.h>

#define CI_BASE A210_NCORE_BASE
#define DDR_PCU A210_DDR_PCU_BASE
#define SLC_DUAL A210_SLC_DUAL_BASE

static int wait_mask(uintptr_t address, uint32_t mask, uint32_t expected)
{
	struct stopwatch deadline;

	stopwatch_init_msecs_expire(&deadline, 100);
	while ((read32((void *)address) & mask) != expected) {
		if (stopwatch_expired(&deadline)) {
			printk(BIOS_ERR, "A210: timeout at %#lx\n",
			       address);
			return -1;
		}
	}
	return 0;
}

static void ci_region(uintptr_t port, unsigned int region, uint32_t base,
		      uint32_t attributes)
{
	write32((void *)(port + NCORE_REGION_BASE_LOW(region)), base);
	write32((void *)(port + NCORE_REGION_BASE_HIGH(region)), 0);
	write32((void *)(port + NCORE_REGION_CONTROL(region)), attributes);
}

static int interconnect_init(void)
{
	static const unsigned int ports[] = {0, 1, 4, 5, 6};

	/* Single-die, two DDR channels, 256-byte interleaving. */
	clrsetbits32((void *)SYSREG_DFMU_CONFIG,
		     DFMU_DDR_WIDTH_INTERLEAVE_MASK, DFMU_DDR_DUAL_256);
	for (unsigned int i = 0; i < ARRAY_SIZE(ports); i++) {
		uintptr_t port = CI_BASE + NCORE_PORT_STRIDE * ports[i];

		write32((void *)(port + NCORE_INTERLEAVE),
			NCORE_INTERLEAVE_256);
		write32((void *)(port + NCORE_ADDRESS_MAP_MODE),
			NCORE_MAP_DUAL_CHANNEL);
		ci_region(port, 0, NCORE_REGION_IO_LOW_BASE,
			  NCORE_REGION_IO_512M);
		ci_region(port, 1, NCORE_REGION_IO_HIGH_BASE,
			  NCORE_REGION_IO_256M);
		ci_region(port, 2, NCORE_REGION_OCRAM_BASE,
			  NCORE_REGION_OCRAM);
		ci_region(port, 9, NCORE_REGION_REMOTE_BASE,
			  NCORE_REGION_IO_32G);
		write32((void *)(port + NCORE_SNOOP_FILTER0),
			ports[i] < 2 ? NCORE_CPU_SNOOP_FILTER
				     : NCORE_ALL_SNOOP_FILTER);
		write32((void *)(port + NCORE_SNOOP_FILTER1),
			ports[i] < 2 ? NCORE_CPU_SNOOP_FILTER
				     : NCORE_ALL_SNOOP_FILTER);
		write32((void *)(port + NCORE_SNOOP_FILTER2),
			NCORE_ALL_SNOOP_FILTER);
	}
	for (unsigned int port = 0; port <= 12; port++) {
		if (port == 2 || port == 3)
			continue;
		uintptr_t base = CI_BASE + NCORE_PORT_STRIDE * port;

		if (port == 10 || port == 11) {
			write32((void *)(base + NCORE_PORT_DVM_ACCESS),
				NCORE_PORT_ACCESS_ALL);
			continue;
		}
		bool reverse = port == 5 || port == 6 || port == 12;

		write32((void *)(base + (reverse ? NCORE_PORT_ACCESS1
						 : NCORE_PORT_ACCESS0)),
			NCORE_PORT_ACCESS_ALL);
		write32((void *)(base + (reverse ? NCORE_PORT_ACCESS0
						 : NCORE_PORT_ACCESS1)),
			NCORE_PORT_ENABLE);
	}
	setbits32((void *)(CI_BASE + NCORE_XAIUTCR), NCORE_ATTACH_REQUEST);
	return wait_mask(CI_BASE + NCORE_XAIUTAR, NCORE_ATTACH_ACK,
			 NCORE_ATTACH_ACK);
}

static int ddr_domain_on(void)
{
	struct stopwatch deadline;
	uint32_t status;

	/* BPC hardware mode and its reset-state sleep signals. */
	write32((void *)(DDR_PCU + DDR_BPC_OFFSET + BPC_SW_CONTROL),
		BPC_HARDWARE_CONTROL);
	write32((void *)(DDR_PCU + DDR_BPC_OFFSET + BPC_SW_SLEEP_USER),
		DDR_BPC_INITIAL_SIGNALS);
	write32((void *)(DDR_PCU + PCU_INTERRUPT_CLEAR),
		PCU_INTERRUPTS_ALL);
	write32((void *)(DDR_PCU + PCU_INTERRUPT_ENABLE),
		PCU_INTERRUPTS_ALL);
	/* A210 AON SDK defines PCU ON as 0x1f, OFF as zero. */
	write32((void *)(DDR_PCU + PCU_SW_STATE), PCU_POWER_ON);
	write32((void *)(DDR_PCU + PCU_SW_REQUEST), PCU_REQUEST);
	stopwatch_init_msecs_expire(&deadline, 100);
	do {
		status = read32((void *)(DDR_PCU + PCU_RAW_STATUS));
		if (stopwatch_expired(&deadline))
			return -1;
	} while (!status);
	write32((void *)(DDR_PCU + PCU_INTERRUPT_CLEAR), status);
	if (status != PCU_REQUEST_ACCEPT) {
		printk(BIOS_ERR, "A210: DDR power request status %#x\n",
		       status);
		return -1;
	}
	return 0;
}

static int slc_sram_init(unsigned int channel)
{
	uintptr_t core =
	    A210_SLC_CORE_BASE + channel * A210_DDR_CHANNEL_STRIDE;
	uintptr_t sysreg =
	    A210_SLC_SYSREG_BASE + channel * A210_DDR_CHANNEL_STRIDE;

	setbits32((void *)(SLC_DUAL + DDR_CLOCK_CONTROL(channel)),
		  DDR_SOFT_RESET_N);
	setbits32((void *)(SLC_DUAL + SLC_CLOCK_CONTROL(channel)),
		  SLC_AXI_CLOCK_ENABLE);
	setbits32((void *)(SLC_DUAL + SLC_CLOCK_CONTROL(channel)),
		  SLC_SOFT_RESET_N);
	write32((void *)(sysreg + SLC_SYSTEM_RESET), SLC_WRAP_RESET_N);
	setbits32((void *)(sysreg + SLC_SYSTEM_CONFIG),
		  SLC_CSR_WRITE_ENABLE | SLC_PATH_ENABLE);
	/* Initialize tag and data RAM before enabling SRAM mode. */
	write32((void *)(core + SLC_INIT_COMMAND), SLC_INIT_TAG);
	if (wait_mask(core + SLC_INIT_STATUS, SLC_INIT_BUSY, 0))
		return -1;
	write32((void *)(core + SLC_INIT_COMMAND), SLC_INIT_DATA);
	if (wait_mask(core + SLC_INIT_STATUS, SLC_INIT_BUSY, 0))
		return -1;
	write32((void *)(core + SLC_SRAM_ADDRESS_MASK),
		SLC_SRAM_INDEX_MASK);
	write32((void *)(core + SLC_SRAM_ADDRESS_BASE), 0);
	write32((void *)(core + SLC_SRAM_WAYS), SLC_SRAM_ALL_WAYS);
	write32((void *)(core + SLC_SRAM_CONTROL), SLC_SRAM_ENABLE);
	write32((void *)(core + SLC_TRANSACTION_ENABLE), 1);
	return 0;
}

int a210_ocram_test(void)
{
	/* Check SRAM through the interconnect, rather than a CPU cache. */
	if (a210_read_csr(A210_CSR_MHCR) & A210_MHCR_DCACHE_ENABLE) {
		printk(BIOS_ERR,
		       "A210: OCRAM test requires D-cache disabled\n");
		return -1;
	}
	for (unsigned int pass = 0; pass < 2; pass++) {
		uint32_t mask = pass ? 0xffffffff : 0;

		for (uintptr_t addr = A210_OCRAM_BASE;
		     addr < A210_OCRAM_BASE + A210_OCRAM_SIZE; addr += 4)
			write32((void *)addr, addr ^ mask);
		asm volatile("fence iorw, iorw" ::: "memory");
		for (uintptr_t addr = A210_OCRAM_BASE;
		     addr < A210_OCRAM_BASE + A210_OCRAM_SIZE; addr += 4) {
			uint32_t value = read32((void *)addr);

			if (value != (uint32_t)(addr ^ mask)) {
				printk(
				    BIOS_ERR,
				    "A210: OCRAM mismatch at %#lx: %#x\n",
				    addr, value);
				return -1;
			}
		}
	}
	printk(BIOS_INFO,
	       "A210: OCRAM 2 MiB address-pattern test passed\n");
	return 0;
}

int a210_ocram_init(void)
{
	printk(BIOS_INFO, "A210: configuring OCRAM interconnect\n");
	if (interconnect_init())
		return -1;
	printk(BIOS_INFO, "A210: enabling DDR/SLC power domain\n");
	if (ddr_domain_on())
		return -1;
	setbits32((void *)(SLC_DUAL + DDR_PLL_STATUS),
		  DDR_PLL_CLOCK_ENABLE);
	for (unsigned int ch = 0; ch < 2; ch++) {
		printk(BIOS_INFO, "A210: initializing SLC%u SRAM\n", ch);
		if (slc_sram_init(ch))
			return -1;
	}
	return 0;
}

void a210_dram_map(void)
{
	static const unsigned int ports[] = {0, 1, 4, 5, 6};
	static const unsigned int sizes[] = {31, 32, 33, 34, 35, 31};

	/* Map the aperture; installed capacity is measured separately. */
	for (unsigned int i = 0; i < ARRAY_SIZE(ports); i++) {
		uintptr_t port = CI_BASE + NCORE_PORT_STRIDE * ports[i];
		uint64_t address = A210_DRAM_BASE;

		for (unsigned int region = 0; region < ARRAY_SIZE(sizes);
		     region++) {
			ci_region(port, region + 3, address >> 12,
				  NCORE_REGION_ENABLE_LOCAL |
				      ((sizes[region] - 13)
				       << NCORE_REGION_SIZE_SHIFT));
			address += 1ULL << sizes[region];
		}
	}
}

void a210_dram_enable(void)
{
	/* Code and stack remain in BRAM while leaving SRAM mode. */
	asm volatile("fence iorw, iorw" ::: "memory");
	write32((void *)(A210_SLC_SYSREG_BASE + SLC_SYSTEM_CONFIG),
		SLC_AXI_FIFO_ENABLE);
	write32((void *)(A210_SLC_SYSREG_BASE + A210_DDR_CHANNEL_STRIDE +
			 SLC_SYSTEM_CONFIG),
		SLC_AXI_FIFO_ENABLE);
	asm volatile("fence iorw, iorw" ::: "memory");
}
