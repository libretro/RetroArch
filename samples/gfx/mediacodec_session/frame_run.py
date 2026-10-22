#!/usr/bin/env python3
"""Exercise opaque Android frame leases and HDR refinement on host CPUs."""
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
         "-DHAVE_THREADS", "-I" + str(here),
         "-I" + str(root / "libretro-common/include"),
         "-I" + str(java_home / "include"),
         "-I" + str(java_home / "include" / platform)]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
sanitizer = os.environ.get("SANITIZER", "")
if sanitizer:
    flags += ["-fsanitize=" + sanitizer, "-fno-omit-frame-pointer"]
if os.name == "nt":
    flags += ["-DANDROID_IMAGE_QUEUE_TEST_NO_FENCES"]
else:
    flags += ["-pthread"]
with tempfile.TemporaryDirectory(prefix="android-frame-") as directory:
    path = Path(directory)
    thread = path / "rthreads.o"
    thread_flags = ["-O1", "-g1", "-DHAVE_THREADS", "-D_POSIX_C_SOURCE=200809L",
                    "-I" + str(root / "libretro-common/include")]
    if sanitizer:
        thread_flags += ["-fsanitize=" + sanitizer, "-fno-omit-frame-pointer"]
    subprocess.run(compiler + thread_flags + ["-c",
        str(root / "libretro-common/rthreads/rthreads.c"), "-o", str(thread)], check=True)
    for name in ("android_frame_test", "color_refine_test"):
        binary = path / (name + (".exe" if os.name == "nt" else ""))
        sources = [str(here / (name + ".c"))]
        if name == "android_frame_test":
            sources += [str(root / "gfx/common/android_frame.c"),
                        str(root / "gfx/common/android_image_queue.c"), str(thread)]
        command = compiler + flags + sources + ["-o", str(binary)]
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                       check=True, timeout=90)
