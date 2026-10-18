#!/usr/bin/env python3
"""Exercise shipping D3D11 per-write maps with changing addresses and pitches."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/d3d11.c").read_text(encoding="utf-8")

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

end = driver.index("} d3d11_texture_lend_t;") + len("} d3d11_texture_lend_t;")
start = driver.rindex("typedef struct", 0, end)
with tempfile.TemporaryDirectory(prefix="retroarch-d3d11-lend-") as directory:
    temp = Path(directory)
    (temp / "lend_type.inc").write_text(driver[start:end], encoding="utf-8")
    (temp / "lend.inc").write_text("\n".join(function(name) for name in (
        "static INLINE void d3d11_release_texture(",
        "static bool d3d11_texture_lend_map(",
        "static void *d3d11_gfx_texture_lend(",
        "static bool d3d11_gfx_texture_lend_ready(",
        "static enum video_texture_update d3d11_gfx_update_texture_internal(")), encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    exe = temp / ("lend.exe" if os.name == "nt" or "mingw" in cc[0] else "lend")
    flags = ["-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra", "-Werror=declaration-after-statement"]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(cc + flags + ["-I" + str(temp), "-I" + str(root / "libretro-common/include"),
        str(Path(__file__).with_name("lend_test.c")), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
