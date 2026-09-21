/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef DRIVERS_SPI_DESIGNWARE_H
#define DRIVERS_SPI_DESIGNWARE_H
#include <types.h>

struct dw_spi {
	uintptr_t base;
	uint32_t frame_size;
	uint16_t divider;
	uint16_t fifo_depth;
};

int dw_spi_init(struct dw_spi *bus, uintptr_t base, uint16_t divider);
int dw_spi_xfer(const struct dw_spi *bus, const void *out, size_t out_size,
	void *in, size_t in_size);
void dw_spi_disable(const struct dw_spi *bus);
#endif
