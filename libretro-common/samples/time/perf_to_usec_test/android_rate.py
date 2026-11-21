#!/usr/bin/env python3
"""Android keeps valid CNTFRQ rates fixed and measures invalid ones."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path,
                    default=root / "features/features_cpu.c")
args = parser.parse_args()
source = args.source.read_text(encoding="utf-8")
start = source.index("static int cpu_features_perf_rate_stated(void)")
end = source.index("#endif /* RETRO_DISABLE_HOST_CLOCK */", start)
source = source[start:end]
# Compile the shipping ARM64 branch on the host. Only the register access
# is substituted; the rate selection, calibration and conversions are real.
for arch in ("__x86_64__", "__i386__", "_M_X64", "_M_IX86"):
    source = source.replace(arch, "TEST_UNUSED_" + arch)
source = source.replace(
    '__asm__ __volatile__ ("mrs %0, cntfrq_el0" : "=r" (hz));',
    'hz = read_frequency();')
fixture = r'''
#include <stdio.h>
#include <stdlib.h>
#include <features/features_cpu.h>
#include <retro_atomic.h>
#define ANDROID 1
#define __aarch64__ 1
static retro_perf_tick_t test_ticks;
static unsigned register_reads;
static unsigned counter_reads;
static unsigned reported_frequency;
static unsigned read_frequency(void)
{
   register_reads++;
   return reported_frequency;
}
retro_perf_tick_t cpu_features_get_perf_counter(void)
{
   counter_reads++;
   return test_ticks;
}
SHIPPING
int main(int argc, char **argv)
{
   retro_time_t span;
   unsigned actual_frequency, reads;
   retro_perf_tick_t five_ms;
   if (argc != 3) return 2;
   reported_frequency = (unsigned)strtoul(argv[1], NULL, 10);
   actual_frequency = (unsigned)strtoul(argv[2], NULL, 10);
   five_ms = actual_frequency / 200;
   span = cpu_features_perf_to_usec(five_ms);
   if (span != 0 || register_reads != 0)
   {
      printf("FAIL: firmware rate %u was trusted before calibration (%lld us)\n",
            reported_frequency, (long long)span);
      return 3;
   }
   cpu_features_perf_calibrate(1000000);
   test_ticks = five_ms;
   cpu_features_perf_calibrate(1005000);
   span = cpu_features_perf_to_usec(five_ms);
   if (span != 5000 || cpu_features_perf_from_usec(5000) != five_ms
         || register_reads > 1)
   {
      printf("FAIL: reported %u Hz, measured span %lld us, %u register reads\n",
            reported_frequency, (long long)span, register_reads);
      return 1;
   }
   test_ticks = actual_frequency / 50;
   cpu_features_perf_calibrate(1020000);
   reads = counter_reads;
   test_ticks = actual_frequency / 25;
   cpu_features_perf_calibrate(1040000);
   test_ticks = actual_frequency;
   cpu_features_perf_calibrate(2000000);
   if (cpu_features_perf_to_usec(five_ms) != 5000 || register_reads > 1)
      return 4;
   if (reported_frequency == actual_frequency && counter_reads != reads)
   {
      puts("FAIL: valid firmware frequency still incurs calibration reads");
      return 5;
   }
   printf("PASS: reported %u Hz, actual %u Hz, %s\n", reported_frequency,
         actual_frequency, reported_frequency == actual_frequency
         ? "no further calibration reads" : "measured fallback");
   return 0;
}
'''.replace("SHIPPING", source)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
runner = shlex.split(os.environ.get("RUNNER", ""))
with tempfile.TemporaryDirectory(prefix="android-counter-") as directory:
    src = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if "mingw" in cc[0] else "test")
    src.write_text(fixture, encoding="utf-8")
    subprocess.run(cc + flags + ["-std=c89", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unused-function", "-I" + str(root / "include"),
                   str(src), "-o", str(exe)], check=True)
    failures = 0
    for reported, actual in ((0, 25600000), (25600000, 25600000),
                             (19200000, 19200000), (1465000000, 25600000)):
        failures += subprocess.run(runner + [str(exe), str(reported),
                                   str(actual)]).returncode != 0
    if failures:
        raise SystemExit("Android counter regression: %d cases failed" % failures)
