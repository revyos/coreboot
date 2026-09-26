/* SPDX-License-Identifier: GPL-2.0-only */
#include <soc/cpu.h>

.section .text.cache_as_ram, "ax", %progbits
.global cache_as_ram
cache_as_ram:
	/* This diagnostic uses existing BRAM and UART configuration. */
	csrw mie, zero
	csrci mstatus, 8
	li t0, 0x20000
	csrc mstatus, t0
	csrr t0, mhartid
	bnez t0, .Lpark
	csrw satp, zero
	sfence.vma
	csrw mscratch, zero
	/* BootROM enables D-cache. Keep memory bring-up uncached. */
	csrr t0, A210_CSR_MHCR
	andi t1, t0, 2
	beqz t1, .Luncached
	fence iorw, iorw
	.long 0x0010000b /* th.dcache.call */
	fence iorw, iorw
	andi t0, t0, -3
	csrw A210_CSR_MHCR, t0
	.long 0x0020000b /* th.dcache.iall */
.Luncached:
	li t0, A210_DIAGNOSTIC_STATUS
	li t1, 0x41323130
	sw t1, 0(t0)
	sw zero, 8(t0)
	sw zero, 12(t0)
	li t1, 1
	sw t1, 4(t0)
	fence iorw, iorw
	fence.i
	ret
.Lpark:
	wfi
	j .Lpark
