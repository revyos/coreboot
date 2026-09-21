# SPDX-License-Identifier: GPL-2.0-only

ifeq ($(CONFIG_SOC_THEAD_TH1520),y)
CPPFLAGS_common += -Isrc/soc/thead/th1520/include
bootblock-y += early.S
bootblock-y += bootblock.c
bootblock-y += cpu_clock.c
bootblock-y += ddr/init.c
bootblock-y += ddr/controller.c
bootblock-y += ddr/phy.c
bootblock-y += ddr/mailbox.c
bootblock-y += ddr/pmu_data.c
bootblock-y += romstage.c
bootblock-y += cbmem.c
all-y += uart.c
all-y += timer.c
all-y += cache.c
all-y += boot_device.c
all-y += spi.c
all-y += emmc.c
bootblock-$(CONFIG_TH1520_STORAGE_TEST) += storage_test.c
ramstage-y += chip.c
ramstage-y += cbmem.c
endif
