/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */
#include <bootstate.h>
#include <cbfs.h>
#include <commonlib/device_tree.h>
#include <console/console.h>
#include <device/mmio.h>
#include <mcall.h>
#include <soc/addressmap.h>
#include <stdlib.h>

static void prepare_payload_fdt(void *unused)
{
	void *original = cbfs_map("fallback/DTB", NULL);
	struct device_tree *tree;
	struct device_tree_node *memory;
	u64 address = 0, size = read64((void *)TH1520_DDR_SIZE_ADDR);

	if (!original || !(tree = fdt_unflatten(original)))
		die("TH1520: invalid payload device tree");
	memory = dt_find_node_by_path(tree, "/memory@0", NULL, NULL, 0);
	if (!memory)
		die("TH1520: payload device tree has no memory node");
	dt_add_reg_prop(memory, &address, &size, 1, 2, 2);
	/* OpenSBI adds its own runtime reservations before entering S-mode. */

	void *flat = malloc(dt_flat_size(tree));

	if (!flat)
		die("TH1520: cannot allocate payload device tree");
	dt_flatten(tree, flat);
	cbfs_unmap(original);
	for (unsigned int hart = 0; hart < CONFIG_MAX_CPUS; hart++)
		OTHER_HLS(hart)->fdt = flat;
	printk(BIOS_INFO, "TH1520: payload FDT ready for OpenSBI/EDK2\n");
}

BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_EXIT, prepare_payload_fdt, NULL);
