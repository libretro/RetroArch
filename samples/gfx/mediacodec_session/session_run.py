#!/usr/bin/env python3
"""Exercise MediaCodec worker-session contracts without Android hardware."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
javac = shutil.which("javac")
if not javac:
    raise SystemExit("A JDK with JNI headers is required")
java_home = Path(os.environ.get("JAVA_HOME", Path(javac).resolve().parents[1]))
platform = "win32" if os.name == "nt" else "darwin" if sys.platform == "darwin" else "linux"
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = ["-O1", "-g1", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
         "-Wno-long-long", "-Wframe-larger-than=2048", "-DHAVE_DYLIB",
         "-DMEDIACODEC_SESSION_TEST", "-I" + str(here),
         "-I" + str(root / "libretro-common/include"),
         "-I" + str(java_home / "include"),
         "-I" + str(java_home / "include" / platform)]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
sanitizer = os.environ.get("SANITIZER", "")
if sanitizer:
    flags += ["-fsanitize=" + sanitizer, "-fno-omit-frame-pointer"]
with tempfile.TemporaryDirectory(prefix="mediacodec-session-") as directory:
    binary = Path(directory) / ("session.exe" if os.name == "nt" else "session")
    command = compiler + flags + [str(here / "test.c"),
        str(root / "gfx/common/mediacodec_session.c"), "-o", str(binary)]
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True, timeout=90)
