#!/usr/bin/env python3
"""Exercise the shipping metadata provider and DXGI submission with CPU mocks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_driver.c").read_text(encoding="utf-8")
start = source.index("static retro_atomic_int_t video_hdr_output_mode;")
end = source.index("void video_driver_set_display_peak_nits(", start)
dxgi = (root / "gfx/common/dxgi_common.c").read_text(encoding="utf-8")
dxgi = dxgi[dxgi.index("void dxgi_swapchain_color_space("):]
dxgi = dxgi[:dxgi.index("/* ------------------------------------------------------------------")]
vulkan = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")


def vulkan_block(marker, semicolon=False):
    start = vulkan.index(marker)
    brace = vulkan.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (vulkan[end] == "{") - (vulkan[end] == "}")
        end += 1
    return vulkan[start:end + int(semicolon)] + "\n"


macro = vulkan.index("#define VULKAN_BUILT_HDR_FLAGS ")
vulkan_code = vulkan[macro:vulkan.index("\n\n", macro)] + "\n"
vulkan_code += vulkan_block("enum vulkan_texture_support", True)
vulkan_code += vulkan_block("static void vulkan_note_built(")
vulkan_code += vulkan_block("static bool vulkan_gpu_format_to_vk(")
vulkan_code += vulkan_block("static bool vulkan_supports_texture_format(")

with tempfile.TemporaryDirectory(prefix="retroarch-hdr-query-") as directory:
    directory = Path(directory)
    (directory / "provider.inc").write_text(source[start:end], encoding="utf-8")
    (directory / "dxgi.inc").write_text(dxgi, encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    libs = [] if os.name == "nt" else ["-pthread"]
    exe = directory / ("provider.exe" if os.name == "nt" else "provider")
    subprocess.run(cc + ["-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra",
                        "-Werror=declaration-after-statement",
                        "-Werror=implicit-function-declaration", "-DHAVE_THREADS",
                        "-I" + str(root / "libretro-common/include"),
                        "-I" + str(directory)] + flags +
                   [str(Path(__file__).with_name("provider_test.c")),
                    str(root / "libretro-common/rthreads/rthreads.c"),
                    "-o", str(exe)] + libs, check=True)
    subprocess.run([str(exe)], check=True)
    (directory / "vulkan.inc").write_text(vulkan_code, encoding="utf-8")
    for hdr in (False, True):
        target = directory / ("vulkan-hdr" if hdr else "vulkan-sdr")
        if os.name == "nt":
            target = target.with_suffix(".exe")
        defines = ["-DVULKAN_HDR_SWAPCHAIN"] if hdr else []
        subprocess.run(cc + ["-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra",
                            "-Werror=declaration-after-statement",
                            "-Werror=implicit-function-declaration", "-DHAVE_THREADS",
                            "-I" + str(root / "libretro-common/include"),
                            "-I" + str(root / "gfx/include"),
                            "-I" + str(directory)] + defines + flags +
                       [str(Path(__file__).with_name("vulkan_caps_test.c")),
                        str(root / "libretro-common/rthreads/rthreads.c"),
                        "-o", str(target)] + libs, check=True)
        subprocess.run([str(target)], check=True)
