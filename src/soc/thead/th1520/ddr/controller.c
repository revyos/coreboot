/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017-2020 Alibaba Group Holding Limited
 * Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn>
 *
 * Ported directly from th1520-vendor-uboot, revision a13e24ed9ed77,
 * board/thead/light-c910/lpddr4. LPDDR4X-3733, dual rank, 64-bit, DBI on.
 */
#include <console/console.h>
#include <delay.h>
#include <timer.h>
#include "ddr_internal.h"

#define MSTR               0x0000
#define STAT               0x0004
#define MRCTRL0            0x0010
#define MRCTRL1            0x0014
#define DERATEEN           0x0020
#define DERATEINT          0x0024
#define DERATECTL          0x002c
#define PWRCTL             0x0030
#define PWRTMG             0x0034
#define HWLPCTL            0x0038
#define RFSHCTL0           0x0050
#define RFSHCTL1           0x0054
#define RFSHCTL3           0x0060
#define RFSHTMG            0x0064
#define RFSHTMG1           0x0068
#define CRCPARCTL0         0x00c0
#define INIT0              0x00d0
#define INIT1              0x00d4
#define INIT2              0x00d8
#define INIT3              0x00dc
#define INIT4              0x00e0
#define INIT5              0x00e4
#define INIT6              0x00e8
#define INIT7              0x00ec
#define DIMMCTL            0x00f0
#define RANKCTL            0x00f4
#define RANKCTL1           0x00f8
#define DRAMTMG0           0x0100
#define DRAMTMG1           0x0104
#define DRAMTMG2           0x0108
#define DRAMTMG3           0x010c
#define DRAMTMG4           0x0110
#define DRAMTMG5           0x0114
#define DRAMTMG6           0x0118
#define DRAMTMG7           0x011c
#define DRAMTMG8           0x0120
#define DRAMTMG12          0x0130
#define DRAMTMG13          0x0134
#define DRAMTMG14          0x0138
#define ZQCTL0             0x0180
#define ZQCTL1             0x0184
#define ZQCTL2             0x0188
#define DFITMG0            0x0190
#define DFITMG1            0x0194
#define DFILPCFG0          0x0198
#define DFIUPD0            0x01a0
#define DFIUPD1            0x01a4
#define DFIUPD2            0x01a8
#define DFIMISC            0x01b0
#define DFITMG2            0x01b4
#define DFISTAT            0x01bc
#define DBICTL             0x01c0
#define DFIPHYMSTR         0x01c4
#define ADDRMAP0           0x0200
#define ADDRMAP1           0x0204
#define ADDRMAP2           0x0208
#define ADDRMAP3           0x020c
#define ADDRMAP4           0x0210
#define ADDRMAP5           0x0214
#define ADDRMAP6           0x0218
#define ADDRMAP7           0x021c
#define ADDRMAP9           0x0224
#define ADDRMAP10          0x0228
#define ADDRMAP11          0x022c
#define ODTCFG             0x0240
#define ODTMAP             0x0244
#define SCHED              0x0250
#define SCHED1             0x0254
#define PERFHPR1           0x025c
#define PERFLPR1           0x0264
#define PERFWR1            0x026c
#define SCHED3             0x0270
#define SCHED4             0x0274
#define DBG0               0x0300
#define DBG1               0x0304
#define DBGCMD             0x030c
#define SWCTL              0x0320
#define SWSTAT             0x0324
#define SWCTLSTATIC        0x0328
#define PSTAT              0x03fc
#define POISONCFG          0x036c
#define PCCFG              0x0400
#define PCFGR_0            0x0404
#define PCFGW_0            0x0408
#define PCTRL_0            0x0490
#define PCFGR_1            0x04b4
#define PCFGW_1            0x04b8
#define PCTRL_1            0x0540
#define PCFGR_2            0x0564
#define PCFGW_2            0x0568
#define PCTRL_2            0x05f0
#define PCFGR_3            0x0614
#define PCFGW_3            0x0618
#define PCTRL_3            0x06a0
#define PCFGR_4            0x06c4
#define PCFGW_4            0x06c8
#define PCTRL_4            0x0750
#define DCH1_STAT          0x1b04
#define DCH1_MRCTRL0       0x1b10
#define DCH1_MRCTRL1       0x1b14
#define DCH1_DERATECTL     0x1b2c
#define DCH1_PWRCTL        0x1b30
#define DCH1_HWLPCTL       0x1b38
#define DCH1_CRCPARCTL0    0x1bc0
#define DCH1_ZQCTL2        0x1c88
#define DCH1_DFISTAT       0x1cbc
#define DCH1_ODTMAP        0x1d44
#define DCH1_DBG1          0x1e04
#define DCH1_DBGCMD        0x1e0c

#define DDR_CFG0 0x00
#define DDR_CFG1 0x04
#define DDR_PLL_CFG0 0x08
#define DDR_PLL_CFG1 0x0c
#define DDR_PLL_STATUS 0x18

static void ctrl_write(uint32_t reg, uint32_t value)
{
	write32((void *)(TH1520_DDR_CTRL_BASE + reg), value);
}

static int ctrl_wait(uint32_t reg, uint32_t value)
{
	void *addr = (void *)(TH1520_DDR_CTRL_BASE + reg);

	if (wait_us(1000000, read32(addr) == value))
		return 0;
	printk(BIOS_ERR, "DDR controller timeout at %#x: %#x != %#x\n",
		reg, read32(addr), value);
	return -1;
}

int ddr_controller_prepare(uint64_t capacity)
{
	if (capacity != 8ULL * GiB && capacity != 16ULL * GiB)
		return -1;

	ddr_sys_write(DDR_PLL_CFG1, 0x4b000000);
	ddr_sys_write(DDR_PLL_CFG0, 0x01204d01);
	udelay(2);
	ddr_sys_write(DDR_PLL_CFG1, 0x0b000000);
	if (!wait_us(1000, ddr_sys_read(DDR_PLL_STATUS) & 1)) {
		printk(BIOS_ERR, "DDR PLL did not lock\n");
		return -1;
	}
	ddr_sys_write(DDR_PLL_STATUS, 0x10000);

	/* Preserve the vendor's APB reset write repetitions. */
	for (unsigned int i = 0; i < 6; i++)
		ddr_sys_write(DDR_CFG0, 0x40);
	for (unsigned int i = 0; i < 4; i++)
		ddr_sys_write(DDR_CFG0, 0xc0);
	for (unsigned int i = 0; i < 6; i++)
		ddr_sys_write(DDR_CFG0, 0xd0);
	ctrl_write(DBG1, 0x00000001);
	ctrl_write(PWRCTL, 0x00000001);
	if (ctrl_wait(STAT, 0x00000000))
		return -1;
	ctrl_write(MSTR, 0x03080020);
	ctrl_write(MRCTRL0, 0x00003030);
	ctrl_write(MRCTRL1, 0x0002d90f);
	ctrl_write(DERATEEN, 0x000013f3);
	ctrl_write(DERATEINT, 0x40000000);
	ctrl_write(DERATECTL, 0x00000001);
	ctrl_write(PWRCTL, 0x00000020);
	ctrl_write(PWRTMG, 0x0040ae04);
	ctrl_write(HWLPCTL, 0x00430000);
	ctrl_write(RFSHCTL0, 0x00210004);
	ctrl_write(RFSHCTL1, 0x000d0021);
	ctrl_write(RFSHCTL3, 0x00000001);
	ctrl_write(RFSHTMG, 0x81c00084);
	ctrl_write(RFSHTMG1, 0x00540000);
	ctrl_write(CRCPARCTL0, 0x00000000);
	ctrl_write(INIT0, 0xc0020002);
	ctrl_write(INIT1, 0x00010002);
	ctrl_write(INIT2, 0x00001f00);
	ctrl_write(INIT3, 0x00640036);
	ctrl_write(INIT4, 0x00f20008);
	ctrl_write(INIT5, 0x0004000b);
	ctrl_write(INIT6, 0x00440012);
	ctrl_write(INIT7, 0x0004001a);
	ctrl_write(DIMMCTL, 0x00000000);
	ctrl_write(RANKCTL, 0x0000ab9f);
	ctrl_write(RANKCTL1, 0x00000017);
	ctrl_write(DRAMTMG0, 0x1f263f28);
	ctrl_write(DRAMTMG1, 0x00080839);
	ctrl_write(DRAMTMG2, 0x08121d17);
	ctrl_write(DRAMTMG3, 0x00d0e000);
	ctrl_write(DRAMTMG4, 0x11040a12);
	ctrl_write(DRAMTMG5, 0x02050e0e);
	ctrl_write(DRAMTMG6, 0x01010008);
	ctrl_write(DRAMTMG7, 0x00000502);
	ctrl_write(DRAMTMG8, 0x00000101);
	ctrl_write(DRAMTMG12, 0x00020000);
	ctrl_write(DRAMTMG13, 0x0d100002);
	ctrl_write(DRAMTMG14, 0x0000010c);
	ctrl_write(ZQCTL0, 0x03a50021);
	ctrl_write(ZQCTL1, 0x02f00800);
	ctrl_write(ZQCTL2, 0x00000000);
	ctrl_write(DFITMG0, 0x059f820c);
	ctrl_write(DFITMG1, 0x000c0303);
	ctrl_write(DFILPCFG0, 0x0351a101);
	ctrl_write(DFIMISC, 0x00000011);
	ctrl_write(DFITMG2, 0x00001f0c);
	ctrl_write(DBICTL, 0x00000007);
	ctrl_write(DFIPHYMSTR, 0x14000001);
	ctrl_write(ADDRMAP0, 0x0002001f);
	ctrl_write(ADDRMAP1, 0x00090909);
	ctrl_write(ADDRMAP2, 0x01010000);
	ctrl_write(ADDRMAP3, 0x01010101);
	ctrl_write(ADDRMAP4, 0x00001f1f);
	ctrl_write(ADDRMAP5, 0x080f0808);
	ctrl_write(ADDRMAP6, 0x08080808);
	ctrl_write(ADDRMAP7, 0x00000f0f);
	ctrl_write(ADDRMAP9, 0x08080808);
	ctrl_write(ADDRMAP10, 0x08080808);
	ctrl_write(ADDRMAP11, 0x00000008);
	ctrl_write(ODTCFG, 0x06090b40);
	ctrl_write(DFIUPD0, 0x00400018);
	ctrl_write(DFIUPD1, 0x00280032);
	ctrl_write(DFIUPD2, 0x00000000);
	ctrl_write(ODTMAP, 0x00000000);
	ctrl_write(SCHED, 0x1f829b1c);
	ctrl_write(SCHED1, 0x4400b00f);
	ctrl_write(PERFHPR1, 0x0f000001);
	ctrl_write(PERFLPR1, 0x0f00007f);
	ctrl_write(PERFWR1, 0x0f00007f);
	ctrl_write(SCHED3, 0x00000208);
	ctrl_write(SCHED4, 0x08400810);
	ctrl_write(DBG0, 0x00000000);
	ctrl_write(DBG1, 0x00000000);
	ctrl_write(DBGCMD, 0x00000000);
	ctrl_write(SWCTL, 0x00000001);
	ctrl_write(SWCTLSTATIC, 0x00000000);
	ctrl_write(POISONCFG, 0x00000001);
	ctrl_write(PCTRL_0, 0x00000001);
	ctrl_write(PCTRL_1, 0x00000001);
	ctrl_write(PCTRL_2, 0x00000001);
	ctrl_write(PCTRL_3, 0x00000001);
	ctrl_write(PCTRL_4, 0x00000001);
	ctrl_write(DCH1_MRCTRL0, 0x00003030);
	ctrl_write(DCH1_MRCTRL1, 0x0002d90f);
	ctrl_write(DCH1_DERATECTL, 0x00000001);
	ctrl_write(DCH1_PWRCTL, 0x00000020);
	ctrl_write(DCH1_HWLPCTL, 0x00430002);
	ctrl_write(DCH1_CRCPARCTL0, 0x00000000);
	ctrl_write(DCH1_ZQCTL2, 0x00000000);
	ctrl_write(DCH1_ODTMAP, 0x00000000);
	ctrl_write(DCH1_DBG1, 0x00000000);
	ctrl_write(DCH1_DBGCMD, 0x00000000);
	if (ctrl_wait(RFSHCTL3, 0x00000001))
		return -1;
	ctrl_write(PCCFG, 0x00000010);
	ctrl_write(PCFGR_0, 0x0000500f);
	ctrl_write(PCFGW_0, 0x0000500f);
	ctrl_write(PCFGR_1, 0x00005020);
	ctrl_write(PCFGW_1, 0x0000501f);
	ctrl_write(PCFGR_2, 0x0000501f);
	ctrl_write(PCFGW_2, 0x0000503f);
	ctrl_write(PCFGR_3, 0x000051ff);
	ctrl_write(PCFGW_3, 0x000051ff);
	ctrl_write(PCFGR_4, 0x0000503f);
	ctrl_write(PCFGW_4, 0x0000503f);
	if (ctrl_wait(PWRCTL, 0x00000020))
		return -1;
	ctrl_write(PWRCTL, 0x00000020);
	if (ctrl_wait(DCH1_PWRCTL, 0x00000020))
		return -1;
	ctrl_write(DCH1_PWRCTL, 0x00000020);
	ctrl_write(DBG1, 0x00000000);
	if (ctrl_wait(PWRCTL, 0x00000020))
		return -1;
	ctrl_write(PWRCTL, 0x00000020);
	if (ctrl_wait(PWRCTL, 0x00000020))
		return -1;
	ctrl_write(PWRCTL, 0x00000020);
	ctrl_write(DCH1_DBG1, 0x00000000);
	if (ctrl_wait(DCH1_PWRCTL, 0x00000020))
		return -1;
	ctrl_write(DCH1_PWRCTL, 0x00000020);
	if (ctrl_wait(DCH1_PWRCTL, 0x00000020))
		return -1;
	ctrl_write(DCH1_PWRCTL, 0x00000020);
	ctrl_write(DFIPHYMSTR, 0x14000001);
	ctrl_write(SWCTL, 0x00000000);
	ctrl_write(DFIMISC, 0x00000010);
	ctrl_write(DFIMISC, 0x00000010);
	ctrl_write(DBG1, 0x00000002);
	ctrl_write(DCH1_DBG1, 0x00000002);

	ctrl_write(ADDRMAP0, 0x0004001f);
	ctrl_write(ADDRMAP0, capacity == 16ULL * GiB ? 0x00040019 : 0x00040018);
	ctrl_write(ADDRMAP1, 0x00090909);
	ctrl_write(ADDRMAP2, 0x00000000);
	ctrl_write(ADDRMAP3, 0x01010101);
	ctrl_write(ADDRMAP4, 0x00001f1f);
	ctrl_write(ADDRMAP5, 0x080f0808);
	ctrl_write(ADDRMAP6, 0x08080808);
	ctrl_write(ADDRMAP7, capacity == 16ULL * GiB ? 0x00000f08 : 0x00000f0f);
	ctrl_write(ADDRMAP9, 0x08080808);
	ctrl_write(ADDRMAP10, 0x08080808);
	ctrl_write(ADDRMAP11, 0x00000008);
	ddr_sys_write(DDR_CFG0, ddr_sys_read(DDR_CFG0) | 0x1fa0);
	return 0;
}

int ddr_controller_enable(void)
{
	ctrl_write(DFIMISC, 0x00000030);
	if (ctrl_wait(DFISTAT, 0x00000001))
		return -1;
	if (ctrl_wait(DCH1_DFISTAT, 0x00000001))
		return -1;
	ctrl_write(DFIMISC, 0x00000010);
	ctrl_write(DFIMISC, 0x00000011);
	ctrl_write(PWRCTL, 0x0000000a);
	ctrl_write(DCH1_PWRCTL, 0x0000000a);
	ctrl_write(SWCTL, 0x00000001);
	if (ctrl_wait(SWSTAT, 0x00000001))
		return -1;
	if (ctrl_wait(STAT, 0x00000001))
		return -1;
	if (ctrl_wait(DCH1_STAT, 0x00000001))
		return -1;
	ctrl_write(DFIPHYMSTR, 0x14000001);
	ctrl_write(SWCTL, 0x00000000);
	ctrl_write(INIT0, 0x00020002);
	ctrl_write(SWCTL, 0x00000001);
	if (ctrl_wait(SWSTAT, 0x00000001))
		return -1;
	/* Enable all five AXI ports as in enable_axi_port(0x1f). */
	ddr_sys_write(DDR_CFG0, 0x00f0);
	ddr_sys_write(DDR_CFG0, 0x1ff0);
	ctrl_write(DBG1, 0);
	ctrl_write(DCH1_DBG1, 0);
	ctrl_write(PCTRL_0, 1);
	ctrl_write(PCTRL_1, 1);
	ctrl_write(PCTRL_2, 1);
	ctrl_write(PCTRL_3, 1);
	ctrl_write(PCTRL_4, 1);
	ctrl_write(RFSHCTL3, 0);
	ddr_sys_write(DDR_CFG1, 0xa0000);
	ctrl_write(SWCTL, 0);
	ctrl_write(SWCTLSTATIC, 1);
	ctrl_write(PWRTMG, 0x40ae04);
	ctrl_write(HWLPCTL, 0x430003);
	ctrl_write(DCH1_HWLPCTL, 0x430003);
	ctrl_write(SWCTL, 1);
	ctrl_write(SWCTLSTATIC, 0);
	ctrl_write(PWRCTL, 0x0000000b);
	ctrl_write(DCH1_PWRCTL, 0x0000000b);
	return 0;
}

