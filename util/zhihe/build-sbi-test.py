#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build the Melon Pi S-mode OpenSBI and HSM smoke-test payload."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parent
output = args.output.resolve()
output.parent.mkdir(parents=True, exist_ok=True)
prefix = os.environ.get("CROSS_COMPILE_riscv", "riscv64-linux-gnu-")
tests = root / "tests"
subprocess.run([
    prefix + "gcc", "-Os", "-march=rv64imafdc_zicsr_zifencei",
    "-mabi=lp64d", "-mcmodel=medany", "-fno-pie", "-no-pie",
    "-fno-stack-protector", "-fno-builtin", "-nostdlib",
    "-Wl,-T," + str(tests / "sbi-smoke.ld"),
    "-o", str(output), str(tests / "sbi-smoke.S"),
    str(tests / "sbi-smoke.c")], check=True)
print(output)
