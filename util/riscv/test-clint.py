#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn>
"""Host checks for compatible-specific CLINT access width and ordering."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = root / "src/drivers/riscv/clint/clint.c"
with tempfile.TemporaryDirectory() as temp:
    work = Path(temp)
    headers = {
        "drivers/riscv/clint.h": (
            root / "src/include/drivers/riscv/clint.h"
        ).read_text(),
        "types.h": (
            "#include <stdint.h>\n#include <stddef.h>\n"
            "#include <stdbool.h>\n"
        ),
        "arch/barrier.h": "void mock_barrier(void);\n#define mb() mock_barrier()\n",
        "arch/encoding.h": (
            "#include <stdint.h>\nuint64_t mock_csr(const char *);\n"
            "#define read_csr(r) mock_csr(#r)\n"
        ),
        "commonlib/helpers.h": (
            "#include <types.h>\n"
            "#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))\n"
        ),
        "console/console.h": "_Noreturn void die(const char *reason);\n",
        "device/mmio.h": (
            "#include <types.h>\n"
            "uint32_t read32(const void *);\n"
            "uint64_t read64(const void *);\n"
            "void write32(void *, uint32_t);\n"
            "void write64(void *, uint64_t);\n"
        ),
    }
    for name, text in headers.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    (work / "test.c").write_text(r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <drivers/riscv/clint.h>

#define BASE 0x100000UL
static struct { unsigned width; uintptr_t addr; uint64_t value; } events[8];
static unsigned count, mmio_reads, csr_reads, high_reads, low_reads;
static const struct riscv_clint *active;
static jmp_buf bad_compatible;
static bool expect_bad;

static void record(unsigned width, void *addr, uint64_t value)
{
    assert(count < 8);
    events[count].width = width;
    events[count].addr = (uintptr_t)addr;
    events[count++].value = value;
}
void write32(void *addr, uint32_t value) { record(32, addr, value); }
void write64(void *addr, uint64_t value) { record(64, addr, value); }
void mock_barrier(void) { record(0, 0, 0); }

uint64_t mock_csr(const char *reg)
{
    assert(!strcmp(active->compatible, "thead,c900-clint"));
    csr_reads++;
#if __riscv_xlen == 64
    assert(!strcmp(reg, "time"));
    return 0x23456789aULL;
#else
    if (!strcmp(reg, "timeh"))
        return ++high_reads == 1 ? 1 : 2;
    assert(!strcmp(reg, "time"));
    return ++low_reads == 1 ? UINT32_MAX : 7;
#endif
}

uint32_t read32(const void *addr)
{
    assert(strcmp(active->compatible, "thead,c900-clint"));
    mmio_reads++;
    if ((uintptr_t)addr == BASE + 0xbffc)
        return ++high_reads == 1 ? 1 : 2;
    assert((uintptr_t)addr == BASE + 0xbff8);
    return ++low_reads == 1 ? UINT32_MAX : 7;
}

uint64_t read64(const void *addr)
{
    assert(__riscv_xlen == 64);
    assert(strcmp(active->compatible, "thead,c900-clint"));
    assert((uintptr_t)addr == BASE + 0xbff8);
    mmio_reads++;
    return 0x23456789aULL;
}

_Noreturn void die(const char *reason)
{
    (void)reason;
    assert(expect_bad);
    longjmp(bad_compatible, 1);
}

int main(void)
{
    static const char *names[] = {
        "thead,c900-clint", "riscv,clint0", "sifive,clint0", "sifive,clint2",
    };
    static const uint64_t values[] = {
        0, UINT32_MAX, 0x100000000ULL, UINT64_MAX,
    };
    for (unsigned i = 0; i < 4; i++) {
        struct riscv_clint clint = { names[i], BASE };
        active = &clint;
        mmio_reads = csr_reads = high_reads = low_reads = 0;
        uint64_t value = riscv_clint_read_time(&clint);
        assert(value == (__riscv_xlen == 64 ? 0x23456789aULL : 0x200000007ULL));
        assert((i == 0) == (mmio_reads == 0));
        assert((i == 0) == (csr_reads != 0));
        for (unsigned hart = 0; hart < 4; hart++) {
            for (unsigned j = 0; j < 4; j++) {
                uintptr_t cmp = BASE + 0x4000 + 8 * hart;
                count = 0;
                riscv_clint_set_timer(&clint, hart, values[j]);
                if (i == 0 || __riscv_xlen == 32) {
                    assert(count == 6);
                    assert(events[0].width == 32 && events[0].addr == cmp);
                    assert(events[0].value == UINT32_MAX);
                    assert(events[2].width == 32 && events[2].addr == cmp + 4);
                    assert(events[2].value == values[j] >> 32);
                    assert(events[4].width == 32 && events[4].addr == cmp);
                    assert(events[4].value == (uint32_t)values[j]);
                    assert(!events[1].width && !events[3].width && !events[5].width);
                } else {
                    assert(count == 2 && events[0].width == 64);
                    assert(events[0].addr == cmp && events[0].value == values[j]);
                    assert(!events[1].width);
                }
            }
            count = 0;
            riscv_clint_set_msip(&clint, hart, true);
            assert(count == 3 && !events[0].width && !events[2].width);
            assert(events[1].width == 32);
            assert(events[1].addr == BASE + 4 * hart && events[1].value == 1);
        }
    }
    struct riscv_clint unknown = { "thead,unmatched-clint", BASE };
    active = &unknown;
    expect_bad = true;
    if (!setjmp(bad_compatible)) {
        riscv_clint_read_time(&unknown);
        assert(!"unknown compatible accepted");
    }
    puts("PASS: C900 CSR/32-bit path, SiFive path, rollover and write ordering");
    return 0;
}
''')
    for width in (32, 64):
        executable = work / f"test-{width}"
        subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            f"-D__riscv_xlen={width}", "-I" + str(work),
            str(source),
            str(work / "test.c"), "-o", str(executable),
        ], check=True)
        print(f"RV{width}:", flush=True)
        subprocess.run([str(executable)], check=True)
