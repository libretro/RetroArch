#!/usr/bin/env python3
"""Check Android Vulkan import resources or execute the RGB conversion oracle."""
import argparse
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--gpu", action="store_true", help="execute RGB shader pixels on an available Vulkan device")
parser.add_argument("--compile-shaders", action="store_true", help="test freshly compiled source shaders; requires glslc and spirv-val")
args = parser.parse_args()
if args.compile_shaders and not args.gpu:
    parser.error("--compile-shaders requires --gpu")
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = ["-O1", "-g1", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
         "-Wno-long-long", "-Wframe-larger-than=2048",
         "-I" + str(root / "libretro-common/include"),
         "-I" + str(root / "gfx/include")]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
sanitizer = os.environ.get("SANITIZER", "")
if sanitizer:
    flags += ["-fsanitize=" + sanitizer, "-fno-omit-frame-pointer"]
units = ("rgb_gpu",) if args.gpu else ("device", "image", "pipeline")
with tempfile.TemporaryDirectory(prefix="android-vulkan-") as directory:
    shader_flags = []
    if args.compile_shaders:
        source = root / "gfx/drivers/vulkan_shaders/external_rgb_to_rgba8888.comp"
        for mode, define in (("RGBA8", []), ("FP16", ["-DOUTPUT_FP16"])):
            spv = Path(directory) / (mode + ".spv")
            subprocess.run(shlex.split(os.environ.get("GLSLC", "glslc")) + define +
                           [str(source), "-o", str(spv)], check=True)
            subprocess.run(shlex.split(os.environ.get("SPIRV_VAL", "spirv-val")) +
                           [str(spv)], check=True)
            data = spv.read_bytes()
            words = struct.unpack("<" + "I" * (len(data) // 4), data)
            header = Path(directory) / (mode + ".inc")
            header.write_text("{" + ",".join(hex(w) for w in words) + "}\n",
                              encoding="utf-8", newline="\n")
            shader_flags += ['-DVULKAN_ANDROID_' + mode + '_SHADER="' + header.as_posix() + '"']
    for unit in units:
        name = "vulkan_" + unit + "_test"
        binary = Path(directory) / (name + (".exe" if os.name == "nt" else ""))
        source = "pipeline" if unit == "rgb_gpu" else unit
        command = compiler + flags + shader_flags + [str(here / (name + ".c")),
            str(root / "gfx/common" / ("vulkan_android_" + source + ".c")), "-o", str(binary)]
        if args.gpu:
            command += ["-lvulkan-1" if os.name == "nt" else "-lvulkan", "-lm"]
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=90)
