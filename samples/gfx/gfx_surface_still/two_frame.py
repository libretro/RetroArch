#!/usr/bin/env python3
"""Shipping two-frame overlay submit, real surface code and simulated driver."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
source = (root / "input/input_driver.c").read_text(encoding="utf-8")
start = source.index("static bool input_overlay_update_apng_frame(")
function = source[start:source.index("\n}\n", start) + 3]
fixture = Path(__file__).with_name("gfx_surface_still_test.c").read_text(encoding="utf-8")
fixture = fixture.replace("int main(void)", "static int baseline_main(void)", 1)
fixture = fixture.replace("static uint32_t st_px_first, st_px_last;", "static uint32_t st_px_first, st_px_last;\nstatic const uint32_t *st_last_pixels;\nstatic bool st_seen_rgba;")
fixture = fixture.replace("      st_px_first = ti->pixels[0];", "      st_last_pixels = ti->pixels;\n      st_seen_rgba = ti->supports_rgba;\n      st_px_first = ti->pixels[0];", 1)
fixture += "\n" + Path(__file__).with_name("two_frame_test.inc").read_text(encoding="utf-8").replace("SHIPPING_FUNCTION", function)
sources = ["gfx/gfx_surface.c", "libretro-common/formats/image/image_yuv_blit.c", "gfx/gfx_instrument.c",
    "libretro-common/formats/image_texture_set.c", "libretro-common/compat/compat_strl.c",
    "libretro-common/rthreads/rthreads.c", "libretro-common/features/features_cpu.c", "libretro-common/time/rtime.c"]
with tempfile.TemporaryDirectory(prefix="retroarch-two-frame-") as directory:
    temp = Path(directory)
    src = temp / "test.c"
    src.write_text(fixture, encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    exe = temp / ("test.exe" if os.name == "nt" or "mingw" in cc[0] else "test")
    flags = ["-std=gnu99", "-O1", "-g1", "-Wall", "-Wextra", "-DHAVE_THREADS", "-DRARCH_INTERNAL",
        "-I" + str(root), "-I" + str(root / "libretro-common/include"), "-I" + str(root / "deps")]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(cc + flags + [str(src)] + [str(root / s) for s in sources] + ["-lm", "-lpthread", "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
