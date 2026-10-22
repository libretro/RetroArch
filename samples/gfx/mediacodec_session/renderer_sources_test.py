#!/usr/bin/env python3
"""Compare Android Makefile and Griffin media objects across codec/thread gates."""
import itertools
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
compiler = shlex.split(os.environ.get("CC", "cc"))
shared = [
    "vulkan_android_device", "android_image_queue", "android_frame",
    "vulkan_android_image", "vulkan_android_sync", "vulkan_android_pipeline",
    "vulkan_android_submit", "vulkan_android_cache_entry", "vulkan_android_cache",
]
native = [
    "mediacodec_inventory", "android_image_ndk", "mediacodec_session",
    "mediacodec_color", "mediacodec_frame_export",
]
hevc = [
    "mediacodec_hevc_packet", "mediacodec_hevc_decoder", "mediacodec_hevc_backend",
]
vp9 = ["mediacodec_vp9_decoder", "mediacodec_vp9_backend"]


def objects(names):
    return {"gfx/common/" + name + ".o" for name in names}


candidates = objects(shared + native + hevc + vp9)
source = (root / "griffin/griffin.c").read_text(encoding="utf-8")
start = source.index("#ifdef HAVE_VULKAN\n#if defined(ANDROID)")
end = source.index('#include "../gfx/common/vulkan_common.c"', start)
unit = re.sub(
    r'#include "../gfx/common/(.+)\.c"', r'"gfx/common/\1.o"', source[start:end]
) + "\n#endif\n"
with tempfile.TemporaryDirectory(prefix="android-media-objects-") as directory:
    makefile = Path(directory) / "objects.mk"
    makefile.write_text(
        "include " + str(root / "Makefile.common") + "\nobjects:\n\t@echo $(OBJ)\n",
        encoding="utf-8",
    )
    for mp4, webm, threads in itertools.product((0, 1), repeat=3):
        expected = objects(shared) if mp4 or webm else set()
        if (mp4 or webm) and threads:
            expected |= objects(
                native + (hevc if mp4 else []) + (vp9 if webm else [])
            )
        command = [
            "make", "-s", "-f", str(makefile), "objects", "HAVE_VULKAN=1",
            "ANDROID=1", "LIBRETRO_COMM_DIR=libretro-common",
            "HAVE_RMP4=" + str(mp4), "HAVE_RVP9=" + str(webm),
            "HAVE_THREADS=" + str(threads),
        ]
        output = subprocess.check_output(command, cwd=root, text=True).split()
        actual = [name for name in output if name in candidates]
        assert set(actual) == expected and len(actual) == len(expected), (
            command, actual, expected
        )
        flags = ["-DANDROID", "-DHAVE_VULKAN"]
        flags += [
            "-D" + name for name, enabled in (
                ("HAVE_RMP4", mp4), ("HAVE_RVP9", webm), ("HAVE_THREADS", threads)
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
print("PASS: Android Makefile/Griffin media objects agree across eight codec/thread configurations")
