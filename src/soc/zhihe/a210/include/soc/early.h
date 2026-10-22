/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_EARLY_H
#define SOC_ZHIHE_A210_EARLY_H

void a210_adc_init(void);
int a210_adc_read_mv(unsigned int channel);
int a210_aon_i2c_init(void);
int a210_aon_i2c1_init(void);
int a210_pmic_i2c_release(void);
int a210_power_control_init(void);
int a210_ocram_init(void);
int a210_ocram_test(void);
void a210_dram_enable(void);
void a210_dram_map(void);
int a210_prepare_harts(void);
int a210_peri1_dma_init(void);
int a210_pcie_dma_init(void);
int a210_usb_dma_init(void);
int a210_display_dma_init(void);
int a210_display_init(void);
void exit_car(void);

#endif
