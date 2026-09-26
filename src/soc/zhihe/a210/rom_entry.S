/* SPDX-License-Identifier: GPL-2.0-only */
#include <soc/cpu.h>

.section .text, "ax", %progbits
.option norelax
.global a210_rom_entry
a210_rom_entry:
	/* Preserve the BootROM caller before coreboot sets up its own stack. */
	li t0, A210_ROM_CONTEXT
	sd ra, 0(t0)
	sd sp, 8(t0)
	sd gp, 16(t0)
	sd tp, 24(t0)
	sd s0, 32(t0)
	sd s1, 40(t0)
	sd s2, 48(t0)
	sd s3, 56(t0)
	sd s4, 64(t0)
	sd s5, 72(t0)
	sd s6, 80(t0)
	sd s7, 88(t0)
	sd s8, 96(t0)
	sd s9, 104(t0)
	sd s10, 112(t0)
	sd s11, 120(t0)
	csrr t1, mstatus
	sd t1, 128(t0)
	csrr t2, mie
	sd t2, 136(t0)
	csrr t2, mtvec
	sd t2, 144(t0)
	csrr t2, A210_CSR_MHCR
	sd t2, 152(t0)
	csrr t2, mscratch
	sd t2, 264(t0)
	csrr t2, medeleg
	sd t2, 272(t0)
	csrr t2, mideleg
	sd t2, 280(t0)
	csrr t2, mcounteren
	sd t2, 288(t0)
	csrr t2, satp
	sd t2, 296(t0)
	li t2, 0x6000
	and t1, t1, t2
	beqz t1, 1f
	frcsr t1
	sd t1, 160(t0)
	fsd fs0, 168(t0)
	fsd fs1, 176(t0)
	fsd fs2, 184(t0)
	fsd fs3, 192(t0)
	fsd fs4, 200(t0)
	fsd fs5, 208(t0)
	fsd fs6, 216(t0)
	fsd fs7, 224(t0)
	fsd fs8, 232(t0)
	fsd fs9, 240(t0)
	fsd fs10, 248(t0)
	fsd fs11, 256(t0)
1:
	li t0, A210_BOOTSTRAP_ENTRY
	jr t0
