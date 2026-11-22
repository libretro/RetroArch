#!/usr/bin/env python3
"""Run real headless OpenGL pixels through the shared viewer history chain."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix="gl-history-pixels-") as directory:
    exe = Path(directory) / "pixels"
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    command += shlex.split(os.environ.get("CFLAGS", ""))
    command += ["-I" + str(root), "-I" + str(root / "libretro-common/include"),
                str(here / "history_pixels.c"), "-o", str(exe), "-lEGL", "-lGL", "-lm"]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True)
