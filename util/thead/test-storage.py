#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as temporary:
    work = Path(temporary)
    headers = {
        "arch/barrier.h": "void mb(void);\n",
        "device/device.h": "#include <types.h>\nstruct device;struct device_operations{void (*read_resources)(struct device*);void (*set_resources)(struct device*);};struct device{struct{int type;}path;struct device_operations *ops;};struct chip_operations{const char *name;void (*init)(void*);void (*enable_dev)(struct device*);};\n#define DEVICE_PATH_CPU_CLUSTER 1\nvoid ram_from_to(struct device*,unsigned,uint64_t,uint64_t);void noop_set_resources(struct device*);\n",
        "bootstate.h": "#define BOOT_STATE_INIT_ENTRY(...)\n",
        "cbfs.h": "#include <types.h>\nvoid *cbfs_map(const char *,size_t *);void cbfs_unmap(void *);\n",
        "types.h": "#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n#include <sys/types.h>\ntypedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint64_t u64;\n",
        "commonlib/bsd/stdlib.h": "#include <stdlib.h>\n",
        "commonlib/region.h": "#include <types.h>\nstruct region_device;\n",
        "commonlib/helpers.h": "#include <types.h>\n#define KiB 1024UL\n#define MiB (1024UL*KiB)\n#define ALIGN_UP(x,a) (((x)+(a)-1)&~((a)-1))\n#define MIN(a,b) ((a)<(b)?(a):(b))\n#define MAX(a,b) ((a)>(b)?(a):(b))\n#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))\n#define __weak __attribute__((weak))\n#define __aligned(x) __attribute__((aligned(x)))\n#define __packed __attribute__((packed))\n#define IS_ENABLED(x) 0\n#ifndef CONFIG\n#define CONFIG(x) 0\n#endif\n#define ENV_BOOTBLOCK 1\n#define ENV_SEPARATE_VERSTAGE 0\n#define ENV_SEPARATE_ROMSTAGE 0\n#define ENV_POSTCAR 0\n#ifndef ENV_RAMSTAGE\n#define ENV_RAMSTAGE 0\n#endif\n",
        "device/mmio.h": "#include <types.h>\nuint8_t read8(const void*);uint16_t read16(const void*);uint32_t read32(const void*);void write8(void*,uint8_t);void write16(void*,uint16_t);void write32(void*,uint32_t);\n",
        "console/console.h": "#include <commonlib/helpers.h>\n#define BIOS_INFO 0\n#define BIOS_WARNING 0\n#define BIOS_DEBUG 0\n#define BIOS_ERR 0\n#define BIOS_SPEW 0\nstatic inline int printk(int l,const char*f,...){(void)l;(void)f;return 0;}\n",
        "delay.h": "void udelay(unsigned int);void mdelay(unsigned int);\n",
        "timer.h": "unsigned int mock_time(void);struct stopwatch{unsigned int end;};static inline void stopwatch_init_msecs_expire(struct stopwatch*s,unsigned ms){s->end=mock_time()+ms*1000;}static inline int stopwatch_expired(struct stopwatch*s){return mock_time()>=s->end;}\n#define wait_us(us,condition) ({unsigned n=100;while(n&&!(condition))n--;n;})\n",
        "lib.h": "#include <commonlib/helpers.h>\n#define dma_coherent(x) 1\nstatic inline unsigned __fls(unsigned x){return 31-__builtin_clz(x);}\n",
        "cbmem.h": "#include <types.h>\n#define CBMEM_ID_MMC_STATUS 0\nvoid *cbmem_find(unsigned);void *cbmem_add(unsigned,size_t);\n",
    }
    for path in ("commonlib/sdhci.h", "commonlib/sd_mmc_ctrlr.h", "commonlib/storage.h"):
        headers[path] = (root / "src/commonlib/include" / path).read_text()
    headers["drivers/spi/designware.h"] = (root / "src/include/drivers/spi/designware.h").read_text()
    headers["soc/addressmap.h"] = (root / "src/soc/thead/th1520/include/soc/addressmap.h").read_text()
    headers["soc/cpu.h"] = (root / "src/soc/thead/th1520/include/soc/cpu.h").read_text()
    headers["soc/iopmp.h"] = (root / "src/soc/thead/th1520/include/soc/iopmp.h").read_text()
    for name, value in headers.items():
        p = work / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(value)
    for name, sources in {
        "iopmp": ["util/thead/tests/iopmp.c"],
        "aon": ["util/thead/tests/aon.c"],
        "cpu-clock": ["util/thead/tests/cpu-clock.c"],
        "spi": ["util/thead/tests/spi.c", "src/drivers/spi/designware/spi.c"],
        "sdhci": ["util/thead/tests/sdhci-test.c"],
        "mmc-partition": ["util/thead/tests/mmc-partition.c"],
    }.items():
        binary = work / name
        subprocess.run(["cc", "-std=gnu11", "-O2", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            *(["-DENV_RAMSTAGE=1"] if name == "sdhci" else []),
            "-include", str(work / "commonlib/helpers.h"), "-I"+str(work),
            "-I"+str(root / "src/commonlib/storage"),
            *[str(root / s) for s in sources], "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
