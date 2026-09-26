/* SPDX-License-Identifier: GPL-2.0-only */
OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS {
	. = 0x80200000;
	.text : { *(.text.entry) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) *(.sdata*) }
	. = ALIGN(16);
	__bss_start = .;
	.bss : { *(.bss*) *(.sbss*) }
	. = ALIGN(16);
	__bss_end = .;
}
