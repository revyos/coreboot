/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
/* TH1520 AON image and mailbox ABI shared with the existing U-Boot port. */
#include <bootstate.h>
#include <cbfs.h>
#include <console/console.h>
#include <delay.h>
#include <device/mmio.h>
#include <soc/cpu.h>
#include <string.h>
#include <timer.h>

#define AON_BASE 0xffffef8000ULL
#define AON_RESET ((void *)0xfffff44024ULL)
#define AON_START ((void *)0xfffff48044ULL)
#define AON_IOPMP ((void *)0xffffc210c0ULL)
#define AON_PAD 0xfffff4a000ULL
#define MBOX_LOCAL 0xffffc39000ULL
#define MBOX_REMOTE 0xffffc44000ULL
#define MBOX_GENERATE 0x10
#define MBOX_INFO(n) (0x14 + 4 * (n))
#define CPU_UV 1000000U

static uint32_t get_be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint32_t voltage_word(void)
{
	/* The mailbox voltage fields use network byte order. */
	return (CPU_UV >> 24) | ((CPU_UV >> 8) & 0xff00) |
		((CPU_UV << 8) & 0xff0000) | (CPU_UV << 24);
}

static int pmic_request(unsigned int function, uint32_t response[7])
{
	uint32_t request[7] = { function << 24 | 0x00010702, 0x00000300 };

	if (function == 1)
		request[2] = request[3] = request[4] = voltage_word();
	setbits32((void *)0xffef010208ULL, (1U << 6) | (1U << 7));
	clrbits32((void *)(MBOX_LOCAL + MBOX_GENERATE), 0xff);
	clrbits32((void *)(MBOX_REMOTE + MBOX_GENERATE), 0xff);
	write32((void *)(MBOX_LOCAL + MBOX_INFO(0)), 0);
	write32((void *)(MBOX_REMOTE + MBOX_INFO(7)), 0);
	for (unsigned int i = 1; i < 7; i++)
		write32((void *)(MBOX_REMOTE + MBOX_INFO(i)), request[i]);
	write32((void *)(MBOX_REMOTE + MBOX_INFO(0)), request[0]);
	setbits32((void *)(MBOX_REMOTE + MBOX_GENERATE), 1U << 6);
	/* INFO0 alone is not a completed response: wait for its doorbell. */
	if (!wait_us(100000, read32((void *)(MBOX_LOCAL + MBOX_GENERATE)) & (1U << 6)))
		return -1;
	for (unsigned int i = 0; i < 7; i++)
		response[i] = read32((void *)(MBOX_LOCAL + MBOX_INFO(i)));
	write32((void *)(MBOX_LOCAL + MBOX_INFO(0)), 0);
	clrbits32((void *)(MBOX_LOCAL + MBOX_GENERATE), 0xff);
	write32((void *)(MBOX_REMOTE + MBOX_INFO(7)), 0xdeadbeaf);
	udelay(1000);
	if ((response[0] & 0xffff00ff) != (function << 24 | 0x00c10002) ||
	    ((response[0] >> 8) & 0xff) < (function == 2 ? 13 : 1))
		return -1;
	return (response[1] & 0xff) ? -1 : 0;
}

static int set_cpu_voltage(void)
{
	uint32_t response[7];
	int ret = -1;

	/* E902 initializes the PMIC and mailbox asynchronously. */
	for (unsigned int attempt = 0; attempt < 20; attempt++) {
		ret = pmic_request(2, response);
		if (!ret)
			break;
	}
	if (ret || pmic_request(1, response) || pmic_request(2, response))
		return -1;
	uint32_t core = get_be32((uint8_t *)response + 9);
	uint32_t memory = get_be32((uint8_t *)response + 13);

	printk(BIOS_INFO, "TH1520 AON: CPU rails %u/%u uV\n", core, memory);
	return core == CPU_UV && memory == CPU_UV ? 0 : -1;
}

static bool valid_image(const void *image, size_t size)
{
	return image && size > 0xc00 && size <= 0x10000 &&
		!memcmp(image, "AON_CONFIG", 10) &&
		!memcmp((const uint8_t *)image + 22, "UBOOT_SET", 9);
}

static int load_aon(void)
{
	size_t size = 0;
	uint8_t *image = cbfs_map("fallback/aon", &size);

	if (!valid_image(image, size)) {
		if (image)
			cbfs_unmap(image);
		return -1;
	}
	write32(AON_RESET, 0);
	/* Release the audio bus used by AON, retaining C906 in reset. */
	write32((void *)0xfffff4403cULL, 0x37);
	clrsetbits32((void *)(AON_PAD + 0x10), 0xffff0000, 0x02080000);
	clrsetbits32((void *)(AON_PAD + 0x14), 0xffff, 0x0208);
	for (size_t offset = 0; offset < size; offset += 4) {
		uint32_t word = 0;

		memcpy(&word, image + offset, MIN(4, size - offset));
		write32((void *)(AON_BASE + offset), word);
	}
	th1520_clean_data_cache();
	cbfs_unmap(image);
	write32(AON_IOPMP, UINT32_MAX);
	write32(AON_START, 0xffef8c00);
	write32(AON_RESET, 3);
	printk(BIOS_INFO, "TH1520: AON started (%zu bytes)\n", size);
	return 0;
}

static void aon_boot(void *unused)
{
	/* Never let AON lower voltage while the CPU is at a fast operating point. */
	if (th1520_cpu_set_clock(300)) {
		printk(BIOS_ERR, "TH1520: cannot establish safe CPU clock for AON\n");
		return;
	}
	if (load_aon() || set_cpu_voltage()) {
		write32(AON_RESET, 0);
		printk(BIOS_ERR, "TH1520: AON/voltage setup failed; keeping 300 MHz\n");
		return;
	}
	if (th1520_cpu_set_clock(1848))
		printk(BIOS_ERR, "TH1520: CPU boost failed; voltage is confirmed safe\n");
}

BOOT_STATE_INIT_ENTRY(BS_POST_DEVICE, BS_ON_ENTRY, aon_boot, NULL);