int ddr_controller_set_capacity(uint64_t capacity)
{
	if (capacity != 8ULL * GiB)
		return -1;

	/* No DRAM consumers exist yet. Quiesce all AXI ports before remapping. */
	ddr_sys_write(DDR_CFG1, 0xa000011f);
	ctrl_write(PCTRL_0, 0);
	ctrl_write(PCTRL_1, 0);
	ctrl_write(PCTRL_2, 0);
	ctrl_write(PCTRL_3, 0);
	ctrl_write(PCTRL_4, 0);
	if (ctrl_wait(PSTAT, 0))
		return -1;
	printk(BIOS_INFO, "TH1520: DDR AXI ports idle\n");
	ctrl_write(DBG1, 2);
	ctrl_write(DCH1_DBG1, 2);

	/* Change only the address map; preserve the trained PHY state. */
	ctrl_write(SWCTL, 0);
	ctrl_write(ADDRMAP0, 0x00040018);
	ctrl_write(ADDRMAP7, 0x00000f0f);
	ctrl_write(SWCTL, 1);
	if (ctrl_wait(SWSTAT, 1))
		return -1;
	printk(BIOS_INFO, "TH1520: DDR address map updated\n");

	ctrl_write(DBG1, 0);
	ctrl_write(DCH1_DBG1, 0);
	ctrl_write(PCTRL_0, 1);
	ctrl_write(PCTRL_1, 1);
	ctrl_write(PCTRL_2, 1);
	ctrl_write(PCTRL_3, 1);
	ctrl_write(PCTRL_4, 1);
	ddr_sys_write(DDR_CFG1, 0xa0000);
	return 0;
}
