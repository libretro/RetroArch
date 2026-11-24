#!/usr/bin/env python3
"""Exercise legacy drivers' shipping cached-menu ownership callbacks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]

with tempfile.TemporaryDirectory(prefix="legacy-menu-memory-") as directory:
    temp = Path(directory)
    callbacks = []
    for filename, prefix in (("sdl_gfx.c", "sdl"), ("d3d8.c", "d3d8"),
                             ("d3d9hlsl.c", "d3d9_hlsl"), ("d3d9cg.c", "d3d9_cg")):
        source = (root / "gfx/drivers" / filename).read_text()
        signatures = ["static void *" + prefix + "_menu_mem_alloc(",
                      "static void " + prefix + "_menu_mem_free("]
        if prefix.startswith("d3d9"):
            signatures += ["static void " + prefix + "_frame_mem_free("]
        callbacks += [function(source, signature) for signature in signatures]
    (temp / "callbacks.inc").write_text("\n".join(callbacks))
    cc = shlex.split(os.environ.get("CC", "cc"))
    exe = temp / ("legacy-menu.exe" if os.name == "nt" or "mingw" in cc[0] else "legacy-menu")
    flags = ["-std=c89", "-O1", "-g1", "-Wall", "-Wextra", "-Werror", "-DHAVE_THREADS",
             "-I" + str(temp), "-I" + str(root), "-I" + str(root / "libretro-common/include")]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(cc + flags + [str(Path(__file__).with_name("legacy_menu_memory_test.c")),
        str(root / "libretro-common/memmap/memalign.c"), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
