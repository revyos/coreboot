# SPDX-License-Identifier: GPL-2.0-only
ifeq ($(CONFIG_SOC_ZHIHE_A210),y)
CPPFLAGS_common += -Isrc/soc/zhihe/a210/include
bootblock-y += early.S
bootblock-y += adc.c
all-y += boot_device.c
all-y += i2c.c
bootblock-y += ocram.c
bootblock-y += power.c
all-$(CONFIG_A210_EMMC) += emmc.c
bootblock-$(CONFIG_A210_EMMC_TEST) += storage_test.c
bootblock-$(CONFIG_A210_BOOTSTRAP) += bootstrap.c transfer.S rom_return.S
bootblock-$(CONFIG_A210_FIRMWARE) += cbmem.c
ramstage-y += cbmem.c chip.c cpu.c cache.c cpu_clock.c aon.c iopmp.c
ramstage-y += display.c
bootblock-$(CONFIG_A210_DDR_TEST) += ddr/pmu_data.c
bootblock-$(CONFIG_A210_DDR_TEST) += ddr/phy.c
bootblock-$(CONFIG_A210_DDR_TEST) += ddr/timing.c
bootblock-$(CONFIG_A210_DDR_TEST) += ddr/training.c
bootblock-$(CONFIG_A210_DDR_TEST) += ddr/raminit.c
bootblock-$(CONFIG_A210_DDR_TEST) += ddr/memory_test.c
all-y += uart.c
all-y += timer.c
all-y += clock.c
ifeq ($(CONFIG_A210_FIRMWARE),y)
ifneq ($(CONFIG_A210_BOOT_RAM),y)
cbfs-files-y += fallback/ddr
fallback/ddr-file := $(obj)/cbfs/fallback/bootblock.raw.bin
fallback/ddr-type := raw
fallback/ddr-compression := none
endif
cbfs-files-y += fallback/aon
fallback/aon-file := $(call strip_quotes,$(CONFIG_A210_AON_FILE))
fallback/aon-type := raw
fallback/aon-compression := lzma
cbfs-files-y += fallback/DTB
fallback/DTB-file := $(call strip_quotes,$(CONFIG_A210_PAYLOAD_DTB))
fallback/DTB-type := raw
endif
endif
