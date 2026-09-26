#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build the A210 BRAM diagnostic and its non-secure RVBL container."""
import argparse
from pathlib import Path
import os
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path)
parser.add_argument("--jobs", default="8")
parser.add_argument("--ddr", action="store_true",
                    help="Build the larger DDR diagnostic for JTAG")
parser.add_argument("--emmc", action="store_true",
                    help="Include read-only eMMC diagnostics")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
output = (args.output or root / "build/a210-sram").resolve()
output.mkdir(parents=True, exist_ok=True)
config = output / ".config"
settings = (root / "configs/config.zhihe_melonpi_sram").read_text()
settings += f"\nCONFIG_A210_DDR_TEST={'y' if args.ddr else 'n'}\n"
settings += f"CONFIG_A210_EMMC_TEST={'y' if args.emmc else 'n'}\n"
config.write_text(settings)
env = dict(os.environ)
env.setdefault("CROSS_COMPILE_riscv", "riscv64-linux-gnu-")
command = ["make", "UPDATED_SUBMODULES=1", f"obj={output}",
           f"DOTCONFIG={config}"]
subprocess.run(command + ["olddefconfig"], cwd=root, env=env, check=True)
raw = output / "cbfs/fallback/bootblock.raw.bin"
subprocess.run(command + ["-j" + args.jobs, str(raw)], cwd=root,
               env=env, check=True)

# Vendor generate_rvbl(): 2 KiB header, 16-byte code padding, no payload.
code = raw.read_bytes()
image = output / "a210-coreboot-sram-rvbl.bin"
if args.ddr:
    image.unlink(missing_ok=True)
    print(f"DDR diagnostic for JTAG: {raw} ({len(code)} bytes)")
    raise SystemExit(0)
code += bytes((-len(code)) % 16)
size = 2048 + len(code)
if size > 32 * 1024:
    raise SystemExit("Diagnostic exceeds the BootROM first-image budget")
header = struct.pack("<I4sII", 0x0010006f, b"RVBL", size, 0)
image.write_bytes(header + bytes(2048 - len(header)) + code)
print(f"BRAM diagnostic: {image} ({size} bytes)")
