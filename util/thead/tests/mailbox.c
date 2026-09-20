/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ddr_internal.h"

static uint32_t messages[80];
static unsigned int message_count, cursor, selected_phy, ticks, acks;
static bool acknowledged, stuck_ack, repeat_progress;

unsigned int mock_time(void)
{
	return ticks++;
}

static uint32_t phy_word(const void *address)
{
	uintptr_t base = selected_phy ? TH1520_DDR_PHY1_BASE : TH1520_DDR_PHY0_BASE;
	uintptr_t offset = (uintptr_t)address - base;

	assert(!(offset & 1) && offset < 0x1000000);
	return offset / 2;
}

uint16_t read16(const void *address)
{
	switch (phy_word(address)) {
	case PHY_MAIL_STATUS:
		return acknowledged ? !stuck_ack : cursor >= message_count;
	case PHY_MAIL_LOW:
		assert(cursor < message_count && !acknowledged);
		return messages[cursor];
	case PHY_MAIL_HIGH:
		assert(cursor < message_count && !acknowledged);
		return messages[cursor] >> 16;
	default:
		assert(!"unexpected mailbox read");
		return 0;
	}
}

void write16(void *address, uint16_t value)
{
	assert(phy_word(address) == PHY_MAIL_ACK);
	if (!value) {
		assert(!acknowledged);
		acknowledged = true;
	} else {
		assert(value == 1 && acknowledged && !stuck_ack);
		acknowledged = false;
		acks++;
		if (!repeat_progress)
			cursor++;
	}
}

static void start(unsigned int phy, const uint32_t *input, unsigned int count)
{
	assert(count <= ARRAY_SIZE(messages));
	memcpy(messages, input, count * sizeof(*input));
	message_count = count;
	selected_phy = phy;
	cursor = ticks = acks = 0;
	acknowledged = stuck_ack = repeat_progress = false;
}

int main(void)
{
	for (unsigned int phy = 0; phy < 2; phy++) {
		const uint32_t progress[] = { 0, 1, 7 };
		const uint32_t stream[] = { 8, 0x12340003, 7, 0xff, 8, 0, 7 };
		const uint32_t empty_stream[] = { 8, 0x12340000, 7 };
		const uint32_t error[] = { 0xff };
		const uint32_t truncated[] = { 8, 0x12340003, 7, 0xff };
		uint32_t long_stream[43] = { 8, 0x12340028 };

		start(phy, progress, ARRAY_SIZE(progress));
		assert(ddr_pmu_wait(phy, 1) == 0 && cursor == 3);
		start(phy, stream, ARRAY_SIZE(stream));
		assert(ddr_pmu_wait(phy, 2) == 0 && cursor == 7);
		start(phy, empty_stream, ARRAY_SIZE(empty_stream));
		assert(ddr_pmu_wait(phy, 1) == 0 && cursor == 3);
		for (unsigned int i = 2; i < 42; i++)
			long_stream[i] = i & 1 ? 0xff : 7;
		long_stream[42] = 7;
		start(phy, long_stream, ARRAY_SIZE(long_stream));
		assert(ddr_pmu_wait(phy, 2) == 0 && cursor == 43);
		start(phy, error, ARRAY_SIZE(error));
		assert(ddr_pmu_wait(phy, 1) == -1 && cursor == 1);
		start(phy, truncated, ARRAY_SIZE(truncated));
		assert(ddr_pmu_wait(phy, 1) == -1 && cursor == 4);
		assert(ticks <= 10002);
		start(phy, progress, 0);
		assert(ddr_pmu_wait(phy, 1) == -1 && !acks);
		assert(ticks <= 10002);
		start(phy, progress, ARRAY_SIZE(progress));
		stuck_ack = true;
		assert(ddr_pmu_wait(phy, 1) == -1 && !acks && acknowledged);
		assert(ticks <= 10002);
		start(phy, progress, ARRAY_SIZE(progress));
		repeat_progress = true;
		assert(ddr_pmu_wait(phy, 1) == -1);
		assert(ticks <= 10002);
	}
	puts("PASS: PMU framing, status-like arguments, long/truncated streams and deadlines");
	return 0;
}
