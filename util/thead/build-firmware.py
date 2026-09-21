#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Package TH1520 coreboot, OpenSBI, EDK2 and optional board AON firmware."""
import argparse
import os
from pathlib import Path
import subprocess
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--profile", choices=("recovery", "normal"), default="recovery")
parser.add_argument("--aon", type=Path, help="board-configured AON firmware for 1.848 GHz")
parser.add_argument("--edk2-fd", required=True, type=Path)
parser.add_argument("--opensbi", required=True, type=Path,
                    help="existing relocatable fw_dynamic.bin")
parser.add_argument("--dtb", required=True, type=Path)
parser.add_argument("--boot-medium", choices=("ram", "spi", "emmc"))
parser.add_argument("--jobs", default="8")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
if args.boot_medium is None:
    args.boot_medium = "ram" if args.profile == "recovery" else "spi"
if args.profile == "normal" and args.boot_medium == "ram":
    parser.error("normal firmware exceeds SRAM; use the RAM recovery profile")
if args.profile == "normal" and args.aon is None:
    parser.error("normal firmware requires --aon for platform power management")
suffix = "" if args.boot_medium == "ram" else "-" + args.boot_medium
output = root / ("build/" + args.profile + suffix)
output.mkdir(parents=True, exist_ok=True)
fd, opensbi, dtb = [p.resolve(strict=True) for p in (args.edk2_fd, args.opensbi, args.dtb)]
if fd.stat().st_size != 4 * 1024 * 1024:
    parser.error("expected the 4 MiB TH1520 EDK2 firmware volume")
# Load the existing relocatable FW_DYNAMIC binary in coreboot's reserved
# OpenSBI region. Its own startup applies relocations; no relink is needed.
sbi = opensbi.read_bytes()
if len(sbi) < 128 or sbi[4:8] != b"OSBI":
    parser.error("expected fw_dynamic.bin with an OpenSBI firmware header")
version, xlen, footprint = struct.unpack_from("<III", sbi, 8)
if version != 1 or xlen != 64 or not len(sbi) <= footprint <= 0x100000:
    parser.error("OpenSBI firmware must fit its reserved 1 MiB region")
prefix = os.environ.get("CROSS_COMPILE_riscv", "riscv64-linux-gnu-")


def wrap_binary(source, name, address, memory_size):
    script = output / (name + ".ld")
    script.write_text(f"""OUTPUT_ARCH(riscv)
ENTRY(_start)
PHDRS {{ firmware PT_LOAD FLAGS(7); }}
SECTIONS {{
  . = {address:#x};
  _start = .;
  .firmware : {{ *(.data) }} :firmware
  .bss (NOLOAD) : {{ . += {memory_size - source.stat().st_size:#x}; }} :firmware
}}
""")
    obj = output / (name + ".o")
    target = output / (name + ".elf")
    subprocess.run([prefix + "objcopy", "-I", "binary", "-O", "elf64-littleriscv",
                    "-B", "riscv", str(source), str(obj)], check=True)
    subprocess.run([prefix + "ld", "-m", "elf64lriscv", "-T", str(script),
                    "-o", str(target), str(obj)], check=True)
    return target


payload = wrap_binary(fd, "edk2", 0x20000000, fd.stat().st_size)
opensbi = wrap_binary(opensbi, "opensbi", 0x80000000, footprint)
config = root / ("build/" + args.profile + suffix + ".config")
template = "recovery" if args.profile == "recovery" else "edk2"
text = (root / ("configs/config.milkv_meles_" + template)).read_text()
if args.aon is not None:
    aon = args.aon.resolve(strict=True)
    blob = aon.read_bytes()
    if not (0xc00 < len(blob) <= 0x10000 and blob[:10] == b"AON_CONFIG" and
            blob[22:31] == b"UBOOT_SET"):
        parser.error("AON input must contain a valid board-configured image")
    text += "CONFIG_TH1520_AON=y\n"
    args.aon = aon
text = text.replace("CONFIG_TH1520_BOOT_" + ("RAM" if template == "recovery" else "SPI") + "=y",
                    "CONFIG_TH1520_BOOT_" + args.boot_medium.upper() + "=y")
inputs = [("OPENSBI_BLOB_PATH", opensbi), ("TH1520_PAYLOAD_DTB", dtb),
          ("PAYLOAD_FILE", payload)]
if args.aon is not None:
    inputs.append(("TH1520_AON_FILE", args.aon))
for name, value in inputs:
    value = str(value)
    if any(c in value for c in ('"', '\\', '\n')):
        parser.error("paths containing quotes, backslashes or newlines are unsupported")
    text += f'CONFIG_{name}="{value}"\n'
config.write_text(text)
env = os.environ | {"CROSS_COMPILE_riscv": prefix}
command = ["make", "UPDATED_SUBMODULES=1", "obj=" + str(output),
           "DOTCONFIG=" + str(config)]
subprocess.run(command + ["olddefconfig"], cwd=root, env=env, check=True)
if "CONFIG_PAYLOAD_ELF=y" not in config.read_text():
    raise SystemExit("recovery configuration did not enable the EDK2 payload")
subprocess.run(command + ["-j" + args.jobs], cwd=root, env=env, check=True)
image = output / "coreboot.rom"
size = image.stat().st_size
if args.boot_medium == "ram" and size > 0x120000:
    raise SystemExit(f"RAM recovery image exceeds BootROM limit: {size} bytes")
limit = 0x400000 if args.boot_medium == "emmc" else 0x800000
if args.boot_medium != "ram" and size > limit:
    raise SystemExit(f"storage firmware exceeds target capacity: {size} bytes")
print(f"{args.profile.capitalize()} image ({args.boot_medium}): {image} ({size} bytes)")
