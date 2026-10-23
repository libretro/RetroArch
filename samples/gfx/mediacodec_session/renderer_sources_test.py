#!/usr/bin/env python3
"""Compare Android Makefile and Griffin media objects across codec/thread gates."""
import itertools
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
compiler = shlex.split(os.environ.get("CC", "cc"))
java = Path(os.environ.get("JAVA_HOME") or Path(shutil.which("javac")).resolve().parent.parent)
test_flags = ["-O1", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
              "-Wno-long-long", "-Wframe-larger-than=2048"]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True):
    test_flags += ["-Wno-c99-extensions"]
if os.environ.get("SANITIZER"):
    test_flags += ["-fsanitize=" + os.environ["SANITIZER"], "-fno-omit-frame-pointer"]
shared = [
    "vulkan_android_device", "android_image_queue", "android_frame",
    "vulkan_android_image", "vulkan_android_sync", "vulkan_android_pipeline",
    "vulkan_android_submit", "vulkan_android_queue", "vulkan_android_cache_entry", "vulkan_android_cache",
]
native = [
    "mediacodec_inventory", "android_image_ndk", "mediacodec_session",
    "mediacodec_color", "mediacodec_frame_export",
]
hevc = [
    "mediacodec_hevc_packet", "mediacodec_hevc_decoder", "mediacodec_hevc_backend",
]
vp9 = ["mediacodec_vp9_decoder", "mediacodec_vp9_backend"]
av1 = ["mediacodec_av1_decoder", "mediacodec_av1_backend"]


def objects(names):
    return {"gfx/common/" + name + ".o" for name in names}


recording = {"record/" + name + ".o" for name in [
    "record_mediacodec_encoder", "record_mux_mediacodec", "record_mediacodec_worker",
    "record_mediacodec_surface", "record_mediacodec_file", "record_session_mediacodec",
    "record_dispatch_mediacodec", "record_gpu_dispatch_mediacodec",
    "record_mediacodec_gpu_worker", "record_android_capture_config",
    "record_android_capture_request", "record_android_capture_images",
    "record_android_capture_submit", "record_android_capture_pipe",
    "record_android_capture_pool", "record_android_capture_ops",
    "record_android_capture_egl", "record_android_capture_gl",
    "record_android_capture_draw", "record_gpu_queue", "record_audio_queue",
]}
registry = {"record/record_android_capture_device.o"}
frontend = {"record/drivers/record_mediacodec.o"}
candidates = (objects(shared + native + hevc + vp9 + av1)
              | recording | registry | frontend)
source = (root / "griffin/griffin.c").read_text(encoding="utf-8")
start = source.index("#ifdef HAVE_VULKAN\n#if defined(ANDROID)")
end = source.index('#include "../gfx/common/vulkan_common.c"', start)
unit = source[start:end] + "\n#endif\n"
start = source.index('#include "../record/drivers/record_mediacodec.c"')
end = source.index("#ifdef HAVE_RECORD_METAL", start)
unit += source[start:end]
# This is an inventory, not a compilation of included implementation files.
# Normalize every captured C include, including the native recording registry.
unit = re.sub(r'#include "\.\./([^"\n]+)\.c"', r'"\1.o"', unit)
assert "#include" not in unit
with tempfile.TemporaryDirectory(prefix="android-media-objects-") as directory:
    makefile = Path(directory) / "objects.mk"
    makefile.write_text(
        "include " + str(root / "Makefile.common") + "\nobjects:\n\t@echo $(OBJ)\n",
        encoding="utf-8",
    )
    for (mp4, webm, av1_enabled, threads, record) in itertools.product(
            (0, 1), repeat=5):
        expected = (objects(shared)
                    if mp4 or webm or av1_enabled or record else set())
        expected |= frontend
        if threads:
            expected |= registry
        if record:
            expected |= recording
        if (mp4 or webm or av1_enabled or record) and threads:
            expected |= objects(
                native + (hevc if mp4 else []) + (vp9 if webm else [])
                + (av1 if av1_enabled else [])
            )
        command = [
            "make", "-s", "-f", str(makefile), "objects", "HAVE_VULKAN=1",
            "ANDROID=1", "LIBRETRO_COMM_DIR=libretro-common",
            "HAVE_RMP4=" + str(mp4), "HAVE_RVP9=" + str(webm),
            "HAVE_THREADS=" + str(threads), "HAVE_RAV1=" + str(av1_enabled),
            "HAVE_RECORD_MEDIACODEC=" + str(record), "HAVE_CONFIGFILE=1",
        ]
        if record and not threads:
            refused = subprocess.run(command, cwd=root, text=True,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert (refused.returncode
                    and "MediaCodec recording requires" in refused.stdout)
            continue
        output = subprocess.check_output(command, cwd=root, text=True).split()
        actual = [name for name in output if name in candidates]
        assert set(actual) == expected and len(actual) == len(expected), (
            command, actual, expected
        )
        flags = ["-DANDROID", "-DHAVE_VULKAN", "-DHAVE_CONFIGFILE"]
        flags += [
            "-D" + name for name, enabled in (
                ("HAVE_RMP4", mp4), ("HAVE_RVP9", webm),
                ("HAVE_RAV1", av1_enabled), ("HAVE_THREADS", threads),
                ("HAVE_RECORD_MEDIACODEC", record)
            ) if enabled
        ]
        output = subprocess.check_output(
            compiler + ["-E", "-P", "-x", "c"] + flags + ["-"],
            input=unit, text=True,
        )
        actual = re.findall(r'"([^"]+)"', output)
        assert set(actual) == expected and len(actual) == len(expected), (
            flags, actual, expected
        )
        binary = Path(directory) / ("renderer-" + "".join(
            map(str, (mp4, webm, av1_enabled, threads, record))))
        command = compiler + test_flags + flags + [
            "-I" + str(root / "gfx/include"),
            "-I" + str(root / "libretro-common/include"),
            "-I" + str(java / "include"), "-I" + str(java / "include/linux"),
            str(root / "samples/gfx/mediacodec_session/renderer_backend_test.c"),
            "-o", str(binary),
        ]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
print("PASS: Android Makefile/Griffin media objects and renderer lifecycle pass "
      "24 codec/thread/recording configurations; eight invalid threadless "
      "recording profiles refused")
