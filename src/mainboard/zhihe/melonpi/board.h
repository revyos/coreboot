/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MAINBOARD_ZHIHE_MELONPI_H
#define MAINBOARD_ZHIHE_MELONPI_H

#include <soc/dram.h>

int melonpi_peripheral_power_init(void);
int melonpi_fan_init(void);
int melonpi_check_board(void);
int melonpi_power_init(void);
int melonpi_secondary_power_init(void);
int melonpi_performance_power_init(void);
extern const struct a210_dram_config melonpi_dram;

#endif
