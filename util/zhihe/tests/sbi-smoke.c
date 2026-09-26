/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>

struct sbiret { long error, value; };
struct report {
	unsigned long marchid;
	unsigned int generation;
	unsigned char padding[52];
} __attribute__((aligned(64)));
static struct report reports[8];
extern void secondary_entry(void);

static struct sbiret sbi(long ext, long fid, long x, long y, long z)
{
	register long a0 asm("a0") = x;
	register long a1 asm("a1") = y;
	register long a2 asm("a2") = z;
	register long a3 asm("a3") = 0;
	register long a4 asm("a4") = 0;
	register long a5 asm("a5") = 0;
	register long a6 asm("a6") = fid;
	register long a7 asm("a7") = ext;

	asm volatile("ecall" : "+r"(a0), "+r"(a1)
		: "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
		: "memory");
	return (struct sbiret){ a0, a1 };
}

static void puts(const char *s)
{
	while (*s) {
		unsigned char c = *s++;

		if (sbi(0x4442434e, 2, c, 0, 0).error)
			sbi(1, 0, c, 0, 0);
	}
}

static void hex(unsigned long value)
{
	const char *digits = "0123456789abcdef";
	char output[19] = "0x";

	for (int i = 0; i < 16; i++)
		output[i + 2] = digits[(value >> (60 - 4 * i)) & 15];
	puts(output);
}

static unsigned long ticks(void)
{
	unsigned long value;

	asm volatile("rdtime %0" : "=r"(value));
	return value;
}

void secondary(unsigned long hart, unsigned long generation)
{
	reports[hart].marchid = sbi(0x10, 5, 0, 0, 0).value;
	__atomic_store_n(&reports[hart].generation, generation,
			 __ATOMIC_RELEASE);
	sbi(0x48534d, 1, 0, 0, 0);
	for (;;)
		asm volatile("wfi");
}

void primary(unsigned long hart, const void *fdt)
{
	struct sbiret version = sbi(0x10, 0, 0, 0, 0);
	int passed = !version.error && sbi(0x10, 1, 0, 0, 0).value == 1;

	puts("\r\nMelon Pi: S-mode SBI test, hart ");
	hex(hart);
	puts(", FDT ");
	hex((uintptr_t)fdt);
	puts("\r\nSBI spec ");
	hex(version.value);
	puts(passed ? "\r\nOpenSBI S-mode handoff PASS\r\n" :
		"\r\nOpenSBI S-mode handoff FAIL\r\n");
	for (unsigned int round = 1; round <= 2 && passed; round++) {
		for (unsigned int id = 1; id < 8; id++) {
			struct sbiret ret = sbi(0x48534d, 0, id,
				(long)secondary_entry, round);
			unsigned long end = ticks() + 5 * 24000000UL;

			puts("HSM hart ");
			hex(id);
			puts(" start result ");
			hex(ret.error);
			if (ret.error) {
				passed = 0;
				break;
			}
			while (__atomic_load_n(&reports[id].generation,
					      __ATOMIC_ACQUIRE) != round &&
			       ticks() < end)
				;
			struct sbiret status;

			do {
				status = sbi(0x48534d, 2, id, 0, 0);
			} while (!status.error && status.value != 1 &&
				 ticks() < end);
			passed = !status.error && status.value == 1 &&
				reports[id].generation == round;
			puts(", marchid ");
			hex(reports[id].marchid);
			puts(passed ? " stopped\r\n" : " timeout\r\n");
			if (!passed)
				break;
		}
	}
	puts(passed ? "HSM start/stop/restart PASS: 8 harts\r\n" :
		"HSM start/stop/restart FAIL\r\n");
	for (;;)
		asm volatile("wfi");
}
