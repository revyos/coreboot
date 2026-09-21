/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
#include <device/mmio.h>
#include <console/console.h>
#include <drivers/spi/designware.h>
#include <soc/storage.h>
#include <spi_flash.h>
#include <spi-generic.h>

#define QSPI0_BASE 0xffea000000ULL
#define QSPI_GPIO 0xffe7f34000ULL
#define QSPI_PAD 0xffec007000ULL
#define QSPI_CS (1U << 3)

static struct dw_spi qspi;
static struct spi_flash flash;
static bool initialized;

static void chip_select(bool active)
{
	if (active)
		clrbits32((void *)QSPI_GPIO, QSPI_CS);
	else
		setbits32((void *)QSPI_GPIO, QSPI_CS);
}

static int setup(const struct spi_slave *slave)
{
	if (slave->bus || slave->cs)
		return -1;
	setbits32((void *)0xffef010204, (1U << 17) | (1U << 6));
	chip_select(false);
	setbits32((void *)(QSPI_GPIO + 4), QSPI_CS);
	for (unsigned int pin = 2; pin <= 8; pin++) {
		if (pin == 4)
			continue; /* Only CS0 is used; leave CS1's mux untouched. */
		unsigned int shift = 4 * (pin % 8);
		void *mux = (void *)(QSPI_PAD + 0x400 + 4 * (pin / 8));

		clrsetbits32(mux, 15U << shift, pin == 3 ? 3U << shift : 0);
		shift = 16 * (pin % 2);
		clrsetbits32((void *)(QSPI_PAD + 4 * (pin / 2)), 0x3ffU << shift,
			(pin == 3 ? 4U : 0x204U) << shift);
	}
	/* QSPI0 is directly clocked by VIDEO_PLL; preserve its programming. */
	uint32_t pll = read32((void *)0xffef010030);
	uint32_t cfg = read32((void *)0xffef010034);
	uint32_t div = (pll & 63) * ((pll >> 20) & 7) * ((pll >> 24) & 7);
	uint64_t rate = (uint64_t)((pll >> 8) & 4095) << 24;

	if (!div)
		return -1;
	if (!(cfg & (1U << 24)))
		rate += cfg & 0xffffff;
	rate = (cfg & (1U << 30)) ? 24000000 :
		((rate * 24000000) >> 24) / div;
	/* Meles and LPi4A board descriptions limit the flash to 10 MHz. */
	uint32_t divider = MAX(2, DIV_ROUND_UP(rate, 10000000));

	divider = (divider + 1) & ~1U;
	if (divider > 65534)
		return -1;
	printk(BIOS_INFO, "TH1520 SPI: %llu kHz, divider %u\n",
		(unsigned long long)(rate / divider / 1000), divider);
	return dw_spi_init(&qspi, QSPI0_BASE, divider);
}

static int claim(const struct spi_slave *slave)
{
	chip_select(true);
	return 0;
}

static void release(const struct spi_slave *slave)
{
	chip_select(false);
	dw_spi_disable(&qspi);
}

static int transfer(const struct spi_slave *slave, const void *out,
	size_t out_size, void *in, size_t in_size)
{
	return dw_spi_xfer(&qspi, out, out_size, in, in_size);
}

static const struct spi_ctrlr controller = {
	.setup = setup,
	.claim_bus = claim,
	.release_bus = release,
	.xfer = transfer,
	.max_xfer_size = 64 * KiB,
};

const struct spi_ctrlr_buses spi_ctrlr_bus_map[] = {
	{ .ctrlr = &controller, .bus_start = 0, .bus_end = 0 },
};
const size_t spi_ctrlr_bus_map_count = ARRAY_SIZE(spi_ctrlr_bus_map);

int th1520_spi_init(void)
{
	if (!initialized) {
		if (spi_flash_probe(0, 0, &flash))
			return -1;
		initialized = true;
	}
	return 0;
}

int th1520_spi_read(size_t offset, size_t size, void *buffer)
{
	if ((size && !buffer) || th1520_spi_init() ||
	    offset > flash.size || size > flash.size - offset)
		return -1;
	return size ? spi_flash_read(&flash, offset, size, buffer) : 0;
}
