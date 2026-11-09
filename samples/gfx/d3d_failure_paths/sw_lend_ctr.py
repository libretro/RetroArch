#!/usr/bin/env python3
"""Check the shipping 3DS framebuffer lend on a stand-in linear heap."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/ctr_gfx.c").read_text(encoding="utf-8")

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

with tempfile.TemporaryDirectory(prefix="retroarch-ctr-sw-lend-") as directory:
    temp = Path(directory)
    (temp / "sw_lend.inc").write_text("\n".join(function(name) for name in (
        "static void ctr_sw_lend_release(",
        "static bool ctr_get_current_software_framebuffer(")), encoding="utf-8")
    exe = temp / "sw_lend"
    subprocess.run(["cc", "-std=gnu99", "-O1", "-g1", "-Wall", "-Wextra",
        "-fsanitize=address,undefined", "-I" + str(temp), "-I" + str(root),
        "-I" + str(root / "libretro-common/include"),
        str(Path(__file__).with_name("sw_lend_ctr_test.c")), "-o", str(exe)],
        check=True)
    subprocess.run([str(exe)], check=True)
