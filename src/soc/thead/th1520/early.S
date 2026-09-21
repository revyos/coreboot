/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2025 Yao Zi <ziyao@disroot.org>
 * Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn>
 * Based on the C910 setup in U-Boot arch/riscv/cpu/th1520.
 */

.section .text.cache_as_ram, "ax", %progbits
.global cache_as_ram
cache_as_ram:
	/* BootROM provides SRAM; no cache-as-RAM is required. */
	csrw mie, zero
	csrci mstatus, 8
	csrw satp, zero
	li t0, 0x638000
	csrw 0x7c0, t0
	li t0, 0x6000
	csrs mstatus, t0
	/* Cache instructions in SRAM; keep data uncached for DDR probing. */
	li t0, 1
	csrw 0x7c1, t0
	li t0, 0x70013
	csrw 0x7c2, t0
	li t0, 1
	csrs 0x7f3, t0
	fence.i
	/* BootROM does not supply a flattened device tree. */
	csrw mscratch, zero
	ret
