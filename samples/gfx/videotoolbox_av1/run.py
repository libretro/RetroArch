#!/usr/bin/env python3
"""Exercise AV1 cold setup ownership with portable Apple API mocks."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
compiler = shlex.split(os.environ.get("CC", "cc"))
parser = argparse.ArgumentParser()
parser.add_argument("--native-format", action="store_true")
args = parser.parse_args()
flags = ["-O2", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
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
    binary = path / ("setup.exe" if os.name == "nt" else "setup")
    command = compiler + flags + ["-I" + str(path), str(source),
                                  "-o", str(binary)] + libraries
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                   check=True, timeout=30)
