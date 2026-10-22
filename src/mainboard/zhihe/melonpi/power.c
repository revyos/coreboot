/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <delay.h>
#include <device/i2c_simple.h>
#include <soc/early.h>
#include <soc/gpio.h>
#include "board.h"

#define PMIC_ADDRESS 0x06
#define PMIC_CHIP_NAME 0x58
#define PMIC_BUCK_ENABLE 0x3d
#define PMIC_GPIO_ENABLE 0x40
#define CPU_BOOT_UV 800000
#define PMIC_BUCK_VOLTAGE_BASE 0x09
#define PMIC_BUCK1_VOLTAGE (PMIC_BUCK_VOLTAGE_BASE + 0)
#define PMIC_BUCK4_VOLTAGE (PMIC_BUCK_VOLTAGE_BASE + 3)
#define PMIC_BUCK5_VOLTAGE (PMIC_BUCK_VOLTAGE_BASE + 4)
#define CPU_P_PMIC_ADDRESS 0x67
#define CPU_P_CHIP_NAME 0x01
#define CPU_P_CHIP_VERSION 0x02
#define CPU_P_BUCK2_VOLTAGE 0x7c
#define CPU_P_BUCK2_ENABLE 0x7d
#define CPU_P_REGULATOR_ENABLE (1U << 0)
#define CPU_P_PMIC_GPIO_ENABLE (1U << 2)

/* SY70209 register layout and voltage steps from the A210 AON SDK. */
static int cpu_voltage(unsigned int buck, unsigned int target_uv)
{
	const uint8_t reg = PMIC_BUCK_VOLTAGE_BASE + buck - 1;
	uint8_t value, verify;

	if (i2c_readb(0, PMIC_ADDRESS, reg, &value))
		return -1;
	unsigned int uv = 600000 + (value & 0x7f) * 12500;

	printk(BIOS_INFO, "A210: BUCK%u voltage setting %u uV\n", buck,
	       uv);
	if (uv > 1000000)
		return -1;
	/* Do not lower a rail inherited from a previous boot stage. */
	if (uv >= target_uv)
		return 0;
	value = (value & ~0x7f) | (target_uv - 600000) / 12500;
	if (i2c_writeb(0, PMIC_ADDRESS, reg, value))
		return -1;
	udelay(100);
	if (i2c_readb(0, PMIC_ADDRESS, reg, &verify))
		return -1;
	return verify == value ? 0 : -1;
}

int melonpi_power_init(void)
{
	uint8_t id, enabled, gpio;
	static const struct {
		uint8_t reg;
		uint8_t mask;
		uint8_t value;
	} ddr_rails[] = {
	    {PMIC_BUCK1_VOLTAGE, 0x3f, (1800000 - 1200000) / 37500},
	    {PMIC_BUCK4_VOLTAGE, 0x3f, (600000 - 300000) / 12500},
	    {PMIC_BUCK5_VOLTAGE, 0x7f, (800000 - 600000) / 12500},
	};

	if (a210_aon_i2c_init() ||
	    i2c_readb(0, PMIC_ADDRESS, PMIC_CHIP_NAME, &id))
		return -1;
	printk(BIOS_INFO, "A210: PMIC chip name %#x\n", id);
	if (id != 0x88)
		return -1;
	if (i2c_readb(0, PMIC_ADDRESS, PMIC_BUCK_ENABLE, &enabled) ||
	    i2c_readb(0, PMIC_ADDRESS, PMIC_GPIO_ENABLE, &gpio))
		return -1;
	/* DDR BUCK1/4/5, CPU BUCK6/7 and the external DDR 1.1 V rail. */
	if ((enabled & 0x79) != 0x79 || !(gpio & (1 << 1))) {
		printk(BIOS_ERR,
		       "A210: required power rails are disabled\n");
		return -1;
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(ddr_rails); i++) {
		uint8_t value;

		if (i2c_readb(0, PMIC_ADDRESS, ddr_rails[i].reg, &value))
			return -1;
		printk(BIOS_INFO, "A210: DDR PMIC register %#x = %#x\n",
		       ddr_rails[i].reg, value);
		if ((value & ddr_rails[i].mask) != ddr_rails[i].value)
			return -1;
	}
	/* Raise SRAM before CPU logic. Keep the BootROM CPU clock. */
	if (cpu_voltage(7, CPU_BOOT_UV) || cpu_voltage(6, CPU_BOOT_UV))
		return -1;
	printk(BIOS_INFO,
	       "A210: DDR rails verified, CPU rails >= 800 mV\n");
	return 0;
}

