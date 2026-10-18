#!/usr/bin/env python3
"""Run the shipping lend publication against remaps on a real worker thread."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_thread_wrapper.c").read_text(encoding="utf-8")

def function(signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]

with tempfile.TemporaryDirectory(prefix="retroarch-lend-refresh-") as directory:
    temp = Path(directory)
    player = (root / "libretro-common/formats/video_player.c").read_text(encoding="utf-8")
    start = player.index("enum vp_job_status\n{")
    (temp / "states.inc").write_text(player[start:player.index("\n};", start) + 3], encoding="utf-8")
    start = player.index("static void vp_job_publish(")
    publish = player[start:player.index("\n}\n", start) + 3]
    publish = publish.replace("retro_atomic_thread_fence_seq_cst()", "counted_fence()")
    publish = publish.replace("retro_atomic_store_release_int(", "counted_store(")
    (temp / "publish.inc").write_text(publish, encoding="utf-8")
    (temp / "lend.inc").write_text("\n".join(function(name) for name in (
        "static int video_thread_lend_find(", "static void video_thread_lend_mark(",
        "static void video_thread_lend_slots(", "static void video_thread_lend_poll(",
        "static void video_thread_lend_forget(thread_video_t *thr, uintptr_t id)\n{",
        "bool video_thread_lend_get(")), encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    windows = os.name == "nt" or "mingw" in cc[0]
    exe = temp / ("lend.exe" if windows else "lend")
    flags = ["-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra", "-Werror=declaration-after-statement",
        "-DHAVE_THREADS", "-DRARCH_INTERNAL", "-I" + str(temp), "-I" + str(root),
        "-I" + str(root / "libretro-common/include"), "-I" + str(root / "deps")]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    sources = ["rthreads/rthreads.c", "features/features_cpu.c", "time/rtime.c"]
    subprocess.run(cc + flags + [str(Path(__file__).with_name("lend_refresh_test.c"))]
        + [str(root / "libretro-common" / s) for s in sources] + ["-o", str(exe)]
        + ([] if windows else ["-lpthread"]), check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
