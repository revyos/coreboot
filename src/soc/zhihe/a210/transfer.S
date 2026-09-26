/* SPDX-License-Identifier: GPL-2.0-only */
.section .text.a210_transfer, "ax", %progbits
.balign 4
.global a210_transfer_start
.global a210_transfer_end
a210_transfer_start:
	/* Position-independent, stackless copy from OCRAM to BRAM. */
	mv t2, a1
	add a2, a0, a2
1:
	lw t0, 0(a0)
	sw t0, 0(a1)
	addi a0, a0, 4
	addi a1, a1, 4
	bltu a0, a2, 1b
	fence iorw, iorw
	fence.i
	jr t2
a210_transfer_end:
