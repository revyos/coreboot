# Milk-V Meles

The initial TH1520 port boots a RAM-resident coreboot image through the
SoC BootROM DOWNLOAD interface. It initializes dual-rank LPDDR4X from
native C code and enters ramstage with four C910 harts.

The basic configurations are bring-up images without a payload. They
can load CBFS from SRAM, SPI NOR or eMMC boot0. The separate
[EDK2 profiles](meles-edk2.md) support AON-backed 1.848 GHz startup. The final
`Payload not loaded` message is expected for these configurations.

## Build

The tested upstream base is `8ce4c91c41da`. Initialize the vboot
submodule required by the coreboot build tools, then select Meles:

```sh
git submodule update --init 3rdparty/vboot
cp configs/config.milkv_meles .config
export CROSS_COMPILE_riscv=riscv64-linux-gnu-
make UPDATED_SUBMODULES=1 olddefconfig
make UPDATED_SUBMODULES=1 -j8
```

`UPDATED_SUBMODULES=1` avoids fetching unrelated platform dependencies
for this initial configuration. The supplied config enables
`ANY_TOOLCHAIN` for the host's RISC-V Linux GCC toolchain. RV64IMAFDC and
the LP64D ABI match that toolchain; the C910 supports those extensions.

The result is `build/coreboot.rom`, exactly 1 MiB. Bootblock occupies the
first 256 KiB, followed by FMAP and CBFS. Stacks, the pre-RAM console and
CBFS metadata live above the image, within the SRAM download window.
The binary already includes the PHY parameters and training data;
normal builds do not need a vendor checkout or a separate DDR script image.

## Run from SRAM

Connect the USB-C port and UART0 at 115200 8N1. Hold BOOT while pressing
RESET to enter BootROM DOWNLOAD, then run on the host:

```sh
fastboot -S 0 flash ram build/coreboot.rom
fastboot reboot
```

These commands download and execute the image in SRAM. They do not write
SPI NOR or eMMC. The ROM's download limit is `0x120000` (1152 KiB).
Execution starts at `0xffe0000000`; DDR is initially unavailable.

A successful 16 GiB dual-rank test reports:

```text
TH1520: LPDDR4X-3733 dual-rank, 16 GiB training map
DDR PHY0 1D training complete
DDR PHY1 1D training complete
DDR PHY0 2D training complete
DDR PHY1 2D training complete
TH1520: 16 GiB DDR detected
TH1520: DDR bank sample test passed
TH1520: loading ramstage from CBFS
waiting for all harts to wake up...
all harts up and running...
Milk-V Meles: coreboot ramstage running
```

Each PHY completes both 1D and 2D training. Address alias checks cover
the configured capacity, followed by two complementary address-dependent
patterns in a 4 KiB sample of each GiB bank. These are bring-up checks,
not exhaustive memory qualification. The 8 GiB dual-rank address-map
fallback is implemented but has not been verified on this Meles.
Single-rank configurations are not supported by this initial port.

Ramstage executes at 16 MiB. CBMEM is below 2 GiB, while the coreboot
memory map describes the full detected memory capacity.

## Timer and early CPU setup

TH1520 binds the timer driver to `thead,c900-clint`, matching the
compatible used by its OS device tree. This binding is explicit in the
SoC code because BootROM does not pass a DTB into the early SRAM stage.
The driver's compatible table separates T-Head and SiFive rules:

- C900 CLINT does not expose an MMIO MTIME register at offset `0xbff8`.
  Read the `time` CSR instead and convert its 3 MHz ticks to microseconds.
- C900 MTIMECMP requires 32-bit MMIO. Write low=`0xffffffff`, then high,
  then the final low word, with ordering barriers between the writes.
- MSIP uses 32-bit MMIO.
- SiFive-compatible RV64 devices retain 64-bit timer MMIO. RV32 reads
  retry high/low/high when the counter rolls over.

The local timer hook in the RISC-V SBI code retains the old access
behavior by default. TH1520 overrides it through the matched CLINT driver.
No global change to all RISC-V MMIO accesses is made.

C910 early initialization was checked against the TH1520 U-Boot entry
and XuanTie `zero_stage_boot`. It enables the required vendor ISA and
CLINT extensions and cache coherency, and keeps L1 data caching disabled
through training and memory probing. Instruction caching is enabled in
SRAM; coherent data caching is enabled on every hart when entering
ramstage. Loaded code is published with C910 cache maintenance before
any hart executes it. BootROM starts hart 0; the port
releases the other three harts into coreboot's existing SMP rendezvous.

## DDR implementation and source

The DDR implementation is ported directly from
`revyos/th1520-vendor-uboot` at
`a13e24ed9ed773d4a07f079576a2fd654af6bfbb`, under
`board/thead/light-c910/lpddr4/`. It uses the LPDDR4X-3733 dual-rank,
64-bit, DBI-enabled profile. A DBI-off profile is not interchangeable.

The source is organized under `src/soc/thead/th1520/ddr/`:

- `controller.c`: PLL, reset, controller timings, address maps,
  AXI port enable and refresh, from `src/ddr_common_func.c`.
- `phy.c`: DQ pinmux, PMU load/start, 1D/2D sequencing and mission-mode
  register settings, from `src/pinmux.c` and
  `src/ddr_phy_fw/lp4x_3733_phy_train1d2d_dualrank.c`.
- `pmu_data.c`: the original four PMU instruction/data arrays.
- `mailbox.c`: the `src/waitfwdone.c` handshake and stream format,
  with one bounded deadline per PHY training pass.
- `init.c`: the cold-start stage ordering and capacity checks, based on
  `src/lpddr4_init.c` and the vendor board's `spl.c` boundary checks.

The controller and PHY use 32-bit and 16-bit MMIO respectively.
All controller polling is bounded. A `0x08` PMU message introduces a
stream header and its declared arguments; argument values such as
`0x07` and `0xff` must not be interpreted as completion or failure.
Arguments are consumed without a fixed-size buffer, under the same
training deadline. A failed training pass halts both PMUs.

The PHY PMU still runs ARC machine code. The original IMEM/DMEM values
are kept verbatim; this port does not claim to replace that algorithm.
The vendor project's GPL-2.0 license and source attribution are retained.

The cold path trains the PHY once with the 16 GiB map, then probes
physical address boundaries. If those addresses alias, it switches
the controller to the 8 GiB map without repeating PHY training and
checks the boundaries again. The DTB does not choose DDR capacity.
It does not import the vendor retention-state serialization or AON
consumer ABI. Deep-suspend restoration is outside this bring-up target.

The C910 feature reference is XuanTie's `zero_stage_boot` at
`6a5281e68b193637327d3a1ad1b54e43d5c70cf8`. The CLINT distinctions follow
OpenSBI's `thead_clint_quirks` in
`lib/utils/timer/fdt_timer_mtimer.c` and its 32-bit comparator accessor.

## Host checks

Pass a vendor source checkout to compare the native C implementation
against the original C functions, without an intermediate representation:

```sh
python3 util/thead/test-ddr.py /path/to/th1520-vendor-uboot
python3 util/riscv/test-clint.py
```

The PHY check compares all 69,982 ordered write/wait events, including
the 1D/2D labels, and all four training-failure exits. The controller
check compares the 182 writes in each 8/16 GiB profile. Mailbox checks
cover status-like arguments, streams longer than the vendor buffer,
truncated messages, handshake stalls and an endless progress stream.
The CLINT test covers C900 versus SiFive matching, RV32/RV64, rollover
and comparator ordering.

## Validation

The 16 GiB Meles completed BootROM USB RAM startup, both PHY training
passes, capacity detection, bank sampling, the four-hart transition and
ramstage table creation. The generic RISC-V hook also built and reached
ramstage on QEMU virt with four harts.

The development workspace retains the tested RAM image and serial log
under `../build/coreboot/meles-tested.rom` and
`../build/coreboot/meles-ram-bringup.log`. The normal SPI firmware and
its existing EDK2 image were not modified by this bring-up.

## Boot media

The default config follows BootROM OM[2:0]: DOWNLOAD values below 4 use
SRAM, 4 uses eMMC boot0 and 7 uses SPI NOR. Unsupported encodings halt.
The forced-source configs are `config.milkv_meles_spi` and
`config.milkv_meles_emmc`; `config.milkv_meles_storage_test` uses SRAM
and performs read-only storage checks before loading ramstage.

SPI uses QSPI0 with GPIO chip select and the shared DesignWare SSI
helper. The conservative divider does not change VIDEO_PLL. eMMC uses
the common SDHCI/MMC implementation in 8-bit SDR PIO at up to 25 MHz.
Its PHY is configured after the SDHCI reset, which clears PHY settings.
Partition switches preserve the eMMC boot-enable and acknowledgement
bits. The coreboot storage interface only reads the selected medium.

```sh
python3 util/thead/test-storage.py
```

Host checks cover SSI register layouts and timeouts, PIO read/write
completion, late errors, eMMC bus-width validation and partition-bit
preservation. Meles hardware passed SPI and eMMC boot0/user-area reads,
unaligned reads and switching back to boot0. These read checks do not
establish successful BootROM startup from either storage medium.

## EDK2 RAM recovery payload

The optional recovery config loads a minimal EDK2 Fastboot payload via
OpenSBI. It uses an existing relocatable `fw_dynamic.bin`: the packager
wraps that binary at the reserved `0x80000000` load address, without
relinking OpenSBI or changing its compiled `FW_TEXT_START`. OpenSBI
applies its own relocations. coreboot supplies `fw_dynamic_info` with
EDK2 entry `0x20000000`, S-mode and boot-hart lottery (`-1`), plus the
board DTB. OpenSBI adds its actual runtime memory reservations to the
DTB before entering EDK2.

Build the sibling EDK2 project's `meles-recovery` profile, then package
its FD with the existing OpenSBI binary and Meles DTB:

```sh
make -C ../th1520-edk2 edk2-recovery
python3 util/thead/build-recovery.py \
  --edk2-fd ../th1520-edk2/build/meles-recovery/TH1520.fd \
  --opensbi ../build/opensbi/platform/generic/firmware/fw_dynamic.bin \
  --dtb ../build/edk2/meles.dtb
```

The packager requires the version-1 RV64 OpenSBI firmware header and
checks its footprint against the 1 MiB reserved region. It retains the
OpenSBI binary's contents and supplies an ELF load envelope for the
coreboot loader. EDK2 is also wrapped as an ELF payload so its fixed
load address is honored; the RISC-V FIT loader places kernels itself.

At the earliest SoC hook, coreboot holds the BootROM USB controller
and PHY in reset until the payload initializes USB. Once DDR training
finishes, it copies the 1 MiB download image into a dedicated region at
22 MiB and compares the complete copy. Both stages then read CBFS from
DRAM. This region is separate from ramstage, stacks and the mapping
cache; the download itself still fits entirely in the BootROM SRAM.

The output is `build/recovery/coreboot.rom`, exactly 1 MiB, within the
1152 KiB BootROM SRAM limit. Download it with the SRAM commands above.
The recovery payload starts serial Fastboot and exposes raw `spi`,
`emmcboot0` and `emmcboot1` targets. The complete image contains no
U-Boot runtime and coreboot does not implement a USB Fastboot service.

`--boot-medium spi` or `--boot-medium emmc` instead builds a forced
storage-source image in `build/recovery-spi/` or `build/recovery-emmc/`.
The latter reads CBFS from boot0; writing it to boot1 does not add boot1
source selection or change eMMC boot-enable configuration.

Meles 16 GiB hardware completed the RAM recovery path and automatically
entered EDK2 Fastboot as `meles-recovery`. SPI and both 4 MiB eMMC boot
areas accepted a 1 MiB image and passed the writers' readback checks.
The eMMC user area and boot-selection settings were not modified.

The SPI-source recovery image restarted and returned to Fastboot.
To exercise eMMC CBFS independently of BootROM straps, the SPI boot
stage was temporarily replaced by the forced-eMMC boot stage. Logs
confirmed `CBFS source eMMC boot0` in both stages, followed by OpenSBI
and working EDK2 Fastboot. The SPI-source boot stage was then restored.
Direct BootROM eMMC startup and boot1 CBFS selection remain untested.

