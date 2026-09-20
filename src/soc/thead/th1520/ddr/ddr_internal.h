/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TH1520_DDR_INTERNAL_H
#define TH1520_DDR_INTERNAL_H

#include <commonlib/helpers.h>
#include <device/mmio.h>
#include <soc/addressmap.h>
#include <types.h>

#define PHY_IMEM 0x50000
#define PHY_DMEM 0x54000
#define PHY_MICRO_MUX 0xd0000
#define PHY_MICRO_RESET 0xd0099
#define PHY_MAIL_STATUS 0xd0004
#define PHY_MAIL_ACK 0xd0031
#define PHY_MAIL_LOW 0xd0032
#define PHY_MAIL_HIGH 0xd0034

struct ddr_pmu_image {
	const uint16_t *imem;
	size_t imem_words;
	const uint16_t *dmem;
	size_t dmem_words;
};

extern const struct ddr_pmu_image ddr_pmu_1d;
extern const struct ddr_pmu_image ddr_pmu_2d;

static inline void ddr_phy_write(unsigned int phy, uint32_t word, uint16_t value)
{
	uintptr_t base = phy ? TH1520_DDR_PHY1_BASE : TH1520_DDR_PHY0_BASE;

	write16((void *)(base + 2UL * word), value);
}

static inline uint16_t ddr_phy_read(unsigned int phy, uint32_t word)
{
	uintptr_t base = phy ? TH1520_DDR_PHY1_BASE : TH1520_DDR_PHY0_BASE;

	return read16((void *)(base + 2UL * word));
}

static inline void ddr_sys_write(uint32_t reg, uint32_t value)
{
	write32((void *)(TH1520_DDR_SYS_BASE + reg), value);
}

static inline uint32_t ddr_sys_read(uint32_t reg)
{
	return read32((void *)(TH1520_DDR_SYS_BASE + reg));
}

int ddr_controller_prepare(uint64_t capacity);
int ddr_controller_enable(void);
int ddr_controller_set_capacity(uint64_t capacity);
int ddr_phy_train(void);
int ddr_pmu_wait(unsigned int phy, unsigned int dimension);

#endif
