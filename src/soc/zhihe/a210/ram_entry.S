/* SPDX-License-Identifier: GPL-2.0-only */
#include <soc/addressmap.h>

.section .text, "ax", %progbits
.option norelax
.global a210_ram_entry
a210_ram_entry:
	/* BOOTBLOCK is the first FMAP region in the downloaded CBFS image. */
	li t0, A210_RAM_IMAGE_BASE
	li t1, A210_DDR_ENTRY
	mv t4, t1
	li t2, A210_DDR_IMAGE_SIZE
	add t2, t0, t2
1:
	lw t3, 0(t0)
	sw t3, 0(t1)
	addi t0, t0, 4
	addi t1, t1, 4
	bltu t0, t2, 1b
	fence iorw, iorw
	fence.i
	jr t4
