/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
#include <commonlib/sdhci.h>
#include <commonlib/storage.h>
#include <console/console.h>
#include <device/mmio.h>
#include <soc/storage.h>
#include <soc/clock.h>
#include <soc/emmc.h>
#include <commonlib/storage/sdhci.h>
#include <delay.h>
#include <string.h>
#include <timer.h>

static struct sdhci_ctrlr host;
static struct storage_media media;
static bool initialized;
static uint32_t input_clock;
static void (*generic_set_ios)(struct sd_mmc_ctrlr *ctrlr);

static void set_ios(struct sd_mmc_ctrlr *ctrlr)
{
	generic_set_ios(ctrlr);
	/* The board uses fixed 1.8 V signalling even in legacy timing. */
	setbits16((void *)(A210_EMMC_BASE + SDHCI_HOST_CONTROL2),
		  SDHCI_CTRL_VDD_180);
}

static int emmc_clock_prepare(struct sdhci_ctrlr *controller)
{
	setbits32((void *)PERI3_CLOCK_ENABLE, PERI3_EMMC_CLOCKS);
	setbits32((void *)PERI3_RESET_N, PERI3_EMMC_RESETS);
	input_clock = a210_emmc_refclk();
	return input_clock ? 0 : -1;
}

static void emmc_phy_configure(void)
{
	/* A210 legacy/HS pad settings and 50-tap SDR delay from U-Boot. */
	write32((void *)(A210_EMMC_BASE + DWC_PHY_CONFIG),
		DWC_PHY_RESET_N | DWC_PHY_PAD_DRIVE);
	write8((void *)(A210_EMMC_BASE + DWC_PHY_SDCLK_DELAY_CONFIG),
	       DWC_PHY_DELAY_UPDATE);
	write8((void *)(A210_EMMC_BASE + DWC_PHY_SDCLK_DELAY),
	       DWC_PHY_SDR_DELAY_TAPS);
	write8((void *)(A210_EMMC_BASE + DWC_PHY_DLL_CONFIG2),
	       DWC_PHY_DLL_START_POINT);
	write8((void *)(A210_EMMC_BASE + DWC_PHY_SDCLK_DELAY_CONFIG), 0);
	write16((void *)(A210_EMMC_BASE + DWC_PHY_CMD_PAD),
		DWC_PHY_PAD_SLEW | DWC_PHY_PAD_PULL_UP |
		    DWC_PHY_PAD_RX_3V3);
	write16((void *)(A210_EMMC_BASE + DWC_PHY_DATA_PAD),
		DWC_PHY_PAD_SLEW | DWC_PHY_PAD_PULL_UP |
		    DWC_PHY_PAD_RX_3V3);
	write16((void *)(A210_EMMC_BASE + DWC_PHY_RESET_PAD),
		DWC_PHY_PAD_SLEW | DWC_PHY_PAD_PULL_UP |
		    DWC_PHY_PAD_RX_3V3);
	write16((void *)(A210_EMMC_BASE + DWC_PHY_CLK_PAD),
		DWC_PHY_PAD_SLEW);
	write16((void *)(A210_EMMC_BASE + DWC_PHY_STROBE_PAD),
		DWC_PHY_PAD_SLEW | DWC_PHY_PAD_PULL_DOWN |
		    DWC_PHY_PAD_RX_3V3);
	write8((void *)(A210_EMMC_BASE + DWC_PHY_DLL_DELAY_CONFIG), 0);
	write8((void *)(A210_EMMC_BASE + DWC_PHY_DLL_CONTROL),
	       DWC_PHY_DLL_ENABLE);
	write8((void *)(A210_EMMC_BASE + DWC_PHY_DLL_CONFIG1),
	       DWC_PHY_DLL_WAIT_CYCLES);
	setbits8((void *)(A210_EMMC_BASE + DWC_EMMC_CONTROL),
		 DWC_EMMC_CARD);
}

void soc_sd_mmc_controller_quirks(struct sd_mmc_ctrlr *ctrlr)
{
	ctrlr->clock_base = input_clock;
	ctrlr->f_min = 400000;
	ctrlr->f_max = MIN(input_clock, 52000000U);
	ctrlr->voltages = MMC_VDD_33_34;
	/* SDR high-speed: 3.3 V card power, fixed 1.8 V signalling. */
	ctrlr->caps = DRVR_CAP_HC | DRVR_CAP_HS | DRVR_CAP_HS52 |
		      DRVR_CAP_4BIT | DRVR_CAP_8BIT | DRVR_CAP_AUTO_CMD12 |
		      DRVR_CAP_PIO_ONLY;
	ctrlr->mdelay_after_cmd0 = 1;
}

int a210_emmc_init(void)
{
	if (initialized)
		return 0;
	memset(&host, 0, sizeof(host));
	host.attach = emmc_clock_prepare;
	host.ioaddr = (void *)A210_EMMC_BASE;
	if (add_sdhci(&host) ||
	    read8((void *)(A210_EMMC_BASE + SDHCI_SOFTWARE_RESET)))
		return -1;
	/* RESET_ALL clears the PHY and vendor eMMC registers. */
	emmc_phy_configure();
	generic_set_ios = host.sd_mmc_ctrlr.set_ios;
	host.sd_mmc_ctrlr.set_ios = set_ios;
	if (storage_setup_media(&media, &host.sd_mmc_ctrlr))
		return -1;
	host.sd_mmc_ctrlr.b_max = 128;
	initialized = true;
	printk(BIOS_INFO,
	       "A210 eMMC: %llu MiB user, %llu KiB boot0, %u-bit PIO\n",
	       (unsigned long long)(media.capacity[0] / MiB),
	       (unsigned long long)(media.capacity[1] / KiB),
	       host.sd_mmc_ctrlr.bus_width);
	printk(BIOS_INFO, "A210 eMMC: input %u Hz, bus %u Hz\n",
	       input_clock, host.sd_mmc_ctrlr.bus_hz);
	return 0;
}

uint64_t a210_emmc_capacity(unsigned int partition)
{
	if (partition > 2 || a210_emmc_init())
		return 0;
	return media.capacity[partition];
}

int a210_emmc_read(unsigned int partition, size_t offset, size_t size,
		   void *buffer)
{
	uint8_t *dest = buffer;
	uint8_t block[512] __aligned(4);
	uint64_t capacity = a210_emmc_capacity(partition);

	if ((size && !buffer) || !capacity || offset > capacity ||
	    size > capacity - offset)
		return -1;
	if (!size)
		return 0;
	if (storage_set_partition(&media, partition))
		return -1;
	while (size) {
		size_t skip = offset % sizeof(block);
		size_t bytes;

		if (!skip && size >= sizeof(block) &&
		    !((uintptr_t)dest & 3)) {
			size_t blocks = MIN(size / sizeof(block), 128);

			if (storage_block_read(&media,
					       offset / sizeof(block),
					       blocks, dest) != blocks)
				return -1;
			bytes = blocks * sizeof(block);
		} else {
			bytes = MIN(size, sizeof(block) - skip);
			if (storage_block_read(&media,
					       offset / sizeof(block), 1,
					       block) != 1)
				return -1;
			memcpy(dest, block + skip, bytes);
		}
		dest += bytes;
		offset += bytes;
		size -= bytes;
	}
	return 0;
}
