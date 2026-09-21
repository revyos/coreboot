/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/uart.h>
#include <soc/addressmap.h>

uintptr_t uart_platform_base(unsigned int idx)
{
	return TH1520_UART0_BASE;
}

unsigned int uart_platform_refclk(void)
{
	return 100000000;
}
