#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn>
"""Check native TH1520 DDR sequences and errors against vendor C."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("vendor", type=Path, help="th1520-vendor-uboot checkout")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = root / "src/soc/thead/th1520/ddr"
vendor = args.vendor.resolve() / "board/thead/light-c910/lpddr4"

def no_includes(path):
    return re.sub(r'^\s*#include[^\n]*', '', path.read_text(), flags=re.M)

with tempfile.TemporaryDirectory() as temp:
    work = Path(temp)
    headers = {
        "types.h": "#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n",
        "commonlib/helpers.h": (
            "#include <types.h>\n"
            "#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))\n"
            "#define GiB (1ULL << 30)\n"
        ),
        "device/mmio.h": (
            "#include <types.h>\n"
            "void write16(void *, uint16_t);\n"
            "void write32(void *, uint32_t);\n"
            "uint16_t read16(const void *);\n"
            "uint32_t read32(const void *);\n"
        ),
        "console/console.h": (
            "#define BIOS_INFO 0\n#define BIOS_DEBUG 1\n#define BIOS_ERR 2\n"
            "static inline int printk(int level, const char *fmt, ...)\n"
            "{ (void)level; (void)fmt; return 0; }\n"
        ),
        "timer.h": (
            "unsigned int mock_time(void);\n"
            "struct stopwatch { unsigned int end; };\n"
            "static inline void stopwatch_init_msecs_expire(struct stopwatch *s, unsigned ms)\n"
            "{ s->end = mock_time() + ms; }\n"
            "static inline int stopwatch_expired(struct stopwatch *s)\n"
            "{ return mock_time() >= s->end; }\n"
            "#define wait_us(us, cond) ((cond) ? 1 : 0)\n"
        ),
        "delay.h": "void udelay(unsigned int us);\n",
    }
    for name, contents in headers.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
    includes = ["-I" + str(work), "-I" + str(source),
                "-I" + str(root / "src/soc/thead/th1520/include")]
    def compile_test(name, paths, extra=()):
        binary = work / name
        subprocess.run(["cc", "-std=gnu11", "-O1", "-Wall", "-Wextra",
                        *includes, *extra, *map(str, paths), "-o", str(binary)],
                       check=True)
        return binary

    mailbox = compile_test("mailbox", [source / "mailbox.c",
                           root / "util/thead/tests/mailbox.c"])
    subprocess.run([str(mailbox)], check=True)
    native = compile_test("phy", [source / "phy.c", source / "pmu_data.c",
                          root / "util/thead/tests/phy-trace.c"])
    trace = subprocess.check_output([str(native)])
    vendor_shim = r'''
#include <stdint.h>
#include <stdio.h>
enum DDR_TYPE { DDR_TYPE_LPDDR4X, DDR_TYPE_LPDDR4 };
enum DDR_BITWIDTH { DDR_BITWIDTH_16=16, DDR_BITWIDTH_32=32, DDR_BITWIDTH_64=64 };
#define printf(...) ((void)0)
static void event(unsigned phy, uint32_t addr, uint16_t value) {
    unsigned char b[7]={phy,addr,addr>>8,addr>>16,addr>>24,value,value>>8};
    fwrite(b,7,1,stdout);
}
void lp4x_3733_phy_train1d2d(void);
void ddr_phy0_reg_wr(unsigned long a,unsigned v) {event(0,a,v);}
void ddr_phy1_reg_wr(unsigned long a,unsigned v) {event(1,a,v);}
void ddr_phy_reg_wr(unsigned long a,unsigned v) {event(0,a,v);event(1,a,v);}
void ddr_phy_broadcast_en(int enable) {(void)enable;}
void dwc_ddrphy_phyinit_userCustom_G_waitFwDone(unsigned char d) {event(2,0,d+1);}
void dwc_ddrphy1_phyinit_userCustom_G_waitFwDone(unsigned char d) {event(2,1,d+1);}
'''
    vendor_c = work / "vendor.c"
    vendor_c.write_text(vendor_shim + no_includes(vendor / "src/pinmux.c") +
        no_includes(vendor / "src/ddr_phy_fw/lp4x_3733_phy_train1d2d_dualrank.c") +
        "\nint main(void) { dq_pinmux(DDR_BITWIDTH_64); "
        "lp4_phy_train1d2d(DDR_TYPE_LPDDR4X,3733,DDR_BITWIDTH_64); return 0; }\n")
    reference = compile_test("vendor", [vendor_c], ["-Wno-unused-parameter"])
    expected = subprocess.check_output([str(reference)])
    assert trace == expected, "native PHY writes differ from vendor C"
    events = list(struct.iter_unpack("<BIH", trace))
    for fail in range(1, 5):
        result = subprocess.run([str(native)], capture_output=True,
                                env={**os.environ, "FAIL_WAIT": str(fail)})
        waits = [i for i, event in enumerate(events) if event[0] == 2]
        prefix = trace[:(waits[fail - 1] + 1) * 7]
        halt = b"".join(struct.pack("<BIH", phy, 0xd0099, 1) for phy in (0, 1))
        assert result.returncode == 1 and result.stdout == prefix + halt
    print(f"PASS: {len(events)} PHY events equal vendor C, all four failure exits halt PMUs")
    print("PHY trace SHA256:", hashlib.sha256(trace).hexdigest())

    controller = compile_test("controller", [source / "controller.c",
                              root / "util/thead/tests/controller-trace.c"])
    common = no_includes(vendor / "src/ddr_common_func.c")
    common = re.sub(r"/\*.*?\*/|//[^\n]*", "", common, flags=re.S)
    def function(name):
        match = re.search(r"\bvoid\s+" + name + r"\s*\([^;]*?\)\s*\{", common)
        assert match, name
        end = match.end()
        depth = 1
        while depth:
            depth += (common[end] == "{") - (common[end] == "}")
            end += 1
        return common[match.start():end]
    names = ("pll_config", "deassert_pwrok_apb", "ctrl_init", "addrmap",
             "de_assert_other_reset_ddr", "ctrl_en", "enable_axi_port",
             "enable_auto_refresh", "lpddr4_auto_selref")
    controller_shim = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define _DDR_CTRL_BADDR 0xffff000000ULL
#define DDR_SYSREG_BADDR 0xffff005000ULL
#include "DWC_ddr_umctl2_header.h"
#include "ddr_reg_define.h"
#include "ddr_sysreg_registers.h"
#include "ddr_sysreg_registers_struct.h"
#include "ddr_common_func.h"
#define printf(...) ((void)0)
DDR_SYSREG_REG_SW_REG_S ddr_sysreg;
void write32(void *,uint32_t);
uint32_t read32(const void *);
void udelay(unsigned int);
void wr(uint64_t addr,uint32_t value) {write32((void *)(uintptr_t)addr,value);}
uint32_t rd(uint64_t addr) {return read32((void *)(uintptr_t)addr);}
void ddr_sysreg_wr(unsigned long reg,unsigned value) {wr(DDR_SYSREG_BADDR+reg,value);}
unsigned ddr_sysreg_rd(unsigned long reg) {return rd(DDR_SYSREG_BADDR+reg);}
'''
    controller_vendor = work / "vendor-controller.c"
    controller_vendor.write_text(controller_shim + "\n".join(function(n) for n in names) +
        "\nint main(void) { pll_config(3733); deassert_pwrok_apb(DDR_BITWIDTH_64); "
        "ctrl_init(2,3733); addrmap(2,DDR_BITWIDTH_64); de_assert_other_reset_ddr(); "
        "ctrl_en(DDR_BITWIDTH_64); enable_axi_port(31); enable_auto_refresh(); "
        "lpddr4_auto_selref(); return 0; }\n")
    for capacity in (8, 16):
        flags = ["-DVENDOR_TRACE", "-I" + str(vendor / "include")]
        if capacity == 16:
            flags.append("-DCONFIG_DDR_DDP")
        baseline = compile_test(f"vendor-controller-{capacity}", [controller_vendor,
                                root / "util/thead/tests/controller-trace.c"], flags)
        reference = subprocess.check_output([str(baseline)], timeout=5)
        actual = subprocess.check_output([str(controller), str(capacity)])
        if actual != reference:
            a = list(struct.iter_unpack("<QI", actual))
            b = list(struct.iter_unpack("<QI", reference))
            first = next((i for i, pair in enumerate(zip(a,b)) if pair[0] != pair[1]), min(len(a),len(b)))
            raise AssertionError(f"controller {capacity}: first mismatch {first}: {a[first:first+3]} != {b[first:first+3]}")
        print(f"PASS: {capacity} GiB controller sequence equals vendor C ({len(actual)//12} writes)")
    for register in (0xffff000004, 0xffff0001bc, 0xffff001cbc, 0xffff000324):
        result = subprocess.run([str(controller), "16"], capture_output=True,
                                env={**os.environ, "FAIL_REG": hex(register)})
        assert result.returncode == 1, hex(register)
    print("PASS: controller polling failures propagate to the caller")
