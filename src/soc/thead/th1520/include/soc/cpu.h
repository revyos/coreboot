/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_THEAD_TH1520_CPU_H
#define SOC_THEAD_TH1520_CPU_H

void th1520_cpu_clock_init(void);
int th1520_cpu_set_clock(unsigned int mhz);
void th1520_clean_data_cache(void);
/* Per-hart RISC-V ramstage cache transition hook. */
void exit_car(void);

#endif
