#!/usr/bin/env python3
"""Exercise worker-owned Android encoder contracts with native API mocks."""
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
         "-DRECORD_MEDIACODEC_TEST", "-I" + str(here),
         "-I" + str(root / "libretro-common/include"),
         "-I" + str(java_home / "include"), "-I" + str(java_home / "include" / platform)]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
if os.environ.get("SANITIZER"):
    flags += ["-fsanitize=" + os.environ["SANITIZER"], "-fno-omit-frame-pointer"]
with tempfile.TemporaryDirectory(prefix="mediacodec-encode-") as directory:
    binary = Path(directory) / ("encoder_test.exe" if os.name == "nt" else "encoder_test")
    for source, extra in (("test.c", []), ("audio_test.c", []),
                          ("mux_test.c", ["-DRECORD_MEDIACODEC_MUX_TEST"]),
                          ("worker_test.c", [])):
        command = compiler + flags + extra + [str(here / source), "-o", str(binary)]
        if source == "worker_test.c":
            command += ["-DHAVE_THREADS",
                        str(root / "record/record_mediacodec_worker.c"),
                        str(root / "record/record_audio_queue.c"),
                        str(root / "libretro-common/queues/retro_spsc.c")]
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=30)
