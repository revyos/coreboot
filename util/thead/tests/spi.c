/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <drivers/spi/designware.h>

#define BASE 0x1000
static uint32_t regs[32];
static uint8_t sent[512];
static unsigned int count, received;
unsigned int mock_time(void) { static unsigned int t; return t += 1000; }
static bool dfs32, rx_timeout, busy_timeout;

void write32(void *address, uint32_t value)
{
	unsigned int offset = (uintptr_t)address - BASE;

	assert(offset < sizeof(regs));
	regs[offset / 4] = value;
	if (offset == 8 && !value)
		received = count;
	if (offset == 0x18)
		regs[offset / 4] = value & 7;
	if (offset == 0x60) {
		assert(count < sizeof(sent));
		assert(count - received < 8);
		sent[count++] = value;
	}
}

uint32_t read32(const void *address)
{
	unsigned int offset = (uintptr_t)address - BASE;

	if (!offset && regs[0] == UINT32_MAX)
		return dfs32 ? 0x1f0000 : 0xf;
	if (offset == 0x28)
		return (rx_timeout ? 0 : 8) | (busy_timeout ? 1 : 0);
	if (offset == 0x24)
		return rx_timeout ? 0 : count - received;
	if (offset == 0x60)
		return 0xa0 + received++;
	return regs[offset / 4];
}

int main(void)
{
	const uint8_t command[] = { 3, 0x12, 0x34, 0x56 };
	uint8_t result[257];
	struct dw_spi spi;

	for (unsigned int variant = 0; variant < 2; variant++) {
		dfs32 = variant;
		assert(dw_spi_init(&spi, BASE, 3) == -1);
		assert(dw_spi_init(&spi, BASE, 400) == 0);
		assert(spi.frame_size == (variant ? 7U << 16 : 7));
		count = received = 0;
		assert(dw_spi_xfer(&spi, command, 4, result + 1, 8) == 0);
		assert(count == 12 && !memcmp(sent, command, 4));
		for (unsigned int i = 4; i < count; i++)
			assert(sent[i] == 0xff);
		for (unsigned int i = 0; i < 8; i++)
			assert(result[i + 1] == 0xa4 + i);
		assert(regs[8 / 4] == 0);
		count = received = 0;
		assert(spi.fifo_depth == 8);
		assert(dw_spi_xfer(&spi, command, 4, result + 1, 256) == 0);
		for (unsigned int i = 0; i < 256; i++)
			assert(result[i + 1] == (uint8_t)(0xa4 + i));
		count = received = 0;
		rx_timeout = true;
		assert(dw_spi_xfer(&spi, command, 4, result, 8) == -1);
		assert(regs[8 / 4] == 0);
		rx_timeout = false;
		count = received = 0;
		busy_timeout = true;
		assert(dw_spi_xfer(&spi, command, 4, result, 8) == -1);
		assert(regs[8 / 4] == 0);
		busy_timeout = false;
		assert(dw_spi_xfer(&spi, NULL, 1, result, 8) == -1);
	}
	puts("PASS: SSI FIFO bounds, frame layouts, command/read and timeout cleanup");
}
