#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compatibility entry point for the TH1520 RAM recovery image builder."""
from pathlib import Path
import runpy

runpy.run_path(str(Path(__file__).with_name("build-firmware.py")), run_name="__main__")
