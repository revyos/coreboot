/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_ZHIHE_A210_CPU_H
#define SOC_ZHIHE_A210_CPU_H

#include <soc/addressmap.h>

#define A210_HART_COUNT 8
#define A210_CORES_PER_CLUSTER 4
#define A210_CPU_CLUSTER_STRIDE 0x4000
#define A210_CPU_CORE_STRIDE 0x200
#define A210_C920_CLUSTER 1
#define A210_CLUSTER_POWER_BASE(cluster) \
	(A210_CPU_POWER_BASE + (cluster) * A210_CPU_CLUSTER_STRIDE)
#define A210_CORE_POWER_BASE(hart) \
	(A210_CLUSTER_POWER_BASE((hart) / A210_CORES_PER_CLUSTER) - \
	 (hart) % A210_CORES_PER_CLUSTER * A210_CPU_CORE_STRIDE)
#define A210_CORE_BPC(hart) (A210_CORE_POWER_BASE(hart) + 0x0c00)
#define A210_CORE_PCU(hart) (A210_CORE_POWER_BASE(hart) + 0x1600)
#define A210_CORE_PPU(hart) (A210_CORE_POWER_BASE(hart) + 0x2000)
#define A210_CLUSTER_BPC(cluster) \
	(A210_CLUSTER_POWER_BASE(cluster) + 0x0e00)
#define A210_CLUSTER_PCU(cluster) \
	(A210_CLUSTER_POWER_BASE(cluster) + 0x1800)
#define A210_CLUSTER_PPU(cluster) \
	(A210_CLUSTER_POWER_BASE(cluster) + 0x2200)

#define A210_C908_RESET_CTRL (A210_CPU_RESET_BASE + 0x04)
#define A210_C920_RESET_CTRL (A210_CPU_RESET_BASE + 0x08)
#define CPU_CLUSTER_RESET_RELEASE (1U << 0)
#define CPU_CORE_RESET_RELEASE(core) (1U << ((core) + 1))
#define CPU_CORE_RESET_MASK 0x1e
#define CPU_RESET_MASK (CPU_CLUSTER_RESET_RELEASE | CPU_CORE_RESET_MASK)

#define A210_CSR_MXSTATUS 0x7c0
#define A210_CSR_MHCR 0x7c1
#define A210_CSR_MCOR 0x7c2
#define A210_CSR_MCCR2 0x7c3
#define A210_CSR_MHINT 0x7c5
#define A210_CSR_MHINT2 0x7cc
#define A210_CSR_MHINT4 0x7ce
#define A210_CSR_MSMPR 0x7f3
#define A210_MHCR_DCACHE_ENABLE 2
#define A210_MCOR_RESET_INIT 0x70013
#define A210_C908_MHINT_INIT 0x0212a10c
#define A210_C908_MCCR2_INIT 0xa2490008
#define A210_C908_MHCR_INIT 0x010011ff
#define A210_MXSTATUS_INIT 0x438000
#define A210_MHINT4_L2_FREE_WRITE (1U << 13)
#define A210_C908_MHINT2_DEBUG (1ULL << 33)
#define A210_SMP_ENABLE 1

/* Expand numeric CSR definitions before arch helpers stringify them. */
#define a210_read_csr(reg) read_csr(reg)
#define a210_write_csr(reg, value) write_csr(reg, value)
#define a210_set_csr(reg, value) set_csr(reg, value)

#endif
