/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_STORAGE_H
#define SOC_ZHIHE_A210_STORAGE_H

#include <stddef.h>
#include <stdint.h>

int a210_emmc_init(void);
void a210_emmc_quiesce(void);
int a210_emmc_resume(void);
uint64_t a210_emmc_capacity(unsigned int partition);
int a210_emmc_read(unsigned int partition, size_t offset, size_t size,
		   void *buffer);
void a210_storage_test(void);
void a210_stage_ram_firmware(void);
void __noreturn a210_load_ddr_stage(void);

#endif
