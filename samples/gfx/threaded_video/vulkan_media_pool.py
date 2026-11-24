#!/usr/bin/env python3
"""Exercise shipping Vulkan pool, uploads and compute target ownership."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import struct
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")


def function(signature):
    match = re.search(re.escape(signature) + r"[^;]*?\n\{", source)
    assert match, signature
    return source[match.start():source.index("\n}\n", match.end()) + 3]


# Output versions cannot retire while shader leases or recorded descriptors survive.
teardown = function("static void vulkan_free(")
released = max(teardown.index("vulkan_filter_chain_free(vk->media_shader_chain[1])"),
               teardown.index("vulkan_views_free_chains(vk, 0, true)"))
for call in ("vulkan_android_renderer_flush(vk)", "vulkan_vaapi_renderer_flush(vk)", "vulkan_stream_states_flush(vk)"):
    assert teardown.index(call) > released, call + " precedes history release"
assert teardown.index("vulkan_deinit_pipelines(vk)") > teardown.index("vulkan_stream_states_flush(vk)")

with tempfile.TemporaryDirectory(prefix="vulkan-media-pool-") as directory:
    tmp = Path(directory)
    sdk = root / "gfx/include/vk_video"
    if not sdk.is_dir():
        sdk = Path("/usr/include/vk_video")
    if sdk.is_dir():
        shutil.copytree(sdk, tmp / "vk_video")
    types = []
    for name in ("vk_texture", "vk_media_version", "vk_media_pool", "vk_stream_state", "vk_planar_atlas_slot", "vk_planar_atlas"):
        start = source.index("struct " + name + "\n{")
        types.append(source[start:source.index("\n};", start) + 3])
    driver = (root / "gfx/video_driver.h").read_text(encoding="utf-8")
    start = driver.index("enum video_texture_update\n{")
    types.append(driver[start:driver.index("\n};", start) + 3])
    (tmp / "pool_types.inc").write_text("\n".join(types) + "\n", encoding="utf-8")
    signatures = (
        "static void vulkan_destroy_texture(",
        "static void vulkan_media_history_retain(",
        "static void vulkan_media_history_release(",
        "static enum video_texture_update vulkan_update_texture_planar(",
        "static enum video_texture_update vulkan_update_texture_internal(")
    (tmp / "pool_upload.inc").write_text("\n".join(function(s) for s in signatures), encoding="utf-8")
    (tmp / "pool_convert.inc").write_text(function(
        "static enum video_texture_update vulkan_planar_materialize_gpu("), encoding="utf-8")
    coefficients = (root / "libretro-common/formats/image/image_yuv_blit.c").read_text(encoding="utf-8")
    start = coefficients.index("void image_yuv_coefficients(")
    prefix = coefficients[coefficients.index("typedef struct"):coefficients.index("#define IMAGE_YUV_SHIFT 14") + len("#define IMAGE_YUV_SHIFT 14")]
    (tmp / "pool_coeff.inc").write_text(prefix + "\n" +
        coefficients[start:coefficients.index("\n}\n", start) + 3], encoding="utf-8")
    (tmp / "pool_atlas.inc").write_text("\n".join(function(s) for s in (
        "static void vulkan_planar_atlas_free(", "static bool vulkan_planar_atlas_idle(",
        "static enum video_texture_update vulkan_planar_atlas_update(",
        "static enum video_texture_update vulkan_planar_materialize_gpu(",
        "static void vulkan_planar_materialize(")), encoding="utf-8")
    (tmp / "pool_retire.inc").write_text("\n".join(function(signature) for signature in (
        "static void vulkan_deferred_textures_push(",
        "static void vulkan_deferred_textures_collect(",
        "static void vulkan_deferred_textures_tick(",
        "static void vulkan_deferred_textures_flush(")), encoding="utf-8")
    (tmp / "pool_destroy.inc").write_text(function("static void vulkan_destroy_texture("), encoding="utf-8")
    opaque = []
    for platform in ("android", "vaapi"):
        adapter = (root / ("gfx/drivers/vulkan_" + platform + "_video.h")).read_text()
        for suffix in ("destroy_destination", "flush", "import"):
            name = "vulkan_" + platform + "_renderer_" + suffix
            start = adapter.index(name + "(")
            start = adapter.rfind("static ", 0, start)
            opaque.append(adapter[start:adapter.index("\n}", start) + 2])
    (tmp / "pool_opaque.inc").write_text("\n".join(opaque) + "\n", encoding="utf-8")

    common = (root / "gfx/common/vulkan_common.c").read_text(encoding="utf-8")
    start = common.index("uint32_t vulkan_find_memory_type_fallback(")
    (tmp / "pool_memory.inc").write_text(common[start:common.index("\n}\n", start) + 3], encoding="utf-8")
    common = (root / "gfx/common/vulkan_common.h").read_text(encoding="utf-8")
    start = common.index("#define VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(")
    end = common.index("#define VULKAN_IMAGE_LAYOUT_TRANSITION(", start)
    (tmp / "pool_barrier.inc").write_text(common[start:common.index("\n", end) + 1], encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    windows = os.name == "nt" or "mingw" in " ".join(cc)
    exe = tmp / ("pool.exe" if windows else "pool")
    command = cc + shlex.split(os.environ.get("CFLAGS", "")) + [
        "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
        "-Werror=declaration-after-statement", "-Wno-unused-function",
        "-I" + str(tmp), "-I" + str(root), "-I" + str(root / "libretro-common/include"),
        "-isystem", str(root / "gfx/include"),
    ]
    gpu_source = Path(__file__).with_name("vulkan_media_pool_pixels.c")
    if "--gpu" in sys.argv:
        gpu_lib = "-lvulkan-1" if windows else "-lvulkan"
        subprocess.run(command + [str(gpu_source), "-o", str(exe), gpu_lib], check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
        binary = tmp / "fresh-atlas.spv"
        subprocess.run([os.environ.get("GLSLANG_VALIDATOR", "glslangValidator"),
                        "-V", "-DPLANAR_ATLAS", "-S", "comp",
                        str(root / "gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp"),
                        "-o", str(binary)], check=True)
        data = binary.read_bytes()
        words = struct.unpack("<%dI" % (len(data) // 4), data)
        fresh = tmp / "fresh-atlas.inc"
        fresh.write_text("{" + ",".join("0x%08x" % w for w in words) + "}\n", encoding="utf-8")
        subprocess.run(command + ['-DVULKAN_ATLAS_TEST_SHADER="' + str(fresh).replace("\\", "/") + '"',
                                  str(gpu_source), "-o", str(exe), gpu_lib], check=True)
    else:
        subprocess.run(command + [str(Path(__file__).with_suffix(".c")), "-o", str(exe)], check=True)
        if windows:
            subprocess.run(command + ["-c", str(gpu_source), "-o", str(tmp / "pixels.o")], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
