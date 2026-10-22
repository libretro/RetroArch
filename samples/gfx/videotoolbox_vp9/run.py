#!/usr/bin/env python3
"""Exercise shipping VP9 setup with mocks, or native CoreMedia formats only."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--native", action="store_true")
args = parser.parse_args()
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = ["-DHAVE_THREADS", "-O2", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
         "-Wno-long-long", "-Wframe-larger-than=2048", "-fstack-usage",
         "-I" + str(root / "libretro-common/include")]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
with tempfile.TemporaryDirectory(prefix="vt-vp9-setup-") as directory:
    path = Path(directory)
    libraries = []
    if args.native:
        flags += ["-DVT_VP9_NATIVE", "-mmacosx-version-min=10.14"]
        for framework in ("CoreMedia", "CoreVideo", "VideoToolbox", "CoreFoundation"):
            libraries += ["-framework", framework]
    else:
        for framework in ("CoreMedia", "CoreVideo", "VideoToolbox"):
            folder = path / framework
            folder.mkdir()
            (folder / (framework + ".h")).write_text(
                '#include "' + (here / "mock_apple.h").as_posix() + '"\n',
                encoding="utf-8")
        flags += ["-I" + str(path)]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    binary = path / ("setup.exe" if os.name == "nt" else "setup")
    thread_object = path / "rthreads.o"
    thread_command = compiler + ["-O2", "-DHAVE_THREADS",
        "-I" + str(root / "libretro-common/include")]
    if args.native:
        thread_command += ["-mmacosx-version-min=10.14"]
    thread_command += shlex.split(os.environ.get("CFLAGS", ""))
    thread_command += ["-c", str(root / "libretro-common/rthreads/rthreads.c"),
                      "-o", str(thread_object)]
    subprocess.run(thread_command, check=True)
    queue_object = path / "retro_spsc.o"
    queue_command = compiler + ["-O2", "-DHAVE_THREADS",
        "-I" + str(root / "libretro-common/include")]
    if args.native:
        queue_command += ["-mmacosx-version-min=10.14"]
    queue_command += shlex.split(os.environ.get("CFLAGS", ""))
    queue_command += ["-c", str(root / "libretro-common/queues/retro_spsc.c"),
                      "-o", str(queue_object)]
    subprocess.run(queue_command, check=True)
    if os.name != "nt":
        libraries += ["-pthread"]
    command = compiler + flags + [str(here / "setup_test.c"), str(thread_object), str(queue_object),
                                  "-o", str(binary)] + libraries
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)], check=True, timeout=90)

    if not args.native:
        command = compiler + flags + [str(here / "owner_test.c"), str(thread_object),
            str(queue_object), "-o", str(binary)] + libraries
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                       check=True, timeout=90)
