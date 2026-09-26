/* SPDX-License-Identifier: GPL-2.0-only */
.section .text.entry, "ax", %progbits
.global _start
_start:
	la t0, __bss_start
	la t1, __bss_end
1:
	bgeu t0, t1, 2f
	sd zero, 0(t0)
	addi t0, t0, 8
	j 1b
2:
	la sp, stacks
	li t0, 4096
	add sp, sp, t0
	tail primary
.global secondary_entry
.balign 4
secondary_entry:
	la sp, stacks
	addi t0, a0, 1
	slli t0, t0, 12
	add sp, sp, t0
	tail secondary
.section .bss.stack, "aw", %nobits
.balign 16
stacks:
.space 32768
