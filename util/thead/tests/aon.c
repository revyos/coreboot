/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <device/mmio.h>

static uint8_t blob[0xc04];
static uint32_t image_words[0x10000 / 4], request[8], reply[7];
static bool bad_image, timeout, bad_header, short_reply, bad_voltage;
static bool safe_clock_failure, set_seen, cleaned, started, boosted, published;
static unsigned int polls, response_reads, requests, clock_calls, reset_state;

static void be32(uint8_t *p, uint32_t value)
{
	p[0] = value >> 24;
	p[1] = value >> 16;
	p[2] = value >> 8;
	p[3] = value;
}

uint32_t read32(const void *address)
{
	uintptr_t addr = (uintptr_t)address;

	if (addr == 0xffffc39010ULL) {
		if (timeout || ++polls < 3)
			return 0;
		published = true;
		return 64;
	}
	if (addr >= 0xffffc39014ULL && addr <= 0xffffc3902cULL) {
		assert(published);
		response_reads++;
		return reply[(addr - 0xffffc39014ULL) / 4];
	}
	return 0;
}

void write32(void *address, uint32_t value)
{
	uintptr_t addr = (uintptr_t)address;

	if (addr >= 0xffffef8000ULL && addr < 0xfffff08000ULL) {
		assert(reset_state == 0 && clock_calls == 1);
		image_words[(addr - 0xffffef8000ULL) / 4] = value;
	} else if (addr == 0xfffff44024ULL) {
		reset_state = value;
		if (value == 3) {
			assert(cleaned);
			assert(!memcmp(image_words, blob, sizeof(blob)));
			started = true;
		}
	} else if (addr == 0xfffff48044ULL) {
		assert(cleaned && value == 0xffef8c00);
	} else if (addr >= 0xffffc44014ULL && addr <= 0xffffc44030ULL) {
		request[(addr - 0xffffc44014ULL) / 4] = value;
		if (addr == 0xffffc44030ULL && value == 0xdeadbeaf)
			assert(response_reads == 7);
	} else if (addr == 0xffffc44010ULL && (value & 64)) {
		assert(started);
		unsigned int function = request[0] >> 24;

		assert(function == 1 || function == 2);
		assert(request[1] == 0x300);
		if (function == 1) {
			assert(requests > 0);
			assert(request[2] == 0x40420f00 && request[3] == request[2]);
			set_seen = true;
		}
		requests++;
		memset(reply, 0, sizeof(reply));
		reply[0] = function << 24 | 0x00c10002 |
			((function == 2 ? (short_reply ? 12 : 13) : 1) << 8);
		if (bad_header)
			reply[0] ^= 0x10000;
		be32((uint8_t *)reply + 9, set_seen ? 1000000 : 800000);
		be32((uint8_t *)reply + 13, bad_voltage ? 900000 : 1000000);
		polls = response_reads = 0;
		published = false;
	}
}

static void setbits32(void *p, uint32_t v) { write32(p, read32(p) | v); }
static void clrbits32(void *p, uint32_t v) { write32(p, read32(p) & ~v); }
static void clrsetbits32(void *p, uint32_t c, uint32_t v)
{
	write32(p, (read32(p) & ~c) | v);
}
void udelay(unsigned int us) { (void)us; }
void th1520_clean_data_cache(void) { cleaned = true; }
int th1520_cpu_set_clock(unsigned int mhz)
{
	clock_calls++;
	if (mhz == 300)
		return safe_clock_failure ? -1 : 0;
	assert(mhz == 1848 && set_seen && requests == 3);
	assert(published && response_reads == 7 && !bad_voltage);
	boosted = true;
	return 0;
}
void *cbfs_map(const char *name, size_t *size)
{
	assert(!strcmp(name, "fallback/aon"));
	*size = sizeof(blob);
	return bad_image ? NULL : blob;
}
void cbfs_unmap(void *p) { assert(p == blob); }
#include "../../../src/soc/thead/th1520/aon.c"

int main(void)
{
	memcpy(blob, "AON_CONFIG", 10);
	memcpy(blob + 22, "UBOOT_SET", 9);
	assert(!valid_image(NULL, 0));
	assert(!valid_image(blob, 0xc00));
	assert(!valid_image(blob, 0x10001));
	for (unsigned int test = 0; test < 7; test++) {
		bad_image = test == 1;
		timeout = test == 2;
		bad_header = test == 3;
		short_reply = test == 4;
		bad_voltage = test == 5;
		safe_clock_failure = test == 6;
		set_seen = cleaned = started = boosted = published = false;
		polls = response_reads = requests = clock_calls = reset_state = 0;
		aon_boot(NULL);
		assert(boosted == (test == 0));
		assert(reset_state == (test == 0 ? 3 : 0));
		if (test)
			assert(clock_calls == 1);
	}
	puts("PASS: AON load, completed mailbox replies and voltage-gated CPU boost");
}
