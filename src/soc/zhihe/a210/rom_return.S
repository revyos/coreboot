/* SPDX-License-Identifier: GPL-2.0-only */
#include <soc/cpu.h>

.section .text.a210_return_to_rom, "ax", %progbits
.global a210_return_to_rom
a210_return_to_rom:
	li t0, A210_ROM_CONTEXT
	ld t1, 128(t0)
	li t2, 0x6000
	and t2, t1, t2
	beqz t2, 1f
	ld t2, 160(t0)
	fscsr t2
	fld fs0, 168(t0)
	fld fs1, 176(t0)
	fld fs2, 184(t0)
	fld fs3, 192(t0)
	fld fs4, 200(t0)
	fld fs5, 208(t0)
	fld fs6, 216(t0)
	fld fs7, 224(t0)
	fld fs8, 232(t0)
	fld fs9, 240(t0)
	fld fs10, 248(t0)
	fld fs11, 256(t0)
1:
	ld t2, 264(t0)
	csrw mscratch, t2
	ld t2, 272(t0)
	csrw medeleg, t2
	ld t2, 280(t0)
	csrw mideleg, t2
	ld t2, 288(t0)
	csrw mcounteren, t2
	ld t2, 296(t0)
	csrw satp, t2
	sfence.vma
	ld t2, 144(t0)
	csrw mtvec, t2
	ld t2, 152(t0)
	fence iorw, iorw
	csrw A210_CSR_MHCR, t2
	fence.i
	ld ra, 0(t0)
	ld sp, 8(t0)
	ld gp, 16(t0)
	ld tp, 24(t0)
	ld s0, 32(t0)
	ld s1, 40(t0)
	ld s2, 48(t0)
	ld s3, 56(t0)
	ld s4, 64(t0)
	ld s5, 72(t0)
	ld s6, 80(t0)
	ld s7, 88(t0)
	ld s8, 96(t0)
	ld s9, 104(t0)
	ld s10, 112(t0)
	ld s11, 120(t0)
	ld t2, 136(t0)
	csrw mie, t2
	li a0, 0
	csrw mstatus, t1
	ret
