/* SPDX-License-Identifier: GPL-2.0-only */
#include <cbfs.h>
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/e902.h>
#include <soc/early.h>
#include <soc/scmi.h>
#include <soc/storage.h>
#include <string.h>

#define E902_VECTOR (A210_AON_SYSREG_BASE + 0x24)
#define E902_RESET (A210_AON_RESET_BASE + 0x18)
#define E902_RESET_RELEASE (1U << 1)
#define PERI0_CLOCK_ENABLE (A210_PERI0_SYSREG_BASE + 0x200)
#define PERI0_RESET_N (A210_PERI0_SYSREG_BASE + 0x400)
#define PERI0_MBOX_ENABLE ((1U << 5) | (1U << 6))
#define E902_IMAGE_LIMIT (A210_SCMI_SHMEM_BASE - A210_AON_SRAM_BASE)

int a210_e902_start(void)
{
	size_t size;
	void *image = cbfs_map("fallback/scmi", &size);

	if (!image || !size || size > E902_IMAGE_LIMIT) {
		if (image)
			cbfs_unmap(image);
		printk(BIOS_ERR, "A210: missing or oversized SCMI firmware\n");
		return -1;
	}
	setbits32((void *)PERI0_CLOCK_ENABLE, PERI0_MBOX_ENABLE);
	setbits32((void *)PERI0_RESET_N, PERI0_MBOX_ENABLE);
	write32((void *)E902_RESET, 0);
	udelay(10);
	memset((void *)A210_AON_SRAM_BASE, 0, A210_AON_SRAM_SIZE);
	memcpy((void *)A210_AON_SRAM_BASE, image, size);
	/* E902 does not share the application CPUs' coherent caches. */
	asm volatile("fence rw, rw; .long 0x0010000b; fence.i" ::: "memory");
	if (memcmp((void *)A210_AON_SRAM_BASE, image, size)) {
		cbfs_unmap(image);
		return -1;
	}
	cbfs_unmap(image);
	if (a210_pmic_i2c_release()) {
		printk(BIOS_ERR, "A210: PMIC I2C handoff failed\n");
		return -1;
	}
	/* Zephyr establishes the eMMC parent clock before publishing SCMI. */
	a210_emmc_quiesce();
	write32((void *)E902_VECTOR, A210_AON_SRAM_BASE);
	write32((void *)E902_RESET, E902_RESET_RELEASE);
	if (a210_scmi_init() || a210_emmc_resume())
		return -1;
	printk(BIOS_INFO, "A210: E902 SCMI firmware started, %zu bytes\n", size);
	return 0;
}
