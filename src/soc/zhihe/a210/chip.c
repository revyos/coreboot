/* SPDX-License-Identifier: GPL-2.0-only */
#include <device/device.h>
#include <device/mmio.h>
#include <soc/addressmap.h>

static void read_resources(struct device *dev)
{
	uint64_t size = (uint64_t)read32(
		(void *)(A210_DIAGNOSTIC_STATUS + 8)) << 20;

	ram_from_to(dev, 0, A210_DRAM_BASE, A210_DRAM_BASE + size);
}

static struct device_operations ops = {
	.read_resources = read_resources,
	.set_resources = noop_set_resources,
};

static void enable(struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_CPU_CLUSTER)
		dev->ops = &ops;
}

struct chip_operations soc_zhihe_a210_ops = {
	.name = "Zhihe A210",
	.enable_dev = enable,
};
