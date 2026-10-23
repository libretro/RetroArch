#!/usr/bin/env python3
"""Exercise shipping device capability publication, including concurrent queries."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
source = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")
common = (root / "gfx/common/vulkan_common.h").read_text(encoding="utf-8")
enums = ""
for text, name in [(source, "enum vulkan_texture_support"),
                   (source, "enum vulkan_device_support"),
                   (common, "enum vulkan_context_flags")]:
    start = text.index(name)
    enums += text[start:text.index("\n};", start) + 3] + "\n"
functions = ""
for name in ("static void vulkan_note_device(", "static bool vulkan_get_device("):
    start = source.index(name)
    functions += source[start:source.index("\n}\n", start) + 3]
getter = functions[functions.index("static bool vulkan_get_device("):]
assert "->flags" not in getter and "->pipelines" not in getter
initialization = source[source.index("static void *vulkan_init("):]
assert initialization.index("vulkan_note_device(vk)") < initialization.index("return vk;")
fixture = (here / "device_info_test.c").read_text(encoding="utf-8")
fixture = fixture.replace("/* ENUMS */", enums).replace("/* FUNCTIONS */", functions)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
runner = shlex.split(os.environ.get("RUNNER", ""))
windows = os.name == "nt" or any("mingw" in arg for arg in cc)
with tempfile.TemporaryDirectory(prefix="vulkan-device-info-") as tmp:
    tmp = Path(tmp)
    unit = tmp / "info.c"
    unit.write_text(fixture, encoding="utf-8")
    for android, threads in [(False, False), (False, True), (True, False), (True, True)]:
        exe = tmp / ("info.exe" if windows else "info")
        command = cc + flags + ["-O1", "-std=c89", "-pedantic", "-Wall", "-Wextra",
                  "-Werror", "-Wno-long-long", "-Wno-c99-extensions",
                  "-Wframe-larger-than=2048",
                  "-I" + str(root), "-I" + str(root / "gfx/include"),
                  "-I" + str(root / "libretro-common/include")]
        if android:
            command += ["-DTEST_ANDROID"]
        if threads:
            command += ["-DHAVE_THREADS"]
        if threads and os.environ.get("DEVICE_INFO_THREADS"):
            command += ["-DDEVICE_INFO_THREADS", "-pthread"]
        subprocess.run(command + [str(unit), "-o", str(exe)], check=True)
        subprocess.run(runner + [str(exe)], check=True)
