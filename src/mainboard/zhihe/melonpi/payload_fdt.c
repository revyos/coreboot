/* SPDX-License-Identifier: GPL-2.0-only */
#include <bootstate.h>
#include <cbfs.h>
#include <commonlib/device_tree.h>
#include <console/console.h>
#include <console/uart.h>
#include <device/mmio.h>
#include <mcall.h>
#include <soc/addressmap.h>
#include <soc/scmi.h>
#include <stdlib.h>

static void prepare_fdt(void *unused)
{
	void *original = cbfs_map("fallback/DTB", NULL);
	struct device_tree *tree;
	struct device_tree_node *memory;
	char memory_type[] = "memory";
	u64 address = A210_DRAM_BASE;
	u64 size = (u64)read32((void *)(A210_DIAGNOSTIC_STATUS + 8)) << 20;
	u32 uart_hz;

	if (!original || !(tree = fdt_unflatten(original)))
		die("Melon Pi: invalid payload device tree\n");
	memory = dt_find_prop_value(tree->root, "device_type", memory_type,
				    sizeof(memory_type));
	if (!memory)
		die("Melon Pi: payload device tree has no memory node\n");
	dt_add_reg_prop(memory, &address, &size, 1, 2, 2);
	struct device_tree_node *uart = dt_find_node_by_path(
	    tree, "/soc/serial@8401000", NULL, NULL, 0);

	if (!uart || a210_scmi_clock_rate(A210_SCMI_CLK_UART4, &uart_hz))
		die("Melon Pi: payload UART clock unavailable\n");
	dt_add_u32_prop(uart, "clock-frequency", uart_hz);
	dt_add_u32_prop(uart, "current-speed", get_uart_baudrate());
	void *flat = malloc(dt_flat_size(tree));

	if (!flat)
		die("Melon Pi: no memory for payload device tree\n");
	dt_flatten(tree, flat);
	cbfs_unmap(original);
	HLS()->fdt = flat;
	printk(BIOS_INFO, "Melon Pi: payload FDT ready, %llu MiB DDR\n",
	       (unsigned long long)(size >> 20));
}

BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_EXIT, prepare_fdt, NULL);
