/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/early.h>
#include <soc/power.h>
#include <timer.h>

#define VO_PCU_ACTIVE 0x10
#define VO_PCU_CURRENT_STATE 0x14
#define VO_BPC_INITIAL_SIGNALS 0x10101
#define VO_CLOCK_ENABLE 0x200
#define VO_RESET_N 0x400
#define VO_DMA_CLOCKS ((1U << 18) | (1U << 19) | (1U << 20) | \
		       (1U << 21) | (1U << 22))
#define VO_DMA_RESETS ((1U << 15) | (1U << 16) | \
		       (1U << 17) | (1U << 18))

int a210_display_init(void)
{
	uintptr_t pcu = A210_VO_PCU_BASE;
	struct stopwatch deadline;
	uint32_t status;

	if (!(read32((void *)(pcu + VO_PCU_ACTIVE)) & 1)) {
		write32((void *)A210_VO_BPC_BASE, BPC_HARDWARE_CONTROL);
		write32((void *)(A210_VO_BPC_BASE + BPC_SW_SLEEP_USER),
			VO_BPC_INITIAL_SIGNALS);
		write32((void *)(pcu + PCU_INTERRUPT_CLEAR),
			PCU_INTERRUPTS_ALL);
		write32((void *)(pcu + PCU_INTERRUPT_ENABLE),
			PCU_INTERRUPTS_ALL);
		write32((void *)(pcu + PCU_SW_STATE), PCU_POWER_ON);
		write32((void *)(pcu + PCU_SW_REQUEST), PCU_REQUEST);
		stopwatch_init_msecs_expire(&deadline, 100);
		do {
			status = read32((void *)(pcu + PCU_RAW_STATUS));
			if (stopwatch_expired(&deadline))
				return -1;
		} while (!status);
		write32((void *)(pcu + PCU_INTERRUPT_CLEAR), status);
		if (status != PCU_REQUEST_ACCEPT)
			return -1;
	}
	if (read32((void *)(pcu + VO_PCU_CURRENT_STATE)) !=
	    PCU_POWER_ON)
		return -1;
	setbits32((void *)(A210_VO_SYSREG_BASE + VO_CLOCK_ENABLE),
		  VO_DMA_CLOCKS);
	setbits32((void *)(A210_VO_SYSREG_BASE + VO_RESET_N), VO_DMA_RESETS);
	if (a210_display_dma_init())
		return -1;
	printk(BIOS_INFO, "A210: display power and DMA access ready\n");
	return 0;
}
