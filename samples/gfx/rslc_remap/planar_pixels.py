#!/usr/bin/env python3
"""Run the compiled planar sampler on a real Vulkan device."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
cc = shlex.split(os.environ.get("CC", "cc"))
windows = os.name == "nt" or "mingw" in " ".join(cc)
with tempfile.TemporaryDirectory(prefix="slang-planar-") as directory:
    exe = Path(directory) / ("pixels.exe" if windows else "pixels")
    args = cc + [
        "-std=c89", "-D_POSIX_C_SOURCE=200112L", "-pedantic", "-Wall", "-Wextra",
        "-Werror=implicit-function-declaration", "-O2", "-g", "-DRSLC_EXTENDED",
        "-Wno-long-long", "-Wno-overlength-strings",
        "-I" + str(root / "libretro-common/include"), "-I" + str(root / "gfx/include")]
    args += shlex.split(os.environ.get("CFLAGS", ""))
    args += [str(Path(__file__).with_suffix(".c")),
             str(root / "gfx/common/vulkan_android_pipeline.c"),
             str(root / "libretro-common/formats/image/image_yuv_blit.c")]
    args += [str(root / "libretro-common/gfx/shaders" / name) for name in (
        "rslc_front.c", "rslc_ir.c", "rslc_emit.c", "rslc_dxbc.c")]
    args += ["-o", str(exe), "-lvulkan-1" if windows else "-lvulkan", "-lm"]
    subprocess.run(args, check=True)
    result = subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)] +
                            sys.argv[1:], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    print(result.stdout, end="")
    result.check_returncode()
    if "Validation Error" in result.stdout or "VUID-" in result.stdout:
        raise AssertionError("Vulkan validation reported a sampler error")
