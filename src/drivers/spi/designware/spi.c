/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
#include <device/mmio.h>
#include <drivers/spi/designware.h>
#include <timer.h>

#define CTRLR0 0x00
#define SSIENR 0x08
#define SER 0x10
#define BAUDR 0x14
#define TXFTLR 0x18
#define RXFLR 0x24
#define SR 0x28
#define IMR 0x2c
#define DR 0x60
#define SR_BUSY (1 << 0)
#define SR_RFNE (1 << 3)

void dw_spi_disable(const struct dw_spi *bus)
{
	write32((void *)(bus->base + SSIENR), 0);
}

int dw_spi_init(struct dw_spi *bus, uintptr_t base, uint16_t divider)
{
	if (divider < 2 || (divider & 1))
		return -1;
	bus->base = base;
	bus->divider = divider;
	dw_spi_disable(bus);
	/* DWC SSI revisions put the data-frame-size field at bit 0 or 16. */
	write32((void *)(base + CTRLR0), UINT32_MAX);
	bus->frame_size = (read32((void *)(base + CTRLR0)) & 0xf) ? 7 : 7U << 16;
	write32((void *)(base + CTRLR0), bus->frame_size);
	write32((void *)(base + BAUDR), divider);
	write32((void *)(base + IMR), 0);
	/* Probe the implemented FIFO threshold bits with SSI disabled. */
	for (bus->fifo_depth = 1; bus->fifo_depth < 256; bus->fifo_depth++) {
		write32((void *)(base + TXFTLR), bus->fifo_depth);
		if (read32((void *)(base + TXFTLR)) != bus->fifo_depth)
			break;
	}
	write32((void *)(base + TXFTLR), 0);
	return 0;
}

int dw_spi_xfer(const struct dw_spi *bus, const void *out, size_t out_size,
	void *in, size_t in_size)
{
	const uint8_t *tx = out;
	uint8_t *rx = in;
	uintptr_t base = bus->base;

	if ((out_size && !tx) || (in_size && !rx) || out_size > SIZE_MAX - in_size)
		return -1;
	dw_spi_disable(bus);
	write32((void *)(base + CTRLR0), bus->frame_size);
	write32((void *)(base + SER), 1);
	write32((void *)(base + SSIENR), 1);
	size_t total = out_size + in_size, sent = 0, received = 0;
	struct stopwatch stall;

	stopwatch_init_msecs_expire(&stall, 100);
	while (received < total) {
		size_t before = received;
		/* Bound outstanding frames by RX depth, including TX in flight. */
		size_t count = MIN(total - sent,
			bus->fifo_depth - (sent - received));

		while (count--) {
			write32((void *)(base + DR), sent < out_size ? tx[sent] : 0xff);
			sent++;
		}
		count = MIN(read32((void *)(base + RXFLR)), sent - received);
		while (count--) {
			uint8_t value = read32((void *)(base + DR));

			if (received >= out_size)
				rx[received - out_size] = value;
			received++;
		}
		if (received != before)
			stopwatch_init_msecs_expire(&stall, 100);
		else if (stopwatch_expired(&stall))
			goto fail;
	}
	if (!wait_us(100000, !(read32((void *)(base + SR)) & SR_BUSY)))
		goto fail;
	dw_spi_disable(bus);
	return 0;
fail:
	dw_spi_disable(bus);
	return -1;
}
