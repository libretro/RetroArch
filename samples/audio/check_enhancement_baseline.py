#!/usr/bin/env python3
"""Compare both enhancement foundations against an unmodified Git revision."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--base", required=True, help="unmodified RetroArch revision")
parser.add_argument("--cc", default="cc")
parser.add_argument("--cflags", default="-O2 -std=c89")
parser.add_argument("--work-dir", default=".", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
common = root / "libretro-common"
flags = shlex.split(args.cflags)


def run(command):
    subprocess.run([str(x) for x in command], cwd=root, check=True)


with tempfile.TemporaryDirectory(prefix="audio-baseline-", dir=args.work_dir) as tmp:
    tmp = Path(tmp).resolve()
    original = [
        ("wsolapitchtempo.c", "audio/dsp_filters/wsolapitchtempo.c",
         ["dspfilter_get_implementation=reference_implementation"]),
        ("sinc.c", "audio/resampler/drivers/sinc_resampler.c",
         ["sinc_resampler=reference_sinc",
          "sinc_resampler_init_hq=reference_f_init_hq"]),
        ("sinc_i.c", "audio/resampler/drivers/sinc_resampler_int16.c", [
            "sinc_resampler_int16_init=reference_i_init",
            "sinc_resampler_int16_init_hq=reference_i_init_hq",
            "sinc_resampler_int16_process=reference_i_process",
            "sinc_resampler_int16_free=reference_i_free"]),
    ]
    hq_reference = False
    for name, source, defines in original:
        contents = subprocess.check_output(
            ["git", "show", args.base + ":libretro-common/" + source], cwd=root)
        if name == "sinc_i.c":
            hq_reference = b"sinc_resampler_int16_init_hq(" in contents
        (tmp / name).write_bytes(contents)
        run([args.cc, *flags, "-I" + str(common / "include"),
             *("-D" + d for d in defines), "-c", tmp / name,
             "-o", tmp / (name + ".o")])
    jobs = [
        ("wsola", "WSOLA_REFERENCE", [
            root / "samples/audio/wsola_search/wsola_search_test.c",
            common / "audio/dsp_filters/wsolapitchtempo.c",
            tmp / "wsolapitchtempo.c.o"]),
        ("sinc_hq", "SINC_REFERENCE", [
            root / "samples/audio/sinc_hq/sinc_hq_test.c",
            common / "audio/resampler/drivers/sinc_resampler.c",
            common / "audio/resampler/drivers/sinc_resampler_int16.c",
            common / "memmap/memalign.c", tmp / "sinc.c.o", tmp / "sinc_i.c.o"]),
    ]
    for name, define, sources in jobs:
        executable = tmp / (name + ".exe")
        run([args.cc, *flags, "-I" + str(common / "include"), "-D" + define,
             *(["-DSINC_REFERENCE_HQ"] if hq_reference else []),
             *sources, "-lm", "-o", executable])
        run([executable])