The development workspace retains the media validation logs under
`../build/coreboot/storage/recovery-spi-uart.txt`,
`../build/coreboot/storage/recovery-emmc-uart.txt` and
`../build/coreboot/storage/recovery-flash.log`. The tested images were
built from coreboot `f81d8b6b5b3a` and the EDK2 recovery implementation
ending at `b44f58361765`, using OpenSBI `105162c4f73a`.

## Boot performance and repeated startup

The initial recovery image inherited a 300 MHz CPU / 150 MHz bus clock
and left both L1 caches disabled. Without AON, the default selects 750 MHz CPU
while keeping the bus at 150 MHz using a divide-by-five setting.
Lock polling is bounded; failure leaves the inherited source selected.
The bus-divider ordering avoids transient overspeed when changing PLLs.
`TH1520_CPU_750MHZ=n` retains the inherited clock for diagnostics.
Voltage rails and VIDEO_PLL are unchanged; the OS can still apply its
own DVFS and bus clock configuration later.

Instruction caching is enabled in SRAM. Data caching stays disabled
for DDR training and capacity probing, then is enabled on every hart
in ramstage. Loaded code is cleaned only when data caching is enabled;
instruction synchronization is performed in either case.

Meles measured these timings with the final 750/150 MHz policy:

| Operation | Initial recovery | Final recovery |
| --- | --- | --- |
| SPI EDK2 read and decompression | 27.9 s | 1.67 s |
| Fastboot 1 MiB SPI write/readback | 88.0 s | 26.9 s |
| Fastboot 1 MiB eMMC boot write/readback | 16.3 s | 0.22 s |

The SPI read took 1552 ms for 245013 compressed bytes. The complete
software-reset-to-Fastboot check returned in about 6.5 s. The SPI
divider remains conservative and was not changed for these tests.

An earlier 750/375 MHz configuration intermittently stopped while
loading ramstage. Temporary stage logs narrowed this to LZMA decoding,
before BSS clearing and code publication. Skipping writeback with
D-cache disabled did not remove that failure. Controlled restarts gave:

| CPU / bus | Repeated-start result |
| --- | --- |
| 750 / 375 MHz | Stopped during decoding on the third restart |
| 300 / 150 MHz | Four restarts passed |
| 750 / 150 MHz control | Four restarts passed |
| 750 / 150 MHz final image | Ten restarts passed |

These results justify retaining the BootROM bus rate for this port;
they do not establish a silicon-wide maximum bus frequency. The final
image also passed eMMC boot0 CBFS loading via the temporary SPI boot
stage, and the SPI-source boot stage was restored afterward. Both eMMC
boot areas and SPI passed firmware write/readback verification.

The final functional source is `b6de0fa5fa83`. Host tests cover both PLL
parents, upward/downward transitions, oscillator selection and lock
failure. The earlier per-hart cache hook also passed four-hart QEMU virt
ramstage startup. Temporary generic-loader tracing has been removed.
Verification records are under `../build/coreboot/storage/`:
`bus150-final-reboots.json`, `bus150-final-flash.log`,
`bus150-emmc-boot-uart.log` and `bus150-spi-boot-uart.log`.

## Final RAM handoff validation

The later direct-SRAM test reached EDK2 decompression but stopped while
loading OpenSBI. Early USB takeover plus verified DRAM staging reached
EDK2 Fastboot while retaining the original relocatable `fw_dynamic.bin`.
The final RAM image is still 1048576 bytes. Thirty consecutive queries
over approximately thirty seconds returned the product and all three
raw target capacities without a disconnect. The RAM service then wrote
SPI and both eMMC boot areas, with readback verification passing.
The updated SPI image then passed three additional consecutive restarts
at approximately 6.5 seconds each.

These handoff changes are in `097b46c9479c` and `1222853a2b1d`. Evidence
is retained under `../build/coreboot/storage/` in
`ram-staging-final-success.txt`, `ram-staging-stability.json` and
`ram-staging-final-flash.log`. The generic-loader trace and experimental
frequency configurations are not part of the final source.
