# Melon Pi

Melon Pi uses the A210 SODIMM module with two
CXDB5CBAM LPDDR4X packages and 8 GiB of memory. Coreboot runs on C908
hart 0. OpenSBI manages the other three C908 and four C920v2 harts.
Other DRAM profiles and multi-die A210 variants are not supported yet.

Native PMIC, OCRAM, DDR and eMMC initialization are implemented.
The firmware boots ramstage and OpenSBI through standard CBFS and
FW_DYNAMIC interfaces. RAM download and eMMC boot0 images share the
same initialization code. No bootzero or U-Boot executable is included.

## Carrier revisions

V1 and V2 share the A210 SODIMM module and boot architecture. V3 is
a minor V2 hardware revision and uses the V2 firmware and DTB. Select
`--board melonpi-v1` or `melonpi-v2` explicitly; V2 remains
the default. ADC straps select the module memory profile and cannot
identify the carrier revision. V1 has passed native firmware, NVMe and
USB boot tests. The V2 results below were obtained on its minor revision
previously called V3; the older V2 population was not separately tested.

V1 enables its fan through AW9535 U31 P02 on AON I2C1. V2 uses
GPIO0_30. On V1 that SoC pin belongs to PCIe, so using the V2 fan path
would affect endpoint reset instead of cooling. AON I2C1 setup preserves
the PMIC bus and reads the existing AON clock configuration.

The DTB compatible must match the requested carrier before packaging
starts. Both bootstrap and main firmware receive the same selection,
and the resulting filenames include the carrier revision.

## Build

Provide a generic A210-capable OpenSBI `fw_dynamic.elf`, a RISC-V ELF
payload and the board DTB. OpenSBI must fit the reserved 1 MiB region
at `0x80000000`. The packager accepts the default position-independent
`fw_dynamic.elf` and gives its load segments physical addresses for
coreboot's SELF loader. OpenSBI performs its own relocation; coreboot
passes the payload entry, privilege mode and DTB at runtime. An ELF
already linked in the reserved region is also accepted.
The DTB must describe the eight harts,
UART4, PLIC and the T-Head CLINT. Firmware patches the measured memory
size and UART input clock before passing it to OpenSBI.

An S-mode test payload exercises SBI calls and starts, stops and
restarts harts 1 through 7:

```sh
python3 util/zhihe/build-sbi-test.py --output build/sbi-test.elf
```

Build the RAM firmware from the coreboot directory:

```sh
python3 util/zhihe/build-firmware.py \
  --board melonpi-v2 \
  --boot-medium ram \
  --scmi /path/to/zephyr.bin \
  --opensbi /path/to/fw_dynamic.elf \
  --payload build/sbi-test.elf \
  --dtb /path/to/a210-melonpi-v2.dtb \
  --output build/melonpi-v2-ram
```

Use `--boot-medium emmc` and a separate output directory to build
`melonpi-v2-coreboot-boot0.bin`. The RAM profile uses a 1.5 MiB CBFS image;
the eMMC profile uses 2 MiB. An image that does not fit fails the build.

## SCMI ownership

Pass the A210 E902 Zephyr SCMI image with `--scmi`. Ramstage stores it in
CBFS as `fallback/scmi`, loads it at 0x308f8000 and releases E902. The
image must fit in 63 KiB; the SCMI request and notification channels
occupy 256 bytes each at 0x30907c00 and 0x30907d00. Startup waits for a successful SCMI Base version
query, not a vendor AON ready marker.

Before releasing E902, coreboot prepares the secondary CPU supplies and
power domains, raises CPU/SRAM supplies to 1 V, and selects the validated
1.896 GHz CPU operating point. It verifies the PMIC I2C bus is idle,
masks its interrupts and disables the controller before Zephyr adopts it. Subsequent CPU
performance and PMIC changes belong to SCMI; coreboot does not issue the
old AON voltage RPC or rewrite CPU PLLs after the handoff.

An initialized eMMC controller stops its card clock while Zephyr sets up
its parent clock. Once SCMI responds, coreboot queries the new reference
rate and reapplies the SDHCI divider before reading more CBFS data.
EDK2 and Linux must attach to the running service without overwriting
SRAM or resetting E902. The Linux DT and firmware resource IDs must match.

A RAM boot completed Zephyr initialization, answered SCMI Base 2.0 and
reached OpenSBI. Complete OS handoff, runtime and reset validation remain
pending. The earlier AON-based checks below apply to the previous boot
chain.

## RAM startup

Enter BootROM Download, then run each command after the preceding
command succeeds:

```sh
fastboot -S 0 flash ram build/melonpi-v2-ram/melonpi-v2-bootstrap-rvbl.bin
fastboot reboot
```

Wait for `OCRAM ready for BootROM RAM download`, then send the second
image:

