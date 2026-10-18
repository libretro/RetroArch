#!/usr/bin/env python3
"""Real gfx_surface ownership with expiring upload maps and simulated completions."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
fixture = Path(__file__).with_name("gfx_surface_still_test.c").read_text(encoding="utf-8")
fixture = fixture.replace("int main(void)", "static int baseline_main(void)", 1)
fixture = fixture.replace("static bool st_lend, st_fp16;", "static bool st_lend, st_fp16;\nstatic bool st_refresh;")
fixture = fixture.replace("void *video_driver_texture_lend(", "static void *persistent_lend(", 1)
fixture = fixture.replace("enum video_texture_update video_driver_texture_update(",
                          "static enum video_texture_update persistent_update(", 1)
fixture = fixture.replace("bool video_driver_texture_lend_needs_refresh(void) { return false; }",
                          "bool video_driver_texture_lend_needs_refresh(void) { return st_refresh; }")
start = fixture.index("bool video_thread_lend_get(")
end = fixture.index("\n}\n", start) + 3
fixture = fixture[:start] + fixture[end:]
fixture = fixture.replace("      n->img = NULL;", "      n->lend_refresh = st_refresh ? n->lend : 0;\n      n->img = NULL;", 1)
fixture += "\n" + Path(__file__).with_name("remap_test.inc").read_text(encoding="utf-8")
sources = ["gfx/gfx_surface.c", "libretro-common/formats/image/image_yuv_blit.c", "gfx/gfx_instrument.c",
    "libretro-common/formats/image_texture_set.c", "libretro-common/compat/compat_strl.c",
    "libretro-common/rthreads/rthreads.c", "libretro-common/features/features_cpu.c", "libretro-common/time/rtime.c"]
with tempfile.TemporaryDirectory(prefix="retroarch-surface-remap-") as directory:
    temp = Path(directory)
    src = temp / "test.c"
    src.write_text(fixture, encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    exe = temp / ("test.exe" if os.name == "nt" or "mingw" in cc[0] else "test")
    flags = ["-std=gnu99", "-O1", "-g1", "-Wall", "-Wextra", "-DHAVE_THREADS", "-DRARCH_INTERNAL",
        "-I" + str(root), "-I" + str(root / "libretro-common/include"), "-I" + str(root / "deps")]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(cc + flags + [str(src)] + [str(root / s) for s in sources]
        + ["-lm", "-lpthread", "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
