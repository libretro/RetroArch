#!/usr/bin/env python3
"""Exercise production screenshot/recording slot selection without a GPU."""
from pathlib import Path
import argparse, os, shlex, subprocess, tempfile
here = Path(__file__).resolve().parent
root = here.parents[2]
p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, default=root / "gfx/drivers/d3d12.c")
a = p.parse_args()
s = a.source.read_text()
s = s[s.index("static bool d3d12_gfx_read_viewport("):]
start = s.index("   if (d3d12->record.enable")
s = s[start:s.index("   readback = d3d12->record.readback[slot];")]
with tempfile.TemporaryDirectory(prefix="d3d12-readback-slot-") as directory:
    tmp = Path(directory)
    (tmp / "selection.h").write_text("static unsigned select_slot(d3d12_video_t *d3d12, bool is_idle)\n{\n   unsigned slot;\n" + s + "   return slot;\n}\n")
    cc = shlex.split(os.environ.get("CC", "cc"))
    exe = tmp / ("slot.exe" if os.name == "nt" or "mingw" in cc[0] else "slot")
    flags = shlex.split(os.environ.get("CFLAGS", "-O1 -g"))
    subprocess.run(cc + flags + ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-I" + str(tmp), str(here / "slot_test.c"), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
