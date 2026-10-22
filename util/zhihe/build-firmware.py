#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build Melon Pi RAM or eMMC boot0 firmware with a native bootstrap."""
import argparse
import os
from pathlib import Path
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--board", choices=("melonpi-v1", "melonpi-v2"),
                    default="melonpi-v2",
                    help="carrier revision; must match the supplied DTB")
parser.add_argument("--opensbi", required=True, type=Path)
parser.add_argument("--scmi", required=True, type=Path,
                    help="A210 E902 Zephyr SCMI firmware, at most 63 KiB")
payload_group = parser.add_mutually_exclusive_group(required=True)
payload_group.add_argument("--payload", type=Path, help="S-mode ELF payload")
payload_group.add_argument("--edk2-fd", type=Path,
                           help="Melon Pi A210.fd firmware volume")
parser.add_argument("--dtb", required=True, type=Path,
                    help="board DTB built from the target kernel")
parser.add_argument("--output", required=True, type=Path)
parser.add_argument("--jobs", default="8")
parser.add_argument("--boot-medium", choices=("ram", "emmc"), default="ram")
args = parser.parse_args()
compatible = subprocess.run(
    ["fdtget", "-t", "s", str(args.dtb), "/", "compatible"],
    capture_output=True, text=True)
expected = "zhihe,a210-" + args.board
names = compatible.stdout.split()
if compatible.returncode or not names:
    parser.error("cannot read the board DTB compatible")
if names[0] != expected:
    parser.error(f"{args.board} requires a {expected} DTB, got {names[0]}")
board_settings = f"CONFIG_MELONPI_V{args.board[-1]}=y\n"
if not 0 < args.scmi.stat().st_size <= 63 * 1024:
    parser.error("SCMI firmware must fit below the shared-memory window")
root = Path(__file__).resolve().parents[2]
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
env = os.environ | {
    "CROSS_COMPILE_riscv": os.environ.get(
        "CROSS_COMPILE_riscv", "riscv64-linux-gnu-")}

# SELF loads at physical addresses. OpenSBI's default PIC ELF starts at
# zero; wrap its unchanged loadable bytes at the reserved RAM window.
# OpenSBI still performs its own PIC relocation and FW_DYNAMIC handoff.
sbi = args.opensbi.read_bytes()
if len(sbi) < 64 or sbi[:7] != b"\x7fELF\x02\x01\x01":
    parser.error("OpenSBI must be a little-endian ELF64 image")
header = struct.unpack_from("<16sHHIQQQIHHHHHH", sbi)
entry, phoff, phsize, phcount = header[4], header[5], header[9], header[10]
if (header[1] not in (2, 3) or header[2] != 243 or phsize != 56 or
        not phcount or phoff + phsize * phcount > len(sbi)):
    parser.error("invalid RISC-V OpenSBI ELF program headers")
segments = [struct.unpack_from("<IIQQQQQQ", sbi, phoff + i * phsize)
            for i in range(phcount)]
segments = [segment for segment in segments if segment[0] == 1]
if not segments:
    parser.error("OpenSBI has no loadable segments")
bias = (0x80000000 if header[1] == 3 and
        min(segment[4] for segment in segments) == 0 else 0)
executable = False
for _, flags, offset, vaddr, address, filesz, memsz, _ in segments:
    if (filesz > memsz or offset + filesz > len(sbi) or
            not 0x80000000 <= address + bias <
            address + bias + memsz <= 0x80100000):
        parser.error("OpenSBI load segments exceed its 1 MiB RAM window")
    if bias and (vaddr != address or address % 4096):
        parser.error("unsupported zero-based OpenSBI PIC segment layout")
    executable |= bool(flags & 1 and address <= entry < address + filesz)
if not executable:
    parser.error("OpenSBI entry is outside its executable load segments")
if bias:
    image = bytearray(4096)
    program_headers = bytearray()
    for _, flags, offset, address, _, filesz, memsz, _ in segments:
        image.extend(bytes(-len(image) % 4096))
        target = address + bias
        program_headers.extend(struct.pack(
            "<IIQQQQQQ", 1, flags, len(image), target, target,
            filesz, memsz, 4096))
        image.extend(sbi[offset:offset + filesz])
    if 64 + len(program_headers) > 4096:
        parser.error("too many OpenSBI loadable segments")
    image[:64] = struct.pack(
        "<16sHHIQQQIHHHHHH", header[0], 2, 243, 1,
        entry + bias, 64, 0, header[7], 64, 56, len(segments), 0, 0, 0)
    image[64:64 + len(program_headers)] = program_headers
    args.opensbi = output / "opensbi-load.elf"
    args.opensbi.write_bytes(image)


