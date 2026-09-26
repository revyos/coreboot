/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/early.h>

#define IOPMP_CONFIG_STRIDE 4
#define IOPMP_ADDRESS_BASE 0x800
#define IOPMP_GROUP_STRIDE 0x20
#define IOPMP_GROUP_COUNT 16
#define IOPMP_DMAC_AP_ID 0x02
#define IOPMP_USB_DFMU_ID 0x09
#define IOPMP_USB3_ID 0x0a
#define IOPMP_USB2_0_ID 0x0b
#define IOPMP_USB2_1_ID 0x0c
#define IOPMP_PCIE_DFMU_ID 0x10
#define IOPMP_PCIE0_ID 0x11
#define IOPMP_PCIE1_ID 0x12
#define IOPMP_SATA_ID 0x14
#define IOPMP_DEVICE_SHIFT 14
#define IOPMP_TOR_RWX_PAIR 0x0f0f
#define IOPMP_ADDRESS_LIMIT_HIGH 0x20
#define IOPMP_AON_ID 0x37
#define IOPMP_PERI1_ID 1
#define IOPMP_GMAC0_ID 3
#define IOPMP_GMAC1_ID 4
#define IOPMP_GMAC2_ID 5
#define IOPMP_DEBUG_ID 0x3e
#define IOPMP_PAGE_WALKER_ID 0
#define IOPMP_VO_ID 0x2b
#define IOPMP_DISPLAY0_ID 0x2c
#define IOPMP_DISPLAY1_ID 0x2d
#define IOPMP_AUX_DISPLAY_ID 0x2e

static int allow_device(uintptr_t base, unsigned int device,
			unsigned int group)
{
	uintptr_t config = base +
		group * IOPMP_CONFIG_STRIDE;
	uintptr_t address = base + IOPMP_ADDRESS_BASE +
		group * IOPMP_GROUP_STRIDE;
	uint32_t tag = device << IOPMP_DEVICE_SHIFT;

	/* Vendor TOR pair for this master: [0, 2^39), without locking. */
	write32((void *)config, 0);
	write32((void *)address, 0);
	write32((void *)(address + 4), tag);
	write32((void *)(address + 8), 0);
	write32((void *)(address + 12), tag | IOPMP_ADDRESS_LIMIT_HIGH);
	write32((void *)config, IOPMP_TOR_RWX_PAIR);
	return read32((void *)config) == IOPMP_TOR_RWX_PAIR &&
		read32((void *)(address + 4)) == tag &&
		read32((void *)(address + 12)) ==
			(tag | IOPMP_ADDRESS_LIMIT_HIGH) ? 0 : -1;
}

int a210_peri1_dma_init(void)
{
	static const unsigned int devices[] = {
		IOPMP_AON_ID, IOPMP_PERI1_ID, IOPMP_GMAC0_ID,
		IOPMP_GMAC1_ID, IOPMP_GMAC2_ID, IOPMP_DEBUG_ID,
		IOPMP_PAGE_WALKER_ID,
	};

	/* Preserve debug access before replacing BootROM's group 1. */
	if (allow_device(A210_PERI1_IOPMP_BASE, IOPMP_DEBUG_ID, 5))
		return -1;
	for (unsigned int i = 0; i < ARRAY_SIZE(devices); i++)
		if (allow_device(A210_PERI1_IOPMP_BASE, devices[i], i))
			return -1;
	printk(BIOS_INFO, "A210: PERI1 DMA access enabled\n");
	return 0;
}

int a210_display_dma_init(void)
{
	/* VO, both display channels, auxiliary display and page walker. */
	static const unsigned int devices[] = {
		IOPMP_VO_ID, IOPMP_DISPLAY0_ID, IOPMP_DISPLAY1_ID,
		IOPMP_AUX_DISPLAY_ID, IOPMP_PAGE_WALKER_ID,
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(devices); i++)
		if (allow_device(A210_VO_IOPMP_BASE, devices[i], i))
			return -1;
	return 0;
}

static int allow_devices(uintptr_t base, const unsigned int *devices,
			 unsigned int count)
{
	/* Preserve entries owned by other masters in the shared block. */
	for (unsigned int i = 0; i < count; i++) {
		unsigned int slot = IOPMP_GROUP_COUNT;

		for (unsigned int g = 0; g < IOPMP_GROUP_COUNT; g++) {
			uintptr_t cfg = base +
				g * IOPMP_CONFIG_STRIDE;
			uintptr_t addr = base +
				IOPMP_ADDRESS_BASE + g * IOPMP_GROUP_STRIDE;

			if (!read32((void *)cfg)) {
				if (slot == IOPMP_GROUP_COUNT)
					slot = g;
			} else if ((read32((void *)(addr + 4)) >>
				    IOPMP_DEVICE_SHIFT) == devices[i]) {
				slot = g;
				break;
			}
		}
		if (slot == IOPMP_GROUP_COUNT ||
		    allow_device(base, devices[i], slot))
			return -1;
	}
	return 0;
}

int a210_pcie_dma_init(void)
{
	static const unsigned int devices[] = {
		IOPMP_PCIE_DFMU_ID, IOPMP_PCIE0_ID, IOPMP_PCIE1_ID,
		IOPMP_SATA_ID, IOPMP_DMAC_AP_ID,
	};

	if (allow_devices(A210_PCIE_IOPMP_BASE, devices, ARRAY_SIZE(devices)))
		return -1;
	printk(BIOS_INFO, "A210: PCIe/SATA DMA access enabled\n");
	return 0;
}

int a210_usb_dma_init(void)
{
	static const unsigned int devices[] = {
		IOPMP_USB_DFMU_ID, IOPMP_USB3_ID,
		IOPMP_USB2_0_ID, IOPMP_USB2_1_ID,
	};

	if (allow_devices(A210_USB_IOPMP_BASE, devices, ARRAY_SIZE(devices)))
		return -1;
	printk(BIOS_INFO, "A210: USB DMA access enabled\n");
	return 0;
}
