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
    # Compile the actual shipping hook and ownership functions without unrelated
    # legacy audio functions, which have different historical stack budgets.
    shipping_audio = (root / "audio/audio_driver.c").read_text(encoding="utf-8")
    audio_fragment = []
    for signature in ("static INLINE int32_t audio_float_to_s16_sat(",
                      "static float audio_driver_pause_read(",
                      "static void audio_driver_pause_write(",
                      "static void audio_driver_pause_track(",
                      "void audio_driver_state_lock(",
                      "void audio_driver_state_unlock(",
                      "int audio_driver_record_tap_install(",
                      "bool audio_driver_record_tap_remove("):
        start = shipping_audio.index(signature)
        end = shipping_audio.index("\n}\n", start) + 3
        audio_fragment.append(shipping_audio[start:end])
    (Path(directory) / "audio_tap_fragment.c").write_text("\n".join(audio_fragment))
    binary = Path(directory) / ("encoder_test.exe" if os.name == "nt" else "encoder_test")
    for source, extra in (("test.c", []), ("audio_test.c", []),
                          ("mux_test.c", ["-DRECORD_MEDIACODEC_MUX_TEST"]),
                          ("worker_test.c", []), ("calibrated_worker_test.c", []),
                          ("surface_test.c", []),
                          ("capture_images_test.c", ["-DRECORD_ANDROID_CAPTURE_TEST"]),
                          ("capture_submit_test.c", []), ("capture_pipe_test.c", []),
                          ("capture_pool_test.c", []), ("device_test.c", []), ("request_test.c", []), ("config_test.c", []), ("audio_tap_test.c", []), ("audio_tap_driver_test.c", []), ("audio_tap_driver_test.c", ["-DHAVE_THREADS"]),
                          ("frontend_test.c", []), ("frontend_test.c", ["-DHAVE_WEBMPLAYER"]), ("capture_ops_test.c", []),
                          ("capture_egl_test.c", []),
                          ("capture_gl_test.c", []),
                          ("draw_test.c", []), ("gpu_worker_test.c", []),
                          ("file_test.c", ["-DRECORD_MEDIACODEC_FILE_TEST"]),
                          ("file_edit_test.c", ["-DRECORD_MEDIACODEC_FILE_TEST"]),
                          ("session_test.c", []), ("dispatch_test.c", []),
                          ("gpu_dispatch_test.c", [])):
        command = compiler + flags + extra + [str(here / source), "-o", str(binary)]
        if source in ("worker_test.c", "calibrated_worker_test.c"):
            command += ["-DHAVE_THREADS",
                        str(root / "record/record_mediacodec_worker.c"),
                        str(root / "record/record_audio_queue.c"),
                        str(root / "libretro-common/queues/retro_spsc.c")]
            if source == "worker_test.c":
                command += [str(root / "record/record_aac_probe.c")]
        if source == "session_test.c":
            command += ["-DHAVE_THREADS",
                        str(root / "record/record_audio_queue.c"),
                        str(root / "libretro-common/queues/retro_spsc.c")]
            if os.name != "nt":
                command += ["-pthread"]
        if source == "dispatch_test.c" and os.name != "nt":
            command += ["-DHAVE_THREADS", "-DRECORD_MEDIACODEC_DISPATCH_TEST", "-pthread"]
        if source == "gpu_dispatch_test.c":
            command += ["-DUSE_SYSTEM_EGL", "-I" + str(root / "gfx/include/ANGLE")]
            if os.name != "nt":
                command += ["-DHAVE_THREADS", "-DRECORD_MEDIACODEC_GPU_DISPATCH_TEST", "-pthread",
                            str(root / "record/record_gpu_queue.c"),
                            str(root / "libretro-common/queues/retro_spsc.c")]
        if source in ("capture_ops_test.c", "capture_egl_test.c"):
            command += ["-DUSE_SYSTEM_EGL", "-I" + str(root / "gfx/include/ANGLE"),
                        str(root / "record/record_android_capture_ops.c")]
        if source == "surface_test.c":
            command += ["-DUSE_SYSTEM_EGL", "-I" + str(root / "gfx/include/ANGLE"),
                        str(root / "record/record_mediacodec_surface.c")]
        if source == "file_edit_test.c":
            command += [str(root / "record/record_mediacodec_file.c"),
                        str(root / "record/record_mediacodec_file_edit.c"),
                        str(root / "record/record_mp4_audio_edit.c")]
        if source in ("file_test.c", "file_edit_test.c") and os.name != "nt":
            command += ["-D_POSIX_C_SOURCE=200809L"]
        if source == "gpu_worker_test.c":
            command += ["-DHAVE_THREADS", "-DUSE_SYSTEM_EGL",
                        "-I" + str(root / "gfx/include/ANGLE"),
                        str(root / "record/record_mediacodec_gpu_worker.c"),
                        str(root / "record/record_gpu_queue.c"),
                        str(root / "libretro-common/queues/retro_spsc.c")]
        if source == "draw_test.c":
            command += ["-I" + str(root / "gfx/include/ANGLE"),
                        str(root / "record/record_android_capture_draw.c")]
        if source == "capture_gl_test.c":
            command += ["-DUSE_SYSTEM_EGL", "-I" + str(root / "gfx/include/ANGLE"),
                        str(root / "record/record_android_capture_gl.c")]
        if source == "capture_images_test.c":
            command += ["-I" + str(root / "gfx/include"),
                        str(root / "record/record_android_capture_images.c")]
        if source in ("capture_pipe_test.c", "capture_pool_test.c"):
            command += ["-DUSE_SYSTEM_EGL", "-I" + str(root / "gfx/include"),
                        "-I" + str(root / "gfx/include/ANGLE")]
        if source == "capture_submit_test.c":
            command += ["-I" + str(root / "gfx/include"),
                        "-DHAVE_THREADS", "-DUSE_SYSTEM_EGL",
                        "-I" + str(root / "gfx/include/ANGLE"),
                        str(root / "record/record_android_capture_submit.c"),
                        str(root / "record/record_android_capture_pipe.c"),
                        str(root / "record/record_gpu_queue.c"),
                        str(root / "libretro-common/queues/retro_spsc.c"),
                        str(root / "gfx/common/vulkan_android_queue.c")]
        if source == "audio_tap_test.c":
            command += [str(root / "libretro-common/audio/conversion/float_to_s16.c")]
        if source == "audio_tap_driver_test.c":
            command += ["-I" + directory, "-DRECORD_AUDIO_TAP_TEST", "-D_GNU_SOURCE",
                        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                        "-Wno-unused-function", "-Wno-unused-parameter", "-Wno-unused-variable",
                        str(root / "record/record_audio_tap.c"),
                        str(root / "libretro-common/audio/conversion/float_to_s16.c"),
                        str(root / "libretro-common/rthreads/rthreads.c"),
                        str(root / "libretro-common/rthreads/retro_eventcount.c"), "-lm"]
            if os.name != "nt":
                command += ["-pthread"]
        if source == "frontend_test.c":
            command += ["-DHAVE_THREADS", "-DHAVE_CONFIGFILE", "-DRECORD_MEDIACODEC_FRONTEND_TEST",
                        "-DUSE_SYSTEM_EGL", "-I" + str(root / "gfx/include"),
                        "-I" + str(root / "gfx/include/ANGLE"), "-lm",
                        str(root / "record/record_audio_tap.c"),
                        str(root / "libretro-common/audio/conversion/float_to_s16.c")]
        if source == "config_test.c":
            command += ["-DRECORD_ANDROID_CAPTURE_CONFIG_TEST", "-DUSE_SYSTEM_EGL",
                        "-I" + str(root / "gfx/include"), "-I" + str(root / "gfx/include/ANGLE")]
        if source == "request_test.c":
            command += ["-DHAVE_THREADS", "-DUSE_SYSTEM_EGL", "-I" + str(root / "gfx/include"),
                        "-I" + str(root / "gfx/include/ANGLE")]
        if source == "device_test.c":
            shipping = (root / "gfx/common/vulkan_common.c").read_text(encoding="utf-8")
            start = shipping.index("   /* Retire capture children")
            prepare = shipping[start:shipping.index("\n#endif", start)]
            start = shipping.index("   if (record_android_capture_device_pending")
            pending = shipping[start:shipping.index("\n#endif", start)]
            start = shipping.index("         vkDestroyDevice(vk->context.device, NULL);")
            end = shipping.index("\n#endif", start)
            destroyed = shipping[start:end + len("\n#endif")]
            assert prepare.index("record_android_capture_device_prepare") < prepare.index("return;")
            assert shipping.index(prepare) < shipping.index("   vulkan_destroy_swapchain(vk);", shipping.index(prepare))
            unit = "static void teardown(VkDevice device, VkResult recording_idle, unsigned video_st_flags)\n{\n"
            unit += "struct test_context storage; struct test_context *vk = &storage;\n"
            unit += "vk->context.device = device;\n" + prepare + "\n" + pending
            unit += "\nif (video_st_flags & VIDEO_FLAG_CACHE_CONTEXT) cached++; else {\n"
            unit += destroyed + "\n}\n}\n"
            (Path(directory) / "device_teardown_fixture.h").write_text(unit)
            command += ["-DANDROID", "-DHAVE_THREADS", "-I" + str(root / "gfx/include"),
                        "-I" + directory]
        print("+", shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        if source == "file_edit_test.c":
            subprocess.run([sys.executable,
                            str(here.parent / "mp4_audio_edit/run.py"),
                            str(binary)], check=True, timeout=30)
            continue
        arguments = [str(binary)] + ([directory] if source == "file_test.c" else [])
        subprocess.run(arguments, check=True, timeout=30)
        if source == "file_test.c":
            assert (Path(directory) / "record-\u00e9.mp4").read_bytes() == b"sentinel"
            assert not (Path(directory) / "nonexistent-recording").exists()
