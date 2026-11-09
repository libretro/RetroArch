#!/usr/bin/env python3
"""Check the shipping Dingux and RS90 framebuffer lends on a stand-in surface."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def build(name, source, video_t, lane):
    driver = (root / source).read_text(encoding="utf-8")
    def function(signature):
        start = driver.index(signature)
        return driver[start:driver.index("\n}\n", start) + 3]
    with tempfile.TemporaryDirectory(prefix="retroarch-sw-lend-" + name) as d:
        temp = Path(d)
        (temp / "sw_lend.inc").write_text("\n".join(function(s) for s in (
            "static bool " + name + "_get_current_software_framebuffer(",
            "static bool " + name + "_sw_lend_take(")), encoding="utf-8")
        exe = temp / "sw_lend"
        subprocess.run(["cc", "-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra",
            "-Werror=declaration-after-statement",
            "-fsanitize=address,undefined",
            "-D" + video_t + "=VIDEO_T",
            "-DGET_FB=" + name + "_get_current_software_framebuffer",
            "-DTAKE=" + name + "_sw_lend_take",
            "-DLANE=\"" + name + "\"", "-D" + lane,
            "-I" + str(temp), "-I" + str(root / "libretro-common/include"),
            str(Path(__file__).with_name("sw_lend_dingux_test.c")),
            "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)

build("sdl_dingux", "gfx/drivers/sdl_dingux_gfx.c", "sdl_dingux_video_t", "DINGUX_LANE")
build("sdl_rs90", "gfx/drivers/sdl_rs90_gfx.c", "sdl_rs90_video_t", "RS90_LANE")
