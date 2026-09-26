/* SPDX-License-Identifier: GPL-2.0-only */
#include <cbfs.h>
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/aon.h>
#include <string.h>
#include <timer.h>

#define AON_VECTOR (A210_AON_SYSREG_BASE + 0x24)
#define AON_RESET (A210_AON_RESET_BASE + 0x18)
#define AON_RESET_RELEASE (1U << 1)
#define AON_READY (A210_AON_SYSREG_BASE + 0x144)
#define AON_READY_MAGIC 0xfeabdeff
#define PERI0_CLOCK_ENABLE (A210_PERI0_SYSREG_BASE + 0x200)
#define PERI0_RESET_N (A210_PERI0_SYSREG_BASE + 0x400)
#define PERI0_MBOX_ENABLE ((1U << 5) | (1U << 6))
#define MBOX_CLEAR 0x04
#define MBOX_GENERATE 0x10
#define MBOX_INFO0 0x14
#define MBOX_ACK 0x30
#define MBOX_DATA_IRQ (1U << 6)
#define MBOX_ACK_IRQ (1U << 7)
#define MBOX_ACK_MAGIC 0xdeadbeaf
#define AON_RPC_BYTES 28
#define AON_RPC_WORDS 7
#define AON_RPC_VERSION 2
#define AON_RPC_PM 1
#define AON_RPC_ACK (1U << 7)
#define AON_SET_VOLTAGE 1
#define AON_GET_VOLTAGE 2

static bool ready;

int a210_aon_start(void)
{
	struct stopwatch deadline;
	size_t size;
	void *image = cbfs_map("fallback/aon", &size);

	ready = false;
	if (!image || !size || size > A210_AON_SRAM_SIZE) {
		if (image)
			cbfs_unmap(image);
		printk(BIOS_ERR, "A210: missing or oversized AON image\n");
		return -1;
	}
	setbits32((void *)PERI0_CLOCK_ENABLE, PERI0_MBOX_ENABLE);
	setbits32((void *)PERI0_RESET_N, PERI0_MBOX_ENABLE);
	write32((void *)AON_RESET, 0);
	udelay(10);
	write32((void *)AON_READY, 0);
	memset((void *)A210_AON_SRAM_BASE, 0, A210_AON_SRAM_SIZE);
	memcpy((void *)A210_AON_SRAM_BASE, image, size);
	/* E902 does not share the application CPUs' coherent caches. */
	asm volatile("fence rw, rw; .long 0x0010000b; fence.i" ::
		     : "memory");
	if (memcmp((void *)A210_AON_SRAM_BASE, image, size)) {
		cbfs_unmap(image);
		return -1;
	}
	cbfs_unmap(image);
	write32((void *)AON_VECTOR, A210_AON_SRAM_BASE);
	write32((void *)(A210_AP_MBOX_BASE + MBOX_INFO0), 0);
	write32((void *)(A210_AP_MBOX_BASE + MBOX_ACK), 0);
	write32((void *)(A210_AON_MBOX_BASE + MBOX_ACK), 0);
	write32((void *)AON_RESET, AON_RESET_RELEASE);
	stopwatch_init_msecs_expire(&deadline, 1000);
	while (read32((void *)AON_READY) != AON_READY_MAGIC) {
		if (stopwatch_expired(&deadline)) {
			printk(BIOS_ERR, "A210: AON startup timed out\n");
			return -1;
		}
		udelay(10);
	}
	ready = true;
	printk(BIOS_INFO, "A210: AON ready, %zu bytes\n", size);
	return 0;
}

static int rpc(unsigned int function, unsigned int resource,
	       unsigned int uv, unsigned int *result)
{
	uint8_t msg[AON_RPC_BYTES] = {
		AON_RPC_VERSION, AON_RPC_WORDS, AON_RPC_PM, function,
	};
	struct stopwatch deadline;
	uint32_t word;

	if (!ready || resource > 0xffff)
		return -1;
	msg[4] = resource >> 8;
	msg[5] = resource;
	msg[8] = uv >> 24;
	msg[9] = uv >> 16;
	msg[10] = uv >> 8;
	msg[11] = uv;
	write32((void *)(A210_AP_MBOX_BASE + MBOX_INFO0), 0);
	write32((void *)(A210_AP_MBOX_BASE + MBOX_ACK), 0);
	write32((void *)(A210_AON_MBOX_BASE + MBOX_ACK), 0);
	for (unsigned int i = 0; i < AON_RPC_WORDS; i++) {
		memcpy(&word, msg + i * sizeof(word), sizeof(word));
		write32((void *)(A210_AON_MBOX_BASE + MBOX_INFO0 +
				i * sizeof(word)), word);
	}
	asm volatile("fence iorw, iorw" ::: "memory");
	write32((void *)(A210_AON_MBOX_BASE + MBOX_GENERATE),
		MBOX_DATA_IRQ);
	stopwatch_init_msecs_expire(&deadline, 1000);
	while (!read32((void *)(A210_AP_MBOX_BASE + MBOX_INFO0))) {
		if (stopwatch_expired(&deadline)) {
			/* A late reply prevents safe channel reuse. */
			ready = false;
			printk(BIOS_ERR, "A210: AON RPC %u timed out\n",
			       function);
			return -1;
		}
		udelay(10);
	}
	for (unsigned int i = 0; i < AON_RPC_WORDS; i++) {
		word = read32((void *)(A210_AP_MBOX_BASE + MBOX_INFO0 +
				      i * sizeof(word)));
		memcpy(msg + i * sizeof(word), &word, sizeof(word));
	}
	write32((void *)(A210_AP_MBOX_BASE + MBOX_INFO0), 0);
	write32((void *)(A210_AON_MBOX_BASE + MBOX_ACK), MBOX_ACK_MAGIC);
	write32((void *)(A210_AON_MBOX_BASE + MBOX_GENERATE),
		MBOX_ACK_IRQ);
	write32((void *)(A210_AP_MBOX_IRQ_BASE + MBOX_CLEAR), 1);
	if (msg[0] != AON_RPC_VERSION || msg[1] < 1 || msg[1] > 24 ||
	    (msg[2] & 0x3f) != AON_RPC_PM || !(msg[2] & AON_RPC_ACK) ||
	    msg[3] != function || msg[4]) {
		printk(BIOS_ERR, "A210: AON RPC %u failed, error %u\n",
		       function, msg[4]);
		return -1;
	}
	if (result) {
		if (msg[1] < 9)
			return -1;
		*result = (uint32_t)msg[9] << 24 |
			  (uint32_t)msg[10] << 16 |
			  (uint32_t)msg[11] << 8 | msg[12];
	}
	return 0;
}

int a210_aon_set_voltage(unsigned int resource, unsigned int uv)
{
	return rpc(AON_SET_VOLTAGE, resource, uv, NULL);
}

int a210_aon_get_voltage(unsigned int resource, unsigned int *uv)
{
	if (!uv)
		return -1;
	return rpc(AON_GET_VOLTAGE, resource, 0, uv);
}
