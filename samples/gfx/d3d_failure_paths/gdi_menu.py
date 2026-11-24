#!/usr/bin/env python3
"""Exercise shipping GDI menu loans, including real Windows DIB pixels."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/gdi_gfx.c").read_text()
header = (root / "gfx/common/gdi_defines.h").read_text()

def function(signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]

with tempfile.TemporaryDirectory(prefix="gdi-menu-") as directory:
    temp = Path(directory)
    start = header.index("   uint8_t *menu_frame;")
    (temp / "storage.inc").write_text(header[start:header.index("#endif", start) + 6])
    (temp / "callbacks.inc").write_text("\n".join(function(name) for name in (
        "static void *gdi_menu_mem_alloc(", "static bool gdi_menu_mem_ready(",
        "static void gdi_menu_mem_free(", "static void gdi_set_texture_frame(")))
    (temp / "pixels.inc").write_text("\n".join(function(name) for name in (
        "static bool gdi_ensure_scratch_rgui(", "static void gdi_blit_rgui_alpha(")))
    cc = shlex.split(os.environ.get("CC", "cc"))
    windows = os.name == "nt" or "mingw" in cc[0]
    flags = ["-std=c89", "-O1", "-g1", "-Wall", "-Wextra", "-DHAVE_THREADS",
             "-Werror=implicit-function-declaration", "-Werror=declaration-after-statement",
             "-I" + str(temp), "-I" + str(root / "libretro-common/include")]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    for name in (("gdi_menu_test", "gdi_menu_pixels") if windows else ("gdi_menu_test",)):
        exe = temp / (name + (".exe" if windows else ""))
        libs = ["-lgdi32", "-lmsimg32"] if name.endswith("pixels") else []
        subprocess.run(cc + flags + [str(Path(__file__).with_name(name + ".c")),
            str(root / "libretro-common/memmap/memalign.c"), "-o", str(exe)] + libs, check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