```sh
fastboot -S 0 flash ram \
  build/melonpi-v2-ram/melonpi-v2-coreboot-ram-rvbl.bin
fastboot reboot
```

The first native stage initializes PMIC and 2 MiB OCRAM. It preserves
the BootROM caller's registers, selects the ROM's OCRAM download
buffer and returns to its USB service. The second image contains a
small copy entry and the complete CBFS image. Its DDR stage executes
from BRAM while preserving the downloaded image in OCRAM. Once DDR
is usable, coreboot copies and verifies the image at `0x81000000`,
then leaves SLC SRAM mode and loads ramstage from the RAM copy.

This flow does not write eMMC. The supplied smoke-test payload ends
in a wait loop; it is not a Linux or EDK2 boot image.

## eMMC boot0 layout

| Offset | Contents |
| --- | --- |
| `0x000000` | RVBL header and native small bootstrap |
| `0x008000` | Complete 2 MiB coreboot image |

The bootstrap reads `fallback/ddr` from CBFS in boot0 into OCRAM.
A stackless OCRAM entry copies that stage into BRAM before executing
it. After DDR initialization, ramstage, OpenSBI, the DTB and the
payload are read from the same boot0 CBFS. The SDHCI/MMC driver also
supports bounded reads of the user and boot1 partitions.

The generated image is 2080 KiB. The last 64 KiB of the tested 4 MiB
boot0 area is reserved for the EDK2 MAC record. The packager rejects
images extending past offset `0x3f0000`.
Ramstage reserves a 1 MiB CBFS read buffer for the compressed EDK2
payload. Flashing through EDK2 Fastboot and a complete boot from
eMMC boot0 after a global reset have passed on hardware.

## EDK2 payload

The platform repository's `MelonPiPkg/MelonPi.dsc` builds `A210.fd`.
Pass that volume to `build-firmware.py --edk2-fd` instead of `--payload`.
The packager wraps it in a RISC-V ELF loaded at `0x88000000`. Its full
4 MiB extent is retained for SEC firmware-volume parsing and compressed
in CBFS. The RAM profile omits the redundant `fallback/ddr` CBFS file;
its DDR stage already occupies the BOOTBLOCK region.

The EDK2 RAM image is 1540 KiB, within the 2 MiB OCRAM. BootROM
Fastboot accepts the initial RAM download. Once EDK2 starts, F9 enters
its native USB Fastboot service. Flash the complete eMMC boot0 image
to `emmc0boot0`; `emmc0boot1` names the second hardware boot area.

The bootstrap enables the Melon Pi fan before waiting for the second
USB download. AON GPIO0_29 enables the 5 V rail, and GPIO0_30 switches
the fan supply. This is an on/off control, not PWM speed regulation.

## CPU and memory initialization

BRAM spans `0x00100000` through `0x0011ffff`. The small bootstrap is
linked at `0x00100a00`; its RVBL entry saves the ROM context first.
The DDR stage starts at `0x00100800`. Its code, tables and 4 KiB stack
remain in BRAM while DDR clocks and the SLC path change. The stack
starts at `0x0011d000`.

D-cache remains disabled through memory training. The ramstage hook
initializes the boot hart's cache and coherency state, following
zero_stage_boot ordering with the A210 vendor's L2 timing values.
OpenSBI's A210 warm entry performs the corresponding setup for each
secondary hart before using a C stack.

Coreboot prepares the C920 supply and the secondary PPU/BPC/PCU power
domains while keeping secondary resets asserted. OpenSBI HSM installs
each reset vector and releases that hart on its first start. Later
stop/start cycles use parking and IPIs. Before E902 startup, coreboot
raises the C908 SRAM, C908 CPU and C920 CPU rails to 1 V and verifies
each voltage. It selects the independent TOP clock while reprogramming
each CPU PLL to 1.896 GHz. Lock, divider and mux checks precede an
independent hardware frequency measurement. The earlier boot chain
measured 1,896,000 kHz on both clusters; the SCMI handoff still requires
hardware validation. A working cooling fan or heatsink is required.

SY70209 BUCK6/7 encoding is 600 mV plus 12.5 mV per step. SY70300
BUCK2 supplies C920 CPU_P at 450 mV plus 5 mV per step. The board code
raises rails below the 800 mV boot voltage, preserves higher valid
settings up to 1 V and verifies enable/voltage registers. Definitions
come from the A210 AON SDK's PMIC drivers and rail mapping. These early
I2C accesses finish before E902 starts; subsequent voltage changes use
SCMI so both processors do not drive the PMIC bus.

