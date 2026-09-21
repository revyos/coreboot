/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <commonlib/sdhci.h>
#include "sdhci.c"

#define BASE 0x10000
static uint32_t status, count, total, tick, delay_end;
static bool late_error, early_end, missing_end;
static uint16_t transfer_mode;
static bool auto_stop = true;
static bool writing;

int sdhci_setup_adma(struct sdhci_ctrlr *h, struct mmc_data *d)
{
	assert(!"PIO-only controller attempted ADMA");
	return -1;
}
int sdhci_complete_adma(struct sdhci_ctrlr *h, struct mmc_command *c)
{
	assert(!"PIO-only controller attempted ADMA completion");
	return -1;
}


void udelay(unsigned int us) { tick += us; }
void mdelay(unsigned int ms) { tick += ms * 1000; }
unsigned int mock_time(void) { return tick += 1000; }
uint8_t read8(const void *addr) { return 0; }
uint16_t read16(const void *addr) { return 0; }
void write8(void *addr, uint8_t value) {}
void write16(void *addr, uint16_t value)
{
	if ((uintptr_t)addr == BASE + SDHCI_TRANSFER_MODE)
		transfer_mode = value;
	if ((uintptr_t)addr == BASE + SDHCI_COMMAND)
		status = SDHCI_INT_RESPONSE |
			(writing ? SDHCI_INT_SPACE_AVAIL : SDHCI_INT_DATA_AVAIL);
}
void write32(void *addr, uint32_t value)
{
	if ((uintptr_t)addr == BASE + SDHCI_INT_STATUS)
		status &= ~value;
	if ((uintptr_t)addr == BASE + SDHCI_BUFFER) {
		assert(writing && count < total && value == (0xcafe0000 | count));
		count++;
	}
}
uint32_t read32(const void *addr)
{
	switch ((uintptr_t)addr - BASE) {
	case SDHCI_INT_STATUS:
		if (!(status & SDHCI_INT_RESPONSE)) {
			if (early_end)
				status |= SDHCI_INT_DATA_END;
			if (count == total && !missing_end && !delay_end--)
				status |= late_error ? SDHCI_INT_ERROR : SDHCI_INT_DATA_END;
		}
		return status;
	case SDHCI_PRESENT_STATE:
		return !early_end && count < total ?
			(writing ? SDHCI_SPACE_AVAILABLE : SDHCI_DATA_AVAILABLE) : 0;
	case SDHCI_RESPONSE:
		return 0x12345678;
	case SDHCI_BUFFER:
		assert(count < total);
		return 0xcafe0000 | count++;
	default:
		return 0;
	}
}

static int transfer(unsigned int blocks, uint32_t *buffer)
{
	struct sdhci_ctrlr host = { .ioaddr = (void *)BASE,
		.sd_mmc_ctrlr.caps = DRVR_CAP_PIO_ONLY | (auto_stop ? DRVR_CAP_AUTO_CMD12 : 0) };
	struct mmc_command cmd = { .cmdidx = writing ? MMC_CMD_WRITE_MULTIPLE_BLOCK : MMC_CMD_READ_MULTIPLE_BLOCK,
		.resp_type = CARD_RSP_R1 };
	struct mmc_data data = { .dest = (void *)buffer, .blocks = blocks,
		.blocksize = 512, .flags = writing ? DATA_FLAG_WRITE : DATA_FLAG_READ };

	count = tick = 0;
	total = 128 * blocks;
	status = 0;
	delay_end = 4;
	int result = sdhci_send_command_bounced(&host.sd_mmc_ctrlr, &cmd, &data, NULL);

	assert(cmd.response[0] == 0x12345678);
	assert(!(transfer_mode & SDHCI_TRNS_DMA));
	assert(!!(transfer_mode & SDHCI_TRNS_ACMD12) == (blocks > 1 && auto_stop));
	return result;
}

int main(void)
{
	uint32_t buffer[256];

	assert(transfer(2, buffer) == 0 && count == 256);
	for (unsigned int i = 0; i < 256; i++)
		assert(buffer[i] == (0xcafe0000 | i));
	writing = true;
	assert(transfer(2, buffer) == 0 && count == 256);
	writing = false;
	auto_stop = false;
	assert(transfer(2, buffer) == 0);
	auto_stop = true;
	late_error = true;
	assert(transfer(1, buffer) != 0);
	late_error = false;
	early_end = true;
	assert(transfer(1, buffer) != 0);
	early_end = false;
	missing_end = true;
	assert(transfer(1, buffer) != 0);
	puts("PASS: PIO simultaneous response/data, full FIFO transfer and completion errors");
}
