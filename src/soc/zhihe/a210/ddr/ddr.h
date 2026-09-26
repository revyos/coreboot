/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_DDR_H
#define SOC_ZHIHE_A210_DDR_H

#include <stddef.h>
#include <soc/ddr_regs.h>
#include <stdint.h>
#include <commonlib/helpers.h>

struct a210_ddrc_setting {
	uint32_t offset;
	uint32_t value;
};

extern const struct a210_ddrc_setting a210_ddrc_4266_1r[];
extern const size_t a210_ddrc_setting_count;
extern const uint16_t ICCM_1D[0x3f35];
extern const uint16_t ICCM_2D[0x31b8];
extern const uint16_t DCCM_1D_PS0[0x33e];
extern const uint16_t DCCM_2D[0x2be];

void a210_phy_write_both(uint32_t reg, uint16_t value);
int a210_ddr_wait_training(unsigned int two_dimensional);
int a210_ddr_phy_train(void);

#endif
