#!/usr/bin/env python3
"""Run real JNI admission contracts against synthetic Android classes."""
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
    raise SystemExit("A JDK with javac is required")
java_home = Path(os.environ.get("JAVA_HOME", Path(javac).resolve().parents[1]))
platform = "win32" if os.name == "nt" else "darwin" if sys.platform == "darwin" else "linux"
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = ["-O1", "-g1", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
         "-Wno-long-long", "-Wframe-larger-than=2048",
         "-I" + str(root / "libretro-common/include"),
         "-I" + str(java_home / "include"),
         "-I" + str(java_home / "include" / platform)]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
sanitizer = os.environ.get("SANITIZER", "")
if sanitizer:
    flags += ["-fsanitize=" + sanitizer, "-fno-omit-frame-pointer"]
env = os.environ.copy()
if os.name == "nt":
    libraries = [str(java_home / "lib/jvm.lib")]
    env["PATH"] = str(java_home / "bin/server") + os.pathsep + env["PATH"]
else:
    server = str(java_home / "lib/server")
    libraries = ["-L" + server, "-Wl,-rpath," + server, "-ljvm"]
with tempfile.TemporaryDirectory(prefix="mediacodec-inventory-") as directory:
    path = Path(directory)
    classes = path / "classes"
    subprocess.run([javac, "-d", str(classes)] +
                   [str(p) for p in sorted((here / "java").rglob("*.java"))], check=True)
    binary = path / ("inventory.exe" if os.name == "nt" else "inventory")
    command = compiler + flags + [str(here / "test.c"),
        str(root / "gfx/common/mediacodec_inventory.c"), "-o", str(binary)] + libraries
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], cwd=path, env=env, check=True, timeout=90)
