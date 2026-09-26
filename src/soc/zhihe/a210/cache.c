/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/encoding.h>
#include <program_loading.h>
#include <soc/early.h>
#include <soc/cpu.h>

void exit_car(void)
{
	/* Follow zero_stage_boot ordering with A210-specific L2 timing. */
	a210_write_csr(A210_CSR_MCOR, A210_MCOR_RESET_INIT);
	asm volatile("fence rw, rw" ::: "memory");
	a210_write_csr(A210_CSR_MHINT, A210_C908_MHINT_INIT);
	a210_set_csr(A210_CSR_MHINT2, A210_C908_MHINT2_DEBUG);
	a210_write_csr(A210_CSR_MCCR2, A210_C908_MCCR2_INIT);
	a210_write_csr(A210_CSR_MSMPR, A210_SMP_ENABLE);
	a210_write_csr(A210_CSR_MXSTATUS, A210_MXSTATUS_INIT);
	a210_set_csr(A210_CSR_MHINT4, A210_MHINT4_L2_FREE_WRITE);
	a210_write_csr(A210_CSR_MHCR, A210_C908_MHCR_INIT);
	asm volatile("fence iorw, iorw; fence.i" ::: "memory");
}

void platform_segment_loaded(uintptr_t start, size_t size, int flags)
{
	/* Publish payloads before OpenSBI enables other harts. */
	asm volatile("fence rw, rw; .long 0x0010000b; fence.i" ::
			 : "memory");
}
