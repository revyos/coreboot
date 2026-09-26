/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_DRAM_H
#define SOC_ZHIHE_A210_DRAM_H

#include <stdint.h>

enum a210_dram_profile {
	A210_LPDDR4X_4266_1R,
};

struct a210_dram_config {
	enum a210_dram_profile profile;
	uint8_t dq_map[4][8];
};

int a210_ddr_init(const struct a210_dram_config *config, uint64_t *size);
int a210_ddr_test(uint64_t size);

#endif
