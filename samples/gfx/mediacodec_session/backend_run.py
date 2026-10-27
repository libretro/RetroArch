#!/usr/bin/env python3
"""Exercise Android decoder admission, bounded service and deferred retirement."""
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
         "-DMEDIACODEC_SESSION_TEST", "-D_DEFAULT_SOURCE",
         "-I" + str(here), "-I" + str(root / "libretro-common/include"),
         "-I" + str(root / "gfx/include"), "-I" + str(java_home / "include"),
         "-I" + str(java_home / "include" / platform)]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
if os.name == "nt":
    flags += ["-DANDROID_IMAGE_QUEUE_TEST_NO_FENCES", "-Wno-unused-parameter"]
flags += shlex.split(os.environ.get("CFLAGS", ""))
sanitizer = os.environ.get("SANITIZER", "")
if sanitizer:
    flags += ["-fsanitize=" + sanitizer, "-fno-omit-frame-pointer"]
common = root / "gfx/common"
export = [common / name for name in ("mediacodec_frame_export.c", "mediacodec_color.c",
          "android_frame.c", "android_image_queue.c")]
threads = [root / "libretro-common" / name for name in ("rthreads/rthreads.c",
           "rthreads/retro_eventcount.c", "time/rtime.c")]
backends = [common / ("mediacodec_" + codec + "_backend.c") for codec in ("hevc", "vp9", "av1")]
cases = [
    ("backend_nothreads_test", "backend_disabled_test.c", backends, []),
    ("backend_volatile_test", "backend_disabled_test.c", backends,
     ["-DHAVE_THREADS", "-DRETRO_ATOMIC_FORCE_VOLATILE", "-DRETRO_ATOMIC_SUPPRESS_WARNING"]),
    ("renderer_backend_test", "renderer_backend_test.c", [],
     ["-DHAVE_THREADS", "-DHAVE_RMP4", "-DHAVE_RVP9", "-DHAVE_RAV1"]),
    ("renderer_backend_nothreads_test", "renderer_backend_test.c", [],
     ["-DHAVE_RMP4", "-DHAVE_RVP9", "-DHAVE_RAV1"]),
    ("renderer_backend_volatile_test", "renderer_backend_test.c", [],
     ["-DHAVE_THREADS", "-DHAVE_RMP4", "-DHAVE_RVP9", "-DHAVE_RAV1",
      "-DRETRO_ATOMIC_FORCE_VOLATILE", "-DRETRO_ATOMIC_SUPPRESS_WARNING"]),
    ("color_refine_test", "color_refine_test.c", [], []),
]
for codec in ("hevc", "vp9", "av1"):
    sources = [common / ("mediacodec_" + codec + "_decoder.c"), common / "android_image_queue.c"]
    sources += [common / ("mediacodec_hevc_packet.c" if codec == "hevc" else "mediacodec_color.c")]
    cases.append((codec + "_decoder_test", codec + "_decoder_test.c", sources, ["-DHAVE_THREADS"]))
    extra = ["-DHAVE_THREADS", "-pthread"] + (["-lwinmm"] if os.name == "nt" else [])
    cases.append((codec + "_backend_test", codec + "_backend_test.c", export + threads, extra))
with tempfile.TemporaryDirectory(prefix="mediacodec-backend-") as directory:
    objects = {}
    for source in threads:
        obj = Path(directory) / (source.stem + ".o")
        command = compiler + flags + ["-DHAVE_THREADS", "-pthread"]
        if os.name == "nt" and source.name == "rtime.c":
            command += ["-Wno-cast-function-type"]
        command += ["-c", str(source), "-o", str(obj)]
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        objects[source] = obj
    for name, unit, sources, extra in cases:
        binary = Path(directory) / (name + (".exe" if os.name == "nt" else ""))
        command = compiler + flags + [str(here / unit)] + [str(objects.get(source, source)) for source in sources]
        command += extra + ["-o", str(binary), "-lm"]
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=90)
