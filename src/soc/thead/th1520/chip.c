/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <device/device.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/iopmp.h>

static void th1520_init(void *unused)
{
	th1520_iopmp_init();
}

static void th1520_read_resources(struct device *dev)
{
	ram_from_to(dev, 0, 0, read64((void *)TH1520_DDR_SIZE_ADDR));
}

static struct device_operations th1520_ops = {
	.read_resources = th1520_read_resources,
	.set_resources = noop_set_resources,
};

static void th1520_enable(struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_CPU_CLUSTER)
		dev->ops = &th1520_ops;
}

struct chip_operations soc_thead_th1520_ops = {
	.name = "T-Head TH1520",
	.init = th1520_init,
	.enable_dev = th1520_enable,
};
