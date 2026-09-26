/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/console.h>
#include <soc/early.h>
#include "board.h"

int melonpi_check_board(void)
{
	printk(BIOS_INFO, "%s: configured carrier revision\n",
	       CONFIG_MAINBOARD_PART_NUMBER);
	a210_adc_init();
	int ch0 = a210_adc_read_mv(0);
	int ch2 = a210_adc_read_mv(2);

	if (ch0 < 0 || ch2 < 0)
		return -1;
	printk(BIOS_INFO, "Melon Pi: board ADC ch0=%d mV ch2=%d mV\n",
		ch0, ch2);
	/* The resistor code selects a profile, not installed capacity. */
	if (ch2 < 1700 || ch2 > 1900) {
		printk(BIOS_ERR, "Melon Pi: unsupported memory straps\n");
		return -1;
	}
	return 0;
}