if args.edk2_fd:
    # The FD reset vector is executable at its linked address. Preserve
    # the complete volume, including erased padding, for SEC FV parsing.
    fd = args.edk2_fd.read_bytes()
    if len(fd) != 4 * 1024 * 1024 or fd[40:44] != b"_FVH":
        parser.error("expected the 4 MiB Melon Pi A210 firmware volume")
    edk2_base = 0x88000000
    pe = fd.find(b"PE\0\0")
    if (pe < 0 or pe + 56 > len(fd) or
            not edk2_base <= struct.unpack_from("<Q", fd, pe + 48)[0]
            < edk2_base + len(fd)):
        parser.error("SEC image is not linked in the Melon Pi FD range")
    elf_ident = b"\x7fELF\x02\x01\x01" + bytes(9)
    header = struct.pack("<16sHHIQQQIHHHHHH", elf_ident, 2, 243, 1,
                         edk2_base, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    segment = struct.pack("<IIQQQQQQ", 1, 7, 4096, edk2_base,
                          edk2_base, len(fd), len(fd), 4096)
    args.payload = output / "edk2.elf"
    args.payload.write_bytes((header + segment).ljust(4096, b"\0") + fd)


def build(name, settings, raw_only=False):
    directory = output / name
    directory.mkdir(exist_ok=True)
    config = directory / ".config"
    config.write_text(settings)
    cmd = ["make", "UPDATED_SUBMODULES=1", f"obj={directory}",
           f"DOTCONFIG={config}"]
    subprocess.run(cmd + ["olddefconfig"], cwd=root, env=env, check=True)
    target = directory / ("cbfs/fallback/bootblock.raw.bin" if raw_only
                          else "coreboot.rom")
    subprocess.run(cmd + ["-j" + args.jobs, str(target)],
                   cwd=root, env=env, check=True)
    return target


settings = (root / "configs/config.zhihe_melonpi_firmware").read_text()
settings += board_settings
for name, path in (("OPENSBI_BLOB_PATH", args.opensbi),
                   ("PAYLOAD_FILE", args.payload),
                   ("A210_PAYLOAD_DTB", args.dtb),
                   ("A210_SCMI_FILE", args.scmi)):
    path = str(path.resolve(strict=True))
    if any(c in path for c in ('"', "\\", "\n")):
        parser.error("input paths must not contain quotes or escapes")
    settings += f'CONFIG_{name}="{path}"\n'
settings += f"CONFIG_A210_BOOT_RAM={'y' if args.boot_medium == 'ram' else 'n'}\n"
rom = build("firmware", settings)

def assembly_stub(name, address, extra=()):
    source = root / f"src/soc/zhihe/a210/{name}.S"
    obj = output / (name + ".o")
    elf = output / (name + ".elf")
    binary = output / (name + ".bin")
    prefix = env["CROSS_COMPILE_riscv"]
    subprocess.run([prefix + "gcc", "-c", "-march=rv64imafdc_zicsr_zifencei",
                    "-mabi=lp64d", "-mno-relax",
                    "-I" + str(root / "src/soc/zhihe/a210/include"),
                    *extra, str(source), "-o", str(obj)], check=True)
    subprocess.run([prefix + "ld", "--no-relax", "-Ttext=" + hex(address),
                    "-e", "a210_" + name, str(obj), "-o", str(elf)],
                   check=True)
    subprocess.run([prefix + "objcopy", "-O", "binary", str(elf),
                    str(binary)], check=True)
    return binary.read_bytes()


def rvbl(code):
    code += bytes((-len(code)) % 16)
    length = len(code) + 2048
    header = struct.pack("<I4sII", 0x0010006f, b"RVBL", length, 0)
    return header + bytes(2048 - len(header)) + code


settings = (root / "configs/config.zhihe_melonpi_sram").read_text()
settings += board_settings
settings = settings.replace("CONFIG_CONSOLE_CBMEM=y",
                            "CONFIG_CONSOLE_CBMEM=n")
settings += "CONFIG_A210_BOOTSTRAP=y\n"
settings += f"CONFIG_A210_BOOT_RAM={'y' if args.boot_medium == 'ram' else 'n'}\n"
bootstrap = build("bootstrap", settings, raw_only=True).read_bytes()
entry = assembly_stub("rom_entry", 0x100800)
if len(entry) > 512:
    raise SystemExit("BootROM context entry exceeds its reserved space")
bootstrap = rvbl(entry.ljust(512, b"\0") + bootstrap)
if len(bootstrap) > 0x8000:
    raise SystemExit("bootstrap exceeds the 32 KiB initial image budget")
(output / f"{args.board}-bootstrap-rvbl.bin").write_bytes(bootstrap)
if args.boot_medium == "ram":
    ddr = (output / "firmware/cbfs/fallback/bootblock.raw.bin").read_bytes()
    if rom.read_bytes()[:len(ddr)] != ddr or len(ddr) % 4:
        raise SystemExit("unexpected DDR stage placement in CBFS image")
    entry = assembly_stub("ram_entry", 0x70000800,
                          [f"-DA210_DDR_IMAGE_SIZE={len(ddr)}"])
    if len(entry) > 2048:
        raise SystemExit("RAM copy entry exceeds its reserved space")
    image = rvbl(entry.ljust(2048, b"\0") + rom.read_bytes())
    if len(image) > 2 * 1024 * 1024:
        raise SystemExit("RAM firmware exceeds OCRAM")
    destination = output / f"{args.board}-coreboot-ram-rvbl.bin"
else:
    image = bootstrap.ljust(0x8000, b"\xff") + rom.read_bytes()
    if len(image) > 0x3f0000:
        raise SystemExit("firmware overlaps the eMMC boot0 MAC store")
    destination = output / f"{args.board}-coreboot-boot0.bin"
destination.write_bytes(image)
print(f"{args.boot_medium} firmware: {destination} ({len(image)} bytes)")
