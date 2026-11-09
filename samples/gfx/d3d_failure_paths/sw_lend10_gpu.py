#!/usr/bin/env python3
"""Check the shipping D3D10 software framebuffer lend on a real device."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/d3d10.c").read_text(encoding="utf-8")

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

# The lend's state, as d3d10_video_t has it
end = driver.index("} sw_lend;") + len("} sw_lend;")
start = driver.rindex("   struct\n   {\n", 0, end)
slots = driver[driver.index("#define D3D10_SW_LEND_SLOTS"):].split("\n", 1)[0]

with tempfile.TemporaryDirectory(prefix="retroarch-d3d10-sw-lend-") as directory:
    temp = Path(directory)
    (temp / "sw_lend_type.inc").write_text(slots + "\n" + driver[start:end] + "\n",
        encoding="utf-8")
    (temp / "sw_lend.inc").write_text("\n".join(function(name) for name in (
        "static void d3d10_sw_lend_release(d3d10_video_t *d3d10)\n{",
        "static bool d3d10_get_current_software_framebuffer(",
        "static int d3d10_sw_lend_find(",
        "static bool d3d10_sw_lend_push(")), encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    windows = os.name == "nt" or "mingw" in cc[0]
    exe = temp / ("sw_lend.exe" if windows else "sw_lend")
    flags = ["-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra",
        "-Werror=declaration-after-statement"]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    libs = ["-ld3d10", "-ldxguid", "-luuid"] if windows else []
    subprocess.run(cc + flags + ["-I" + str(temp), "-I" + str(root),
        "-I" + str(root / "libretro-common/include"),
        str(Path(__file__).with_name("sw_lend10_gpu_test.c")), "-o", str(exe)] + libs,
        check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
