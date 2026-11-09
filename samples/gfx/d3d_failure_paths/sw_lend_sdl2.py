#!/usr/bin/env python3
"""Check the shipping SDL2 software framebuffer lend on a real renderer."""
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/sdl2_gfx.c").read_text(encoding="utf-8")
common = (root / "gfx/common/sdl2_common.h").read_text(encoding="utf-8")

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

end = common.index("} sw_lend;") + len("} sw_lend;")
start = common.rindex("   struct\n   {\n", 0, end)

if not shutil.which("sdl2-config"):
    print("SKIP: no sdl2-config")
    raise SystemExit(0)

def config(*flags):
    return shlex.split(subprocess.check_output(["sdl2-config", *flags], text=True))

with tempfile.TemporaryDirectory(prefix="retroarch-sdl2-sw-lend-") as directory:
    temp = Path(directory)
    (temp / "sw_lend_type.inc").write_text(common[start:end] + "\n", encoding="utf-8")
    (temp / "sw_lend.inc").write_text("\n".join(function(name) for name in (
        "static INLINE void sdl_tex_zero(",
        "static void sdl_refresh_input_size(",
        "static bool sdl2_sw_lend_holds(",
        "static void sdl2_sw_lend_close(",
        "static bool sdl2_sw_lend_take(",
        "static bool sdl2_get_current_software_framebuffer(")), encoding="utf-8")
    exe = temp / "sw_lend_sdl2"
    subprocess.run(["cc", "-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra",
        "-Werror=declaration-after-statement", "-fsanitize=address,undefined",
        "-I" + str(temp), "-I" + str(root),
        "-I" + str(root / "libretro-common/include")] + config("--cflags") + [
        str(Path(__file__).with_name("sw_lend_sdl2_test.c")), "-o", str(exe)]
        + config("--libs"), check=True)
    subprocess.run([str(exe)], check=True, env={"SDL_VIDEODRIVER": "dummy"})
