#!/usr/bin/env python3
"""Exercise SDL3's menu loans and conversion fallbacks on real renderers."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/sdl3_gfx.c").read_text()
common = (root / "gfx/common/sdl3_common.h").read_text()

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

def config(flag):
    return shlex.split(subprocess.check_output(["pkg-config", flag, "sdl3"], text=True))

with tempfile.TemporaryDirectory(prefix="sdl3-menu-memory-") as directory:
    temp = Path(directory)
    start = common.index("typedef struct sdl3_tex\n")
    end = common.index("} sdl3_tex_t;", start) + len("} sdl3_tex_t;")
    (temp / "texture.inc").write_text(common[start:end])
    start = common.index("enum sdl3_flags\n")
    (temp / "flags.inc").write_text(common[start:common.index("\n};", start) + 3])
    start = common.index("#define SDL3_MENU_MEM_MAX")
    (temp / "memory.inc").write_text(common[start:common.index("#endif", start)])
    end = common.index("} menu_lend;") + len("} menu_lend;")
    start = common.rindex("   struct\n   {\n", 0, end)
    (temp / "menu_lend.inc").write_text(common[start:end])
    (temp / "callbacks.inc").write_text("\n".join(function(name) for name in (
        "static void sdl3_set_frame_scale_mode(",
        "static bool sdl3_renderer_has_format(",
        "static INLINE void sdl3_tex_zero(",
        "static void sdl3_refresh_input_size(",
        "static void sdl3_stream_upload(",
        "static void sdl3_menu_lend_close(",
        "static void *sdl3_get_menu_framebuffer(",
        "static void *sdl3_menu_mem_alloc(",
        "static bool sdl3_menu_mem_ready(",
        "static void sdl3_menu_mem_free(",
        "static void sdl3_poke_set_texture_frame(")))
    exe = temp / ("menu.exe" if os.name == "nt" else "menu")
    flags = ["-std=c89", "-O1", "-g1", "-Wall", "-Wextra", "-DHAVE_THREADS",
             "-Werror=implicit-function-declaration", "-Werror=declaration-after-statement",
             "-I" + str(temp), "-I" + str(root),
             "-I" + str(root / "libretro-common/include")]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags + config("--cflags") + [
        str(Path(__file__).with_name("menu_mem_sdl3_test.c")),
        str(root / "libretro-common/memmap/memalign.c"),
        str(root / "libretro-common/gfx/scaler/pixconv.c"), "-o", str(exe)] + config("--libs"), check=True)
    subprocess.run([str(exe)], check=True)
