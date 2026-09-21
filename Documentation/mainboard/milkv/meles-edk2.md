# Meles EDK2 firmware profiles

The TH1520 coreboot port provides two EDK2 profiles. Both use native DDR
initialization and the existing relocatable OpenSBI dynamic firmware.
No U-Boot runtime is part of either image.

| Profile | Purpose | Medium | Image size |
| --- | --- | --- | --- |
| recovery | Automatically start serial USB Fastboot | BootROM RAM | 1 MiB |
| normal | Full EDK2 boot manager and device drivers | SPI | 2 MiB |

The RAM profile fits the 1152 KiB BootROM download window. Its CBFS is
copied to DDR after training. The normal image uses the same 256 KiB
bootblock area with a larger CBFS region and is not a RAM download image.

## Build

From the coreboot checkout, build the independent EDK2 platform project:

```sh
make -C ../th1520-edk2 edk2-recovery
make -C ../th1520-edk2 edk2 EDK2_BOARD=meles
```

Use the existing board-configured AON image and Meles DTB as inputs:

```sh
python3 util/thead/build-firmware.py --profile recovery \
  --edk2-fd ../th1520-edk2/build/meles-recovery/TH1520.fd \
  --opensbi ../build/opensbi/platform/generic/firmware/fw_dynamic.bin \
  --dtb ../build/edk2/meles.dtb --aon ../build/edk2/aon.bin

python3 util/thead/build-firmware.py --profile normal \
  --edk2-fd ../th1520-edk2/build/meles/TH1520.fd \
  --opensbi ../build/opensbi/platform/generic/firmware/fw_dynamic.bin \
  --dtb ../build/edk2/meles.dtb --aon ../build/edk2/aon.bin
```

Outputs are `build/recovery/coreboot.rom` and
`build/normal-spi/coreboot.rom`. The AON input must already contain the
board's configuration, including its versioned `AON_CONFIG` header and
legacy `UBOOT_SET` ABI marker. That marker is an AON configuration format,
not a dependency on a running U-Boot instance. The tested input is the
53192-byte image already used by the Meles firmware project.

`build-recovery.py` remains a compatibility entry point. Omitting
`--aon` is supported only for the minimal recovery/bring-up profile,
which then retains the earlier 750 MHz boot policy.

## AON and CPU operating point

AON-enabled boots first establish a 300 MHz CPU / 150 MHz bus clock.
The E902 is held in reset while the board-configured image is copied
from CBFS into its SRAM. Coreboot prepares the AON I2C pads, audio bus
and memory access before releasing the E902.

The PMIC mailbox service is probed with bounded retries. Coreboot asks
for 1.0 V, validates the completed response and reads both CPU rails
back. Only matching 1000000 uV values permit the switch to 1848 MHz CPU
and 616 MHz bus. A missing image, malformed response, timeout or wrong
voltage leaves the safe CPU clock and reports a failure.

AON also lets the existing OpenSBI platform select its AON restart and
shutdown implementation. This enables cold AON startup and voltage
control; DDR retention-state export and S3 resume are not added here.
Display PLL programming is unchanged.

## Loading and updating

Enter DOWNLOAD with BOOT + RESET and load the recovery image:

```sh
fastboot -S 0 flash ram build/recovery/coreboot.rom
fastboot reboot
```

Once `meles-recovery` appears, program the normal image:

```sh
fastboot -s meles-recovery -S 0 flash spi build/normal-spi/coreboot.rom
fastboot -s meles-recovery reboot
```

The recovery writer verifies the programmed bytes by reading them back.
It also exposes `emmcboot0` and `emmcboot1`, without a raw user-area
writing target. These commands do not select an eMMC boot partition.
The normal profile retains the complete EDK2 interface and its existing
Fastboot application. Use the RAM recovery `spi` target for these raw
coreboot images; the existing `loader` target validates SPL/FIT images.

## Validation

Host register tests cover PLL transitions, the AON mailbox completion
protocol, malformed replies and voltage-gated boost. Display setup tests
check the active AP register windows, VO local gates and the required
clock/reset-before-IOPMP order.
Meles recovery boots report 1000000/1000000 uV and 1848/616 MHz, including
the RAM path. Three AON-backed recovery restarts passed.

The normal profile uses the existing Meles DTB. Linux retains the
`/chosen/coreboot-table` marker and sees the AON and mailbox nodes.
With the mailbox module loaded, both drivers bind and all six AON power
domains register. Earlier mailbox-request messages occur during deferred
probe; check final binding instead of treating each message as a missing
AON firmware image.

The SPI boot path also needs eMMC IOPMP setup even though EDK2 uses PIO.
Without it, Linux ADMA fails before the root device appears. EDK2 now
sets the controller's default attribute after enabling its clock, matching
TH1520 U-Boot. Register-only correction and a controller reprobe restored
HS200 discovery on the original failing boot.

IOPMP register access depends on each subsystem's clock and reset state.
Coreboot initializes all 28 entries from mainline U-Boot's policy table
in ramstage, before AON and EDK2 start. Both recovery and normal profiles
enable the storage and VI/VO/DSP clocks, release VP/VO/VI/DSP resets,
and enable local DPU/GPU IOPMP APB/AXI gates before writing attributes.
The PLL frequencies are unchanged. Early eMMC access still prepares its
own policy before ramstage; EDK2 drivers may reapply their device policy.

The final normal RELEASE image was flashed to SPI with readback
verification. Linux 7.0.0-31-ruyi then discovered the HS200 eMMC at 2.75 s,
mounted its root filesystem at 5.70 s and reached the login prompt without
manual register changes or ADMA errors. This boot also had no AON mailbox
request errors. A Linux `reboot -f` from the preceding diagnostic build
returned to EDK2, where the serial F9 key entered USB Fastboot.

## I/O throughput

The SPI controller now batches FIFO transfers and derives its divider
from the inherited VIDEO_PLL. On Meles this selects 9.9 MHz without
retuning the PLL, within the existing 10 MHz board limit. Reading the
roughly 801 kB compressed payload takes 647 ms instead of 4867 ms;
OpenSBI's 106433-byte read takes 86 ms instead of 646 ms.

The EDK2 payload now negotiates HS52 eMMC within DTB/card limits and
uses cache-synchronized ADMA2 buffers. The Meles 49.5 MHz, eight-bit
path reads at about 41.5 MiB/s, up from 11.5 MiB/s. These measurements
refer to EDK2 Block I/O, not coreboot's separate early eMMC PIO driver.
USB and GMAC measurements and standalone EFI test instructions are in
`../th1520-edk2/docs/io-performance.md` relative to the coreboot checkout.
