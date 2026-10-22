#!/usr/bin/env python3
"""Exercise Android Vulkan FD ownership and warm submission/cache contracts."""
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
compiler = shlex.split(os.environ.get("CC", "cc"))
platform = "win32" if os.name == "nt" else "darwin" if sys.platform == "darwin" else "linux"
flags = ["-O1", "-g1", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
         "-Wno-long-long", "-Wframe-larger-than=2048", "-DHAVE_THREADS",
         "-I" + str(root / "libretro-common/include"), "-I" + str(root / "gfx/include"),
         "-I" + str(java_home / "include"), "-I" + str(java_home / "include" / platform)]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
sanitizer = os.environ.get("SANITIZER", "")
if sanitizer:
    flags += ["-fsanitize=" + sanitizer, "-fno-omit-frame-pointer"]
with tempfile.TemporaryDirectory(prefix="android-vulkan-warm-") as directory:
    helper = []
    if os.name == "nt":
        obj = Path(directory) / "fd_test.o"
        subprocess.run(compiler + flags + ["-c", str(here / "win32_fd_test.c"),
                       "-o", str(obj)], check=True)
        helper = [str(obj)]
        flags += ["-include", str(here / "win32_fd_test.h"), "-I" + str(here / "win32_fd_headers")]
        print("Windows CRT pipe ownership model; no native SYNC_FD device import", flush=True)
    common = ["vulkan_android_submit.c", "vulkan_android_image.c", "vulkan_android_sync.c",
              "vulkan_android_pipeline.c", "android_frame.c", "android_image_queue.c"]
    for name in ("sync", "submit", "entry", "cache"):
        sources = ["vulkan_android_sync.c"] if name == "sync" else list(common)
        if name in ("entry", "cache"):
            sources += ["vulkan_android_cache_entry.c"]
        if name == "cache":
            sources += ["vulkan_android_cache.c"]
        sources = [str(root / "gfx/common" / p) for p in sources]
        if name != "sync":
            sources += [str(root / "libretro-common/formats/image/image_yuv_blit.c")]
        binary = Path(directory) / (name + (".exe" if os.name == "nt" else ""))
        command = compiler + flags + [str(here / ("vulkan_" + name + "_test.c"))] + sources + helper + ["-o", str(binary), "-lm"]
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=90)
