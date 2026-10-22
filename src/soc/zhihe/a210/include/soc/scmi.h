/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_SCMI_H
#define SOC_ZHIHE_A210_SCMI_H

#include <stdint.h>

/* Stable firmware clock ID, shared with the A210 SCMI DT binding. */
#define A210_SCMI_CLK_EMMC_REF 140
#define A210_SCMI_CLK_UART4 4
#define A210_SCMI_SHMEM_BASE 0x30907c00
#define A210_SCMI_SHMEM_SIZE 0x100

int a210_scmi_init(void);
int a210_scmi_clock_rate(unsigned int id, uint32_t *hz);

#endif
