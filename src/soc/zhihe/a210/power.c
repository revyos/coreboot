/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/early.h>

#define PMIC_CONFIG 0x50
#define PMIC_HARDWARE_BYPASS (1U << 28)
#define PMIC_I2C_SELECT (1U << 29)
#define PCA_CONTROL 0x20

int a210_power_control_init(void)
{
	void *config = (void *)(A210_PMIC_CTRL_BASE + PMIC_CONFIG);
	const uint32_t mode = PMIC_HARDWARE_BYPASS | PMIC_I2C_SELECT;

	/* Configure before PCU/PPU requests and before AON owns the PMIC. */
	setbits32(config, mode);
	uint32_t value = read32(config);

	/* Software voltage control must not wait for autonomous PCA requests. */
	write32((void *)(A210_C920_PCA_BASE + PCA_CONTROL), 0);
	write32((void *)(A210_CPU_TOP_MEM_PCA_BASE + PCA_CONTROL), 0);
	write32((void *)(A210_CPU_TOP_LOGIC_PCA_BASE + PCA_CONTROL), 0);

	printk(BIOS_INFO, "A210: PMIC request controller %#x\n", value);
	return (value & mode) == mode ? 0 : -1;
}
