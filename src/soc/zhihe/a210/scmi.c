/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/scmi.h>
#include <timer.h>

#define SMT_STATUS 0x04
#define SMT_FLAGS 0x10
#define SMT_LENGTH 0x14
#define SMT_HEADER 0x18
#define SMT_PAYLOAD 0x1c
#define SMT_FREE (1U << 0)
#define MBOX_GENERATE 0x10
#define MBOX_NOTIFY (1U << 6)
#define SCMI_BASE 0x10
#define SCMI_CLOCK 0x14
#define SCMI_VERSION 0
#define SCMI_CLOCK_RATE_GET 6

static uint32_t token;
static bool failed;

static void *smt(unsigned int offset)
{
	return (void *)(uintptr_t)(A210_SCMI_SHMEM_BASE + offset);
}

static int wait_free(void)
{
	struct stopwatch timeout;

	stopwatch_init_msecs_expire(&timeout, 1000);
	while (!(read32(smt(SMT_STATUS)) & SMT_FREE)) {
		if (stopwatch_expired(&timeout)) {
			failed = true;
			return -1;
		}
		udelay(10);
	}
	return 0;
}

static int request(unsigned int protocol, unsigned int command,
		   const uint32_t *args, unsigned int count,
		   uint32_t *result, unsigned int words)
{
	uint32_t header = ((token++ & 0x3ff) << 18) | (protocol << 10) | command;

	if (failed || wait_free())
		return -1;
	write32(smt(SMT_STATUS), 0);
	write32(smt(SMT_FLAGS), 0);
	write32(smt(SMT_LENGTH), sizeof(uint32_t) * (count + 1));
	write32(smt(SMT_HEADER), header);
	for (unsigned int i = 0; i < count; i++)
		write32(smt(SMT_PAYLOAD + i * sizeof(uint32_t)), args[i]);
	asm volatile("fence iorw, iorw" ::: "memory");
	write32((void *)(A210_AON_MBOX_BASE + MBOX_GENERATE), MBOX_NOTIFY);
	if (wait_free())
		return -1;
	asm volatile("fence iorw, iorw" ::: "memory");
	if (read32(smt(SMT_STATUS)) != SMT_FREE ||
	    read32(smt(SMT_HEADER)) != header ||
	    read32(smt(SMT_LENGTH)) != sizeof(uint32_t) * (words + 2) ||
	    read32(smt(SMT_PAYLOAD))) {
		failed = true;
		return -1;
	}
	for (unsigned int i = 0; i < words; i++)
		result[i] = read32(smt(SMT_PAYLOAD + (i + 1) * sizeof(uint32_t)));
	return 0;
}

int a210_scmi_init(void)
{
	uint32_t version;

	if (request(SCMI_BASE, SCMI_VERSION, NULL, 0, &version, 1) ||
	    (version >> 16) < 1)
		return -1;
	printk(BIOS_INFO, "A210: SCMI Base %u.%u ready\n",
	       version >> 16, version & 0xffff);
	return 0;
}

int a210_scmi_clock_rate(unsigned int id, uint32_t *hz)
{
	uint32_t rate[2];

	if (request(SCMI_CLOCK, SCMI_CLOCK_RATE_GET, &id, 1, rate, ARRAY_SIZE(rate)) ||
	    !rate[0] || rate[1])
		return -1;
	*hz = rate[0];
	return 0;
}
