/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_CLOCK_H
#define SOC_ZHIHE_A210_CLOCK_H

#include <soc/addressmap.h>

#define A210_OSC_HZ 24000000
#define TOP_PERI_CLK_CFG3 (A210_TOP_CRG_BASE + 0x28)
#define TOP_PERI_CLK_CFG4 (A210_TOP_CRG_BASE + 0x2c)
#define TOP_UART_OSC_SELECT (1U << 24)
#define TOP_EMMC_VIDEO_PLL_SELECT (1U << 20)
#define TOP_EMMC_DIV_MASK 0xf
#define EMMC_REFERENCE_DIVIDER 4
#define UART_REFERENCE_DIVIDER 10
#define A210_AUDIO0_PLL (A210_PLL_WRAP_BASE + 0x00)
#define A210_GMAC_PLL (A210_PLL_WRAP_BASE + 0x40)
#define A210_VIDEO_PLL (A210_PLL_WRAP_BASE + 0x140)
#define PLL_CFG0 0x00
#define PLL_CFG1 0x04
#define PLL_STATUS 0x10
#define PLL_REFDIV_MASK 0x3f
#define PLL_FBDIV_SHIFT 8
#define PLL_FBDIV_MASK 0xfff
#define PLL_POSTDIV1_SHIFT 20
#define PLL_POSTDIV2_SHIFT 24
#define PLL_POSTDIV_MASK 7
#define PLL_FRAC_BITS 24
#define PLL_FRAC_MASK 0xffffff
#define PLL_INTEGER_MODE (1U << 24)
#define PLL_RESET (1U << 30)
#define PLL_BYPASS (1U << 31)
#define PLL_LOCK (1U << 0)

#define A210_C908_PLL (A210_CPU_CLOCK_BASE + 0x1400)
#define A210_C920_PLL (A210_CPU_CLOCK_BASE + 0x1440)
#define A210_C908_CCU (A210_CPU_CLOCK_BASE + 0x0a00)
#define A210_C920_CCU (A210_CPU_CLOCK_BASE + 0x0c00)
#define A210_CPU_CLOCK_MUX (A210_CPU_CLOCK_BASE + 0x1600)
#define CPU_CLOCK_FROM_TOP (1U << 0)
#define TOP_CPU_CLOCK_BACKUP(cluster) \
	(A210_TOP_CRG_BASE + 0x04 + (cluster) * 4)
#define TOP_CPU_BACKUP_MUX_MASK 7
#define TOP_CPU_BACKUP_DIV0 4
#define PLL_DAC_POWER_DOWN (1U << 25)
#define PLL_CFG0_INTEGER(fb, ref, post1, post2) \
	(((fb) << PLL_FBDIV_SHIFT) | (ref) | \
	 ((post1) << PLL_POSTDIV1_SHIFT) | \
	 ((post2) << PLL_POSTDIV2_SHIFT))
#define CCU_CONTROL 0x04
#define CCU_SOFTWARE_CONTROL (1U << 0)
#define CCU_LOW_POWER_CONTROL 0x10
#define CCU_HIGH_SPEED (1U << 0)
#define CCU_CLOCK_ENABLE (1U << 1)
#define CCU_RATIO_NORMAL 0x1c
#define CCU_RATIO_MASK 0xffff
#define CPU_CLOCK_MONITOR (A210_CPU_SYSREG_BASE + 0x290)
#define CPU_CLOCK_MEASURED_KHZ (A210_CPU_SYSREG_BASE + 0x294)
#define CPU_MONITOR_SELECT_SHIFT 8
#define CPU_MONITOR_C908 18
#define CPU_MONITOR_C920 0
#define CPU_MONITOR_ENABLE (1U << 0)

#define PERI3_CLOCK_ENABLE (A210_PERI3_SYSREG_BASE + 0x200)
#define PERI3_RESET_N (A210_PERI3_SYSREG_BASE + 0x400)
#define PERI3_EMMC_CLOCKS ((1U << 2) | (0x1fU << 8))
#define PERI3_EMMC_RESETS (7U << 3)
#define PERI3_ADC_CLOCK (1U << 15)
#define PERI3_ADC_RESET_N (1U << 11)

int a210_set_cpu_clock(unsigned int cluster, unsigned int mhz);
unsigned int a210_emmc_refclk(void);
unsigned int a210_uart_refclk(void);

#endif
