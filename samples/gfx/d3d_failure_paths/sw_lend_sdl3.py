#!/usr/bin/env python3
"""Check the shipping SDL3 software framebuffer lend against a stand-in for SDL."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
driver = (root / "gfx/drivers/sdl3_gfx.c").read_text(encoding="utf-8")
common = (root / "gfx/common/sdl3_common.h").read_text(encoding="utf-8")

def function(signature):
    start = driver.index(signature)
    return driver[start:driver.index("\n}\n", start) + 3]

end = common.index("} sw_lend;") + len("} sw_lend;")
start = common.rindex("   struct\n   {\n", 0, end)

with tempfile.TemporaryDirectory(prefix="retroarch-sdl3-sw-lend-") as directory:
    temp = Path(directory)
    (temp / "sw_lend_type.inc").write_text(common[start:end] + "\n", encoding="utf-8")
    (temp / "sw_lend.inc").write_text("\n".join(function(name) for name in (
        "static INLINE void sdl3_tex_zero(",
        "static void sdl3_refresh_input_size(",
        "static bool sdl3_sw_lend_holds(",
        "static void sdl3_sw_lend_close(",
        "static bool sdl3_sw_lend_take(",
        "static bool sdl3_get_current_software_framebuffer(")), encoding="utf-8")
    exe = temp / "sw_lend_sdl3"
    subprocess.run(["cc", "-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra",
        "-Werror=declaration-after-statement", "-fsanitize=address,undefined",
        "-I" + str(temp), "-I" + str(root),
        "-I" + str(root / "libretro-common/include")] + [
        str(Path(__file__).with_name("sw_lend_sdl3_test.c")), "-o", str(exe)],
        check=True)
    subprocess.run([str(exe)], check=True)
