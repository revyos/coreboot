/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_AON_H
#define SOC_ZHIHE_A210_AON_H

int a210_aon_start(void);
int a210_aon_set_voltage(unsigned int resource, unsigned int uv);
int a210_aon_get_voltage(unsigned int resource, unsigned int *uv);

#endif
