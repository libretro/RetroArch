#!/usr/bin/env python3
"""Run SDL2's shipping menu loan and upload callbacks on real renderers."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/sdl2_gfx.c").read_text()
common = (root / "gfx/common/sdl2_common.h").read_text()

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

def config(*flags):
    command = ["pkg-config", "sdl2"] if os.name == "nt" else ["sdl2-config"]
    result = shlex.split(subprocess.check_output(command + list(flags), text=True))
    # The oracle owns main() and its console; SDL_MAIN_HANDLED is explicit.
    return [flag for flag in result if flag not in
            ("-Dmain=SDL_main", "-mwindows", "-lSDL2main", "-lmingw32")]

with tempfile.TemporaryDirectory(prefix="sdl2-menu-memory-") as directory:
    temp = Path(directory)
    start = common.index("typedef struct sdl2_tex\n")
    end = common.index("} sdl2_tex_t;", start) + len("} sdl2_tex_t;")
    (temp / "texture.inc").write_text(common[start:end])
    start = common.index("#define SDL2_MENU_MEM_MAX")
    (temp / "memory.inc").write_text(common[start:common.index("#endif", start)])
    (temp / "callbacks.inc").write_text("\n".join(function(name) for name in (
        "static INLINE void sdl_tex_zero(",
        "static void sdl_refresh_input_size(",
        "static void *sdl2_menu_mem_alloc(",
        "static bool sdl2_menu_mem_ready(",
        "static void sdl2_menu_mem_free(",
        "static void sdl2_poke_set_texture_frame(")))
    exe = temp / ("menu.exe" if os.name == "nt" else "menu")
    flags = ["-std=c89", "-O1", "-g1", "-Wall", "-Wextra",
             "-Werror=implicit-function-declaration", "-Werror=declaration-after-statement",
             "-I" + str(temp), "-I" + str(root),
             "-I" + str(root / "libretro-common/include")]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags + config("--cflags") + [
        str(Path(__file__).with_name("menu_mem_sdl2_test.c")),
        str(root / "libretro-common/memmap/memalign.c"), "-o", str(exe)] + config("--libs"), check=True)
    subprocess.run([str(exe)], check=True)
