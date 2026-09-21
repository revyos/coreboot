/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_THEAD_TH1520_STORAGE_H
#define SOC_THEAD_TH1520_STORAGE_H
#include <commonlib/region.h>
#include <types.h>

#define TH1520_SOURCE_RAM 0
#define TH1520_SOURCE_SPI 1
#define TH1520_SOURCE_EMMC 2

int th1520_spi_init(void);
int th1520_spi_read(size_t offset, size_t size, void *buffer);
int th1520_emmc_init(void);
int th1520_emmc_read(unsigned int partition, size_t offset, size_t size, void *buffer);
uint64_t th1520_emmc_capacity(unsigned int partition);
void th1520_storage_test(void);
#endif
