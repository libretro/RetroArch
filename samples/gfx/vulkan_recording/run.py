#!/usr/bin/env python3
"""Exercise shipping Vulkan capture placement and submission decisions."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
source = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")
functions = ""
for name in ("static record_android_capture_renderer_t *vulkan_record_gpu(",
             "static void vulkan_record_service(",
             "static VkResult vulkan_record_submit("):
    start = source.index(name)
    functions += source[start:source.index("\n}\n", start) + 3]
common = (root / "gfx/common/vulkan_common.h").read_text(encoding="utf-8")
flags = ""
for name in ("enum vk_flags", "enum vulkan_context_flags"):
    start = common.index(name)
    flags += common[start:common.index("\n};", start) + 3] + "\n"
frame = source[source.index("static bool vulkan_frame("):]
record = frame.index("recording = vulkan_record_gpu(")
present = frame.index("/* Prepare backbuffer for presentation. */")
end = frame.index("recording_result = vkEndCommandBuffer(")
assert record < present < end
start = frame.index("   recording_result = vkEndCommandBuffer(")
end_frame = frame[start:frame.index("\n#else", start)]
start = frame.index("   vulkan_record_service(video_info);")
submit = frame[start:frame.index("\n#else", start)]
assert frame.rfind("slock_lock(vk->context->queue_lock)", 0, start) >= 0
assert start < frame.index("   vulkan_present_wait = 0;")
fixture = (here / "frame_test.c").read_text(encoding="utf-8")
fixture = fixture.replace("/* FUNCTIONS */", functions)
fixture = fixture.replace("/* FLAGS */", flags)
fixture = fixture.replace("/* END_FRAME */", end_frame)
fixture = fixture.replace("/* SUBMIT */", submit)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
runner = shlex.split(os.environ.get("RUNNER", ""))
windows = os.name == "nt" or any("mingw" in arg for arg in cc)
with tempfile.TemporaryDirectory(prefix="vulkan-recording-") as tmp:
    tmp = Path(tmp)
    unit = tmp / "frame.c"
    unit.write_text(fixture, encoding="utf-8")
    for hdr in (False, True):
        exe = tmp / ("frame.exe" if windows else "frame")
        command = cc + flags + ["-std=c89", "-pedantic", "-Wall", "-Wextra",
                  "-Werror", "-Wno-long-long", "-Wframe-larger-than=2048",
                  "-I" + str(root), "-I" + str(root / "gfx/include"),
                  "-I" + str(root / "libretro-common/include")]
        if hdr:
            command += ["-DVULKAN_HDR_SWAPCHAIN"]
        subprocess.run(command + [str(unit), "-o", str(exe)], check=True)
        subprocess.run(runner + [str(exe)], check=True)
