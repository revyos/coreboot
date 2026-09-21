# Lichee Pi 4A RAM recovery

The Lichee Pi 4A uses a separate 1 MiB BootROM RAM image. It combines
the TH1520 DDR initialization, OpenSBI, the Lichee Pi 4A device tree and
an EDK2 recovery volume configured with the LPi4A USB switch. Fastboot
reports `lpi4a-recovery` as its product.

Build the board's EDK2 volume first, then package it from coreboot:

```sh
make -C ../th1520-edk2 edk2-lpi4a-recovery
python3 util/thead/build-firmware.py --board lpi4a \
  --profile recovery \
  --edk2-fd ../th1520-edk2/build/lpi4a-recovery/TH1520.fd \
  --opensbi ../build/opensbi/platform/generic/firmware/fw_dynamic.bin \
  --dtb ../build/edk2-lpi4a/lpi4a.dtb \
  --aon ../build/edk2/aon.bin
```

The output is `build/lpi4a-recovery/coreboot.rom`. This recovery
profile starts AON, verifies both CPU rails at 1.0 V and then selects
the 1.848 GHz operating point. Loading
the image into BootROM RAM does not write eMMC. Once EDK2 Fastboot is
running, query the product and target sizes before writing boot0 or
boot1. Writing boot1 does not select it for BootROM startup.

The 8 GiB dual-rank LPi4A completed BootROM RAM download, one PHY
training pass, hardware address alias detection, controller remapping,
bank samples, AON voltage verification, OpenSBI and EDK2 Fastboot.
Fastboot reports `lpi4a-recovery`; the normal firmware uses
`lpi4a-edk2` when F9 opens Fastboot.

## Normal eMMC firmware

The normal image also uses coreboot, OpenSBI and EDK2. Build it with:

```sh
make -C ../th1520-edk2 edk2 EDK2_BOARD=lpi4a
python3 util/thead/build-firmware.py --board lpi4a \
  --profile normal \
  --edk2-fd ../th1520-edk2/build/lpi4a/TH1520.fd \
  --opensbi ../build/opensbi/platform/generic/firmware/fw_dynamic.bin \
  --dtb ../build/edk2-lpi4a/lpi4a.dtb \
  --aon ../build/edk2/aon.bin
```

The output is `build/lpi4a-normal-emmc/coreboot.rom`, a 2 MiB image
for eMMC boot0. It leaves the final 8 KiB available for MAC records.
From LPi4A RAM Recovery, install it with:

```sh
fastboot -s lpi4a-recovery -S 0 flash emmcboot0 \
  build/lpi4a-normal-emmc/coreboot.rom
fastboot -s lpi4a-recovery reboot
```

The writer checks the readback and preserves eMMC boot selection.
Both firmware profiles use native coreboot DDR initialization and
pass the measured memory capacity to OpenSBI and EDK2 in the DTB.
