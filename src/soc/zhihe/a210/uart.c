/* SPDX-License-Identifier: GPL-2.0-only */
#include <console/uart.h>
#include <boot/coreboot_tables.h>
#include <commonlib/coreboot_tables.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <soc/clock.h>
#include <drivers/uart/uart8250reg.h>

#define UART_REG(reg) (A210_UART4_BASE + 4 * (reg))

/* Preserve the console clock, baud rate and pins during SRAM bring-up. */
void uart_init(unsigned int idx) {}

uintptr_t uart_platform_base(unsigned int idx) { return A210_UART4_BASE; }

unsigned int uart_platform_refclk(void) { return a210_uart_refclk(); }

enum cb_err fill_lb_serial(struct lb_serial *serial)
{
	serial->type = LB_SERIAL_TYPE_MEMORY_MAPPED;
	serial->baseaddr = A210_UART4_BASE;
	serial->baud = get_uart_baudrate();
	serial->regwidth = 4;
	serial->input_hertz = uart_platform_refclk();
	return serial->input_hertz ? CB_SUCCESS : CB_ERR;
}

void uart_tx_byte(unsigned int idx, unsigned char data)
{
	for (unsigned int tries = 0; tries < 1000000; tries++) {
		if (read32((void *)UART_REG(UART8250_LSR)) &
		    UART8250_LSR_THRE) {
			write32((void *)UART_REG(UART8250_TBR), data);
			return;
		}
	}
}

void uart_tx_flush(unsigned int idx)
{
	for (unsigned int tries = 0; tries < 1000000; tries++)
		if (read32((void *)UART_REG(UART8250_LSR)) &
		    UART8250_LSR_TEMT)
			return;
}

unsigned char uart_rx_byte(unsigned int idx)
{
	return read32((void *)UART_REG(UART8250_RBR));
}