int melonpi_secondary_power_init(void)
{
	const uint8_t address = CPU_P_PMIC_ADDRESS;
	uint8_t gpio, id, revision, voltage, enabled, verify;

	if (a210_aon_i2c_init() ||
	    i2c_readb(0, PMIC_ADDRESS, PMIC_GPIO_ENABLE, &gpio))
		return -1;
	/* SY70209 GPIO2 enables the SY70300 that supplies C920 CPU_P. */
	if (!(gpio & CPU_P_PMIC_GPIO_ENABLE) &&
	    i2c_writeb(0, PMIC_ADDRESS, PMIC_GPIO_ENABLE,
		       gpio | CPU_P_PMIC_GPIO_ENABLE))
		return -1;
	udelay(100);
	if (i2c_readb(0, address, CPU_P_CHIP_NAME, &id) || id != 0xff ||
	    i2c_readb(0, address, CPU_P_CHIP_VERSION, &revision) ||
	    revision > 1 ||
	    i2c_readb(0, address, CPU_P_BUCK2_VOLTAGE, &voltage) ||
	    i2c_readb(0, address, CPU_P_BUCK2_ENABLE, &enabled))
		return -1;
	/* AON SDK: BUCK2 is 450 mV + code * 5 mV, at most 1 V. */
	unsigned int uv = 450000 + voltage * 5000;

	if (uv > 1000000)
		return -1;
	if (uv < CPU_BOOT_UV) {
		voltage = (CPU_BOOT_UV - 450000) / 5000;
		if (i2c_writeb(0, address, CPU_P_BUCK2_VOLTAGE, voltage))
			return -1;
	}
	if (!(enabled & CPU_P_REGULATOR_ENABLE) &&
	    i2c_writeb(0, address, CPU_P_BUCK2_ENABLE,
		       enabled | CPU_P_REGULATOR_ENABLE))
		return -1;
	udelay(100);
	if (i2c_readb(0, address, CPU_P_BUCK2_VOLTAGE, &verify) ||
	    verify != voltage ||
	    i2c_readb(0, address, CPU_P_BUCK2_ENABLE, &verify) ||
	    !(verify & CPU_P_REGULATOR_ENABLE))
		return -1;
	printk(BIOS_INFO, "Melon Pi: C920 CPU_P ready at %u uV\n",
	       450000 + voltage * 5000);
	return 0;
}

#define MELONPI_CPU_PERFORMANCE_UV 1000000
int melonpi_performance_power_init(void)
{
	const uint8_t voltage = (MELONPI_CPU_PERFORMANCE_UV - 450000) / 5000;
	uint8_t verify;

	/* E902 is still stopped. Raise SRAM before logic, then the C920 rail. */
	if (cpu_voltage(7, MELONPI_CPU_PERFORMANCE_UV) ||
	    cpu_voltage(6, MELONPI_CPU_PERFORMANCE_UV) ||
	    i2c_writeb(0, CPU_P_PMIC_ADDRESS, CPU_P_BUCK2_VOLTAGE, voltage))
		return -1;
	mdelay(2);
	if (i2c_readb(0, CPU_P_PMIC_ADDRESS, CPU_P_BUCK2_VOLTAGE, &verify) ||
	    verify != voltage)
		return -1;
	printk(BIOS_INFO, "Melon Pi: CPU supplies ready at 1 V\n");
	return 0;
}

#define MELONPI_3V3_ENABLE_PIN 26
#define MELONPI_3V3_ENABLE_PAD 27
#define MELONPI_5V_ENABLE_PIN 29
#define MELONPI_5V_ENABLE_PAD 30

int melonpi_peripheral_power_init(void)
{
	/* The carrier's 3V3 power-good also enables its 1V8 supply. */
	a210_gpio_output(A210_AON_GPIO0_BASE, MELONPI_5V_ENABLE_PIN);
	a210_pad_gpio(A210_AON_PADCTRL_BASE, MELONPI_5V_ENABLE_PAD);
	a210_gpio_output(A210_AON_GPIO0_BASE, MELONPI_3V3_ENABLE_PIN);
	a210_pad_gpio(A210_AON_PADCTRL_BASE, MELONPI_3V3_ENABLE_PAD);
	mdelay(2);
	printk(BIOS_INFO, "Melon Pi: peripheral 5V/3V3 enabled\n");
	return 0;
}
