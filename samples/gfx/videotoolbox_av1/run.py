#!/usr/bin/env python3
"""Exercise AV1 cold setup ownership with portable Apple API mocks."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
compiler = shlex.split(os.environ.get("CC", "cc"))
parser = argparse.ArgumentParser()
parser.add_argument("--native-format", action="store_true")
parser.add_argument("--native-pixels", action="store_true")
parser.add_argument("--pipeline", action="store_true")
args = parser.parse_args()
if args.pipeline:
    args.native_pixels = True
if args.native_pixels:
    args.native_format = True
flags = ["-DHAVE_THREADS", "-O2", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
         "-Wno-long-long", "-Wframe-larger-than=2048",
         "-I" + str(root / "libretro-common/include")]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="vt-av1-setup-") as directory:
    path = Path(directory)
    libraries = []
    source = here / "setup_test.c"
    if args.native_format:
        source = here / "native_format_test.c"
        flags += ["-mmacosx-version-min=10.14"]
        for framework in ("CoreMedia", "CoreVideo", "VideoToolbox", "CoreFoundation"):
            libraries += ["-framework", framework]
    else:
        for framework in ("CoreMedia", "CoreVideo", "VideoToolbox"):
            folder = path / framework
            folder.mkdir()
            (folder / (framework + ".h")).write_text(
                '#include "' + (here.parent / "videotoolbox_vp9/mock_apple.h").as_posix()
                + '"\n', encoding="utf-8")
        folder = path / "dispatch"
        folder.mkdir()
        (folder / "dispatch.h").write_text(
            '#include "' + (here.parent / "videotoolbox_vp9/mock_dispatch.h").as_posix()
            + '"\n', encoding="utf-8")
    binary = path / ("setup.exe" if os.name == "nt" else "setup")
    command = compiler + flags + ["-I" + str(path), str(source),
                                  "-o", str(binary)] + libraries
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                   check=True, timeout=30)
    if args.native_pixels:
        objects = []
        for name, relative in (("rthreads", "rthreads/rthreads.c"),
                               ("retro_spsc", "queues/retro_spsc.c"),
                               ("features_cpu", "features/features_cpu.c")):
            obj = path / (name + ".o")
            command = compiler + ["-O2", "-DHAVE_THREADS", "-mmacosx-version-min=10.14",
                "-I" + str(root / "libretro-common/include"), "-c",
                str(root / "libretro-common" / relative), "-o", str(obj)]
            subprocess.run(command, check=True)
            objects += [str(obj)]
        command = compiler + flags + [str(here / "native_pixel_test.c")]
        command += objects + ["-o", str(binary)] + libraries
        subprocess.run(command, check=True)
        result = subprocess.run([sys.executable, str(here / "native_run.py"),
                                 "--binary", str(binary)], check=False)
        if not result.returncode and args.pipeline:
            import importlib.util
            specification = importlib.util.spec_from_file_location(
                "hevc_metal_fixture", here.parent / "videotoolbox_hevc/run.py")
            helper = importlib.util.module_from_spec(specification)
            specification.loader.exec_module(helper)
            unit = path / "av1_metal.m"
            unit.write_text(helper.metal_test_source() + '\n#include "'
                + (here / "native_metal.m").as_posix() + '"\n', encoding="utf-8")
            metal_object = path / "av1_metal.o"
            command = compiler + ["-DHAVE_THREADS", "-DHAVE_RAV1", "-O2",
                "-fobjc-arc", "-fblocks", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-function", "-Wframe-larger-than=4096",
                "-mmacosx-version-min=10.14", "-I" + str(root / "libretro-common/include"),
                "-c", str(unit), "-o", str(metal_object)]
            subprocess.run(command, check=True)
            for name, relative in (("av1_hw", "av1/rav1_hw.c"),
                                   ("rh265", "h265/rh265.c")):
                obj = path / (name + ".o")
                command = compiler + ["-O2", "-DHAVE_THREADS", "-Wno-unused-parameter",
                    "-mmacosx-version-min=10.14", "-I" + str(root / "libretro-common/include"),
                    "-c", str(root / "libretro-common/formats" / relative), "-o", str(obj)]
                subprocess.run(command, check=True)
                objects += [str(obj)]
            command = compiler + flags + ["-DVT_AV1_METAL_TEST", "-Wno-unused-function",
                str(here / "native_pixel_test.c"), *objects, str(metal_object),
                "-o", str(binary), "-framework", "Foundation", "-framework", "Metal", "-lm"] + libraries
            subprocess.run(command, check=True)
            result = subprocess.run([sys.executable, str(here / "native_run.py"),
                                     "--binary", str(binary), "--metal"], check=False)
        raise SystemExit(result.returncode)
    if not args.native_format:
        command = compiler + flags + ["-I" + str(path), str(here / "sample_test.c"),
                                      "-o", str(binary)]
        subprocess.run(command, check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                       check=True, timeout=30)
        objects = []
        for name, relative in (("rthreads", "rthreads/rthreads.c"),
                               ("retro_spsc", "queues/retro_spsc.c")):
            obj = path / (name + ".o")
            command = compiler + ["-O2", "-DHAVE_THREADS",
                "-I" + str(root / "libretro-common/include")]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += ["-c", str(root / "libretro-common" / relative), "-o", str(obj)]
            subprocess.run(command, check=True)
            objects += [str(obj)]
        for name in ("owner_test.c", "decoder_test.c", "dispatch_test.c", "backend_test.c"):
            command = compiler + flags + ["-I" + str(path), str(here / name)]
            command += objects + ["-o", str(binary)]
            if os.name != "nt":
                command += ["-pthread"]
            subprocess.run(command, check=True)
            subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                           check=True, timeout=90)