DDR settings and training data come from A210 vendor U-Boot revision
`7f63f1980d9c`, under `board/zhihe/a210-evb/ddr/`. GPL notices are
retained. Trailing zero words are written by the loader rather than
stored in BRAM. All 42,456 PHY events and 91 controller writes match
the original source, including order. Polling has deadlines and
mailbox streams are drained without a fixed-size buffer.

## Ethernet DMA access

Before entering the payload, coreboot installs the vendor PERI1 IOPMP
TOR entries for AON, the PERI1 master, GMAC0/1/2, chip debug and the
IOMMU page walker. Preserve debug access before replacing BootROM's
second entry. Every entry uses the vendor device ID and a read-back
check. Without these permissions the MAC can reach its PHY but DMA
transactions fail with a bus error.

## Source layout

`src/soc/zhihe/a210/` owns controller access, clock decoding, coherent
interconnect, power domains, memory initialization and boot-device
handling. Register bases, offsets and fields are named in
`include/soc/`. These drivers have no Melon Pi callbacks or names.

`src/mainboard/zhihe/melonpi/` owns the boot sequence, resistor-code
interpretation, PMIC wiring and voltage policy, memory profile,
DQ permutation and payload DTB preparation.

## Hardware checks

The following checks passed on the 8 GiB board with the earlier
vendor-AON boot chain:

- Native BootROM USB startup and the second OCRAM RAM download.
- Complete 2 MiB OCRAM address and inverse-address patterns.
- Both DDR PHYs' 1D/2D training and both controllers' normal mode.
- MR5=`0x13`, MR8=`0x18` on both controllers, yielding 8192 MiB.
- Address-alias checks across that range and two patterns over 1 MiB.
- User, boot0 and boot1 reads, including unaligned sector boundaries.
- A 1 MiB boot0 read in approximately 153 ms using 8-bit SDR PIO.
- Coreboot ramstage, OpenSBI v1.9 and S-mode SBI calls.
- Two complete HSM start/stop rounds for all seven secondary harts
  at the original BootROM clocks.
- AON startup and set/get voltage RPCs for all three CPU/SRAM rails.
- Both CPU clusters measured at 1.896 GHz after an eMMC boot0 reset.
- The eMMC boot0 chain reaches EDK2 and responds to Fastboot queries.

These are bring-up checks, not a full-memory or storage stress test.
HSM validation does not yet cover OS suspend, power-off or restart.

With JTAG attached, debugger interception of `ebreak` can stop
OpenSBI's semihosting probe before its banner. Disable that debugger
setting, or build OpenSBI without `CONFIG_SERIAL_SEMIHOSTING`.

## References

- [A210 hardware documentation][a210]
- [XuanTie zero_stage_boot][zsb]
- A210 AON SDK: `p100-img-aon`, PMIC drivers and `aon_pmic.c`.

[a210]: https://developer.zhcomputing.com/zh/docs/A210/
[zsb]: https://github.com/XUANTIE-RV/zero_stage_boot

## HDMI preparation

The V2 carrier's peripheral power rails require both AON GPIO29 (5 V)
and AON GPIO26 (3.3 V). The 3.3 V regulator's power-good signal enables
the carrier 1.8 V rail used by the HDMI DDC level shifters. Enable these
rails before starting the fan or payload; enabling only 5 V leaves the
HDMI control path unavailable.

Ramstage powers the VO domain through PCU/BPC and installs the vendor
VO IOPMP device entries for display DMA. EDK2 owns the DC8200, HDMI
controller and DDC pins. It requests output rates through SCMI; Zephyr
owns the divider and parent selection. The GPU domain is independent of this scanout path.

A full boot0 restart reached F2 setup on serial and HDMI simultaneously.
The capture sink selected 1920x1080 at 148.5 MHz through EDID. Linux
native display takeover and the updated RAM download still need tests.

## Device tree

Use the target kernel's matching `zhihe/a210-melonpi-vN.dtb` for
`--dtb`, where N is the carrier revision. Its source
is split into `a210.dtsi` for the SoC, `a210-som.dtsi` for the SODIMM
module and two carrier DTS files. The module owns eMMC,
Ethernet PHYs, memory and supply policy. The carrier owns the console,
HDMI connector and peripheral power enables.

Coreboot preserves the DT board identity and patches the memory size
from DDR training and the console clock from hardware. EDK2 discovers
controllers from this tree and passes it to the operating system.

## PCIe DMA access

Ramstage permits the PCIe fabric and both root-port DMA masters in the
shared PCIe IOPMP. Existing BootROM eMMC, SD and TEE entries are retained.
The initializer reuses a matching slot or an unused slot and verifies
the programmed entry. It fails if no slot is available.

Linux owns PCIe power sequencing, the E16 PHY, endpoint reset and
enumeration. Use the matching `a210-melonpi-v2.dtb` when packaging either
boot0 or RAM firmware; EDK2 passes this tree to the EFI kernel.
