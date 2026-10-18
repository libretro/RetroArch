#!/usr/bin/env python3
"""Check shipping D3D11 per-write loans on a real device."""
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
with tempfile.TemporaryDirectory(prefix="retroarch-d3d11-lend-gpu-") as directory:
    temp = Path(directory)
    (temp / "lend_type.inc").write_text(driver[start:end], encoding="utf-8")
    player = (root / "libretro-common/formats/video_player.c").read_text(encoding="utf-8")
    start = player.index("enum vp_job_status\n{")
    (temp / "states.inc").write_text(player[start:player.index("\n};", start) + 3], encoding="utf-8")
    start = player.index("static void vp_job_publish(")
    publish = player[start:player.index("\n}\n", start) + 3]
    (temp / "publish.inc").write_text(publish, encoding="utf-8")
    (temp / "lend.inc").write_text("\n".join(function(name) for name in (
        "static INLINE void d3d11_release_texture(", "static void d3d11_init_staging(",
        "static bool d3d11_texture_lend_map(", "static void *d3d11_gfx_texture_lend(",
        "static bool d3d11_gfx_texture_lend_ready(",
        "static enum video_texture_update d3d11_gfx_update_texture_internal(")), encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    windows = os.name == "nt" or "mingw" in cc[0]
    exe = temp / ("lend.exe" if windows else "lend")
    flags = ["-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra", "-Werror=declaration-after-statement"]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    libs = ["-ld3d11", "-ldxguid", "-luuid"] if windows else []
    subprocess.run(cc + flags + ["-I" + str(temp), "-I" + str(root / "libretro-common/include"),
        str(Path(__file__).with_name("lend_gpu_test.c")), "-o", str(exe)] + libs, check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
