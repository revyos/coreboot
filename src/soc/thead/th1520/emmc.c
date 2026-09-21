/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
#include <commonlib/sdhci.h>
#include <commonlib/storage.h>
#include <console/console.h>
#include <device/mmio.h>
#include <soc/storage.h>
#include <string.h>
#include <timer.h>

#define EMMC_BASE 0xffe7080000ULL
static struct sdhci_ctrlr host;
static struct storage_media media;
static bool initialized;
static uint32_t input_clock;
static void (*generic_set_ios)(struct sd_mmc_ctrlr *ctrlr);

static void set_ios(struct sd_mmc_ctrlr *ctrlr)
{
	generic_set_ios(ctrlr);
	/* The board uses fixed 1.8 V signalling even in legacy timing. */
	setbits16((void *)(EMMC_BASE + 0x3e), 1U << 3);
}

static int emmc_clock_prepare(struct sdhci_ctrlr *controller)
{
	setbits32((void *)0xffef010204, 1U << 30);
	/* Allow the storage controller's bus access before any future DMA use. */
	write32((void *)0xfffc0280c0, UINT32_MAX);
	uint32_t pll = read32((void *)0xffef010030);
	uint32_t cfg = read32((void *)0xffef010034);
	uint32_t div = (pll & 63) * ((pll >> 20) & 7) * ((pll >> 24) & 7);
	uint64_t rate = (uint64_t)((pll >> 8) & 4095) << 24;

	if (!div)
		return -1;
	if (!(cfg & (1U << 24)))
		rate += cfg & 0xffffff;
	input_clock = (cfg & (1U << 30)) ? 6000000 :
		((rate * 24000000) >> 24) / (div * 4);
	return 0;
}

static void emmc_phy_configure(void)
{
	/* DWC MSHC PHY: fixed 1.8 V pads and the SDR delay line. */
	write32((void *)(EMMC_BASE + 0x300), 0x00cc0001);
	write8((void *)(EMMC_BASE + 0x31d), 0x10);
	write8((void *)(EMMC_BASE + 0x31e), 0x32);
	write8((void *)(EMMC_BASE + 0x326), 0x0a);
	write8((void *)(EMMC_BASE + 0x31d), 0);
	write16((void *)(EMMC_BASE + 0x304), 0x669);
	write16((void *)(EMMC_BASE + 0x306), 0x669);
	write16((void *)(EMMC_BASE + 0x30c), 0x669);
	write16((void *)(EMMC_BASE + 0x308), 0x660);
	write16((void *)(EMMC_BASE + 0x30a), 0x671);
	write8((void *)(EMMC_BASE + 0x328), 0x60);
	write8((void *)(EMMC_BASE + 0x324), 1);
	write8((void *)(EMMC_BASE + 0x325), 0x25);
	setbits8((void *)(EMMC_BASE + 0x52c), 1);
}

void soc_sd_mmc_controller_quirks(struct sd_mmc_ctrlr *ctrlr)
{
	ctrlr->clock_base = input_clock;
	ctrlr->f_min = 400000;
	ctrlr->f_max = 25000000;
	ctrlr->voltages = MMC_VDD_165_195;
	/* Keep SDR at <=25 MHz, with auto-stop available independently of DMA. */
	ctrlr->caps = DRVR_CAP_HC | DRVR_CAP_HS | DRVR_CAP_4BIT | DRVR_CAP_8BIT |
		DRVR_CAP_1V8_VDD | DRVR_CAP_AUTO_CMD12 | DRVR_CAP_PIO_ONLY;
	ctrlr->mdelay_after_cmd0 = 1;
}

int th1520_emmc_init(void)
{
	if (initialized)
		return 0;
	memset(&host, 0, sizeof(host));
	host.attach = emmc_clock_prepare;
	host.ioaddr = (void *)EMMC_BASE;
	if (add_sdhci(&host) || read8((void *)(EMMC_BASE + 0x2f)))
		return -1;
	/* SDHCI RESET_ALL clears the PHY and vendor eMMC-mode registers. */
	emmc_phy_configure();
	generic_set_ios = host.sd_mmc_ctrlr.set_ios;
	host.sd_mmc_ctrlr.set_ios = set_ios;
	if (storage_setup_media(&media, &host.sd_mmc_ctrlr))
		return -1;
	host.sd_mmc_ctrlr.b_max = 128;
	initialized = true;
	printk(BIOS_INFO, "TH1520 eMMC: %llu MiB user, %llu KiB boot0, %u-bit PIO\n",
		(unsigned long long)(media.capacity[0] / MiB),
		(unsigned long long)(media.capacity[1] / KiB), host.sd_mmc_ctrlr.bus_width);
	return 0;
}

uint64_t th1520_emmc_capacity(unsigned int partition)
{
	if (partition > 2 || th1520_emmc_init())
		return 0;
	return media.capacity[partition];
}

int th1520_emmc_read(unsigned int partition, size_t offset, size_t size, void *buffer)
{
	uint8_t *dest = buffer;
	uint8_t block[512] __aligned(4);
	uint64_t capacity = th1520_emmc_capacity(partition);

	if ((size && !buffer) || !capacity || offset > capacity || size > capacity - offset)
		return -1;
	if (!size)
		return 0;
	if (storage_set_partition(&media, partition))
		return -1;
	while (size) {
		size_t skip = offset % sizeof(block);
		size_t bytes;

		if (!skip && size >= sizeof(block) && !((uintptr_t)dest & 3)) {
			size_t blocks = MIN(size / sizeof(block), 128);

			if (storage_block_read(&media, offset / sizeof(block), blocks, dest) != blocks)
				return -1;
			bytes = blocks * sizeof(block);
		} else {
			bytes = MIN(size, sizeof(block) - skip);
			if (storage_block_read(&media, offset / sizeof(block), 1, block) != 1)
				return -1;
			memcpy(dest, block + skip, bytes);
		}
		dest += bytes;
		offset += bytes;
		size -= bytes;
	}
	return 0;
}
