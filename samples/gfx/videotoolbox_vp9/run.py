#!/usr/bin/env python3
"""Exercise VP9 setup, native decoder pixels and the Metal playback pipeline."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--native", action="store_true")
parser.add_argument("--pixels", action="store_true")
parser.add_argument("--pipeline", action="store_true")
args = parser.parse_args()
if args.pipeline:
    args.pixels = True
if args.pixels and not args.native:
    parser.error("--pixels requires --native")
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = ["-DHAVE_THREADS", "-O2", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
         "-Wno-long-long", "-Wframe-larger-than=2048", "-fstack-usage",
         "-I" + str(root / "libretro-common/include")]
if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
    flags += ["-Wno-c99-extensions"]
with tempfile.TemporaryDirectory(prefix="vt-vp9-setup-") as directory:
    path = Path(directory)
    libraries = []
    if args.native:
        flags += ["-DVT_VP9_NATIVE", "-mmacosx-version-min=10.14"]
        for framework in ("CoreMedia", "CoreVideo", "VideoToolbox", "CoreFoundation"):
            libraries += ["-framework", framework]
    else:
        for framework in ("CoreMedia", "CoreVideo", "VideoToolbox"):
            folder = path / framework
            folder.mkdir()
            (folder / (framework + ".h")).write_text(
                '#include "' + (here / "mock_apple.h").as_posix() + '"\n',
                encoding="utf-8")
        dispatch_headers = path / "dispatch"
        dispatch_headers.mkdir()
        (dispatch_headers / "dispatch.h").write_text(
            '#include "' + (here / "mock_dispatch.h").as_posix() + '"\n', encoding="utf-8")
        flags += ["-I" + str(path)]
    flags += shlex.split(os.environ.get("CFLAGS", ""))
    binary = path / ("setup.exe" if os.name == "nt" else "setup")
    thread_object = path / "rthreads.o"
    thread_command = compiler + ["-O2", "-DHAVE_THREADS",
        "-I" + str(root / "libretro-common/include")]
    if args.native:
        thread_command += ["-mmacosx-version-min=10.14"]
    thread_command += shlex.split(os.environ.get("CFLAGS", ""))
    thread_command += ["-Wno-unused-parameter"]
    thread_command += ["-c", str(root / "libretro-common/rthreads/rthreads.c"),
                      "-o", str(thread_object)]
    subprocess.run(thread_command, check=True)
    queue_object = path / "retro_spsc.o"
    queue_command = compiler + ["-O2", "-DHAVE_THREADS",
        "-I" + str(root / "libretro-common/include")]
    if args.native:
        queue_command += ["-mmacosx-version-min=10.14"]
    queue_command += shlex.split(os.environ.get("CFLAGS", ""))
    queue_command += ["-c", str(root / "libretro-common/queues/retro_spsc.c"),
                      "-o", str(queue_object)]
    subprocess.run(queue_command, check=True)
    if os.name != "nt":
        libraries += ["-pthread"]
    command = compiler + flags + [str(here / "setup_test.c"), str(thread_object), str(queue_object),
                                  "-o", str(binary)] + libraries
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)], check=True, timeout=90)

    command = compiler + flags + [str(here / "snapshot_test.c"), str(thread_object),
        "-o", str(binary)] + libraries
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                   check=True, timeout=90)

    if not args.native:
        for name in ("owner_test.c", "decoder_test.c", "dispatch_test.c", "backend_test.c", "driver_test.c"):
            command = compiler + flags + [str(here / name), str(thread_object),
                str(queue_object), "-o", str(binary)] + libraries
            print("+", shlex.join(command), flush=True)
            subprocess.run(command, check=True)
            subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)],
                           check=True, timeout=90)
    else:
        unit = path / "native_setup.m"
        unit.write_text('#include "' + (here / "setup_test.c").as_posix() + '"\n',
                        encoding="utf-8")
        for mode in ("-fobjc-arc", "-fno-objc-arc"):
            command = compiler + flags + [mode, str(unit), str(thread_object),
                str(queue_object), "-o", str(binary)] + libraries
            print("+", shlex.join(command), flush=True)
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True, timeout=90)

        codec_objects = {}
        for codec, source in (("vp9", "vp9/rvp9.c"), ("hevc", "h265/rh265.c"),
                              ("av1", "av1/rav1_hw.c")):
            obj = path / (codec + ".o")
            command = compiler + ["-O2", "-Wno-unused-parameter",
                "-I" + str(root / "libretro-common/include"),
                "-mmacosx-version-min=10.14", "-c",
                str(root / "libretro-common/formats" / source), "-o", str(obj)]
            subprocess.run(command, check=True)
            codec_objects[codec] = str(obj)
        for features, codecs in ((["-DHAVE_RVP9"], ["vp9"]),
                (["-DHAVE_RMP4"], ["hevc"]),
                (["-DHAVE_RMP4", "-DHAVE_RVP9"], ["hevc", "vp9"]),
                (["-DHAVE_RAV1"], ["av1"]),
                (["-DHAVE_RMP4", "-DHAVE_RAV1"], ["hevc", "av1"]),
                (["-DHAVE_RVP9", "-DHAVE_RAV1"], ["vp9", "av1"]),
                (["-DHAVE_RMP4", "-DHAVE_RVP9", "-DHAVE_RAV1"], ["hevc", "vp9", "av1"])):
            for mode in ("-fobjc-arc", "-fno-objc-arc"):
                command = compiler + flags + features + [mode, "-fblocks",
                    "-Wno-unused-function", str(here / "lifecycle_test.m"),
                    str(thread_object), str(queue_object)]
                command += [codec_objects[codec] for codec in sorted(set(codecs + ["hevc"]))]
                command += ["-o", str(binary)] + libraries
                print("+", shlex.join(command), flush=True)
                subprocess.run(command, check=True)
                subprocess.run([str(binary)], check=True, timeout=90)

        if args.pixels:
            feature_object = path / "features_cpu.o"
            command = compiler + ["-O2", "-mmacosx-version-min=10.14",
                "-I" + str(root / "libretro-common/include"), "-c",
                str(root / "libretro-common/features/features_cpu.c"),
                "-o", str(feature_object)]
            subprocess.run(command, check=True)
            command = compiler + flags + ["-Wno-unused-function",
                str(here / "native_pixel_test.c"), str(thread_object),
                str(queue_object), codec_objects["vp9"], str(feature_object),
                "-o", str(binary)] + libraries
            print("+", shlex.join(command), flush=True)
            subprocess.run(command, check=True)
            result = subprocess.run([str(binary)], check=False, timeout=90)
            if result.returncode or not args.pipeline:
                raise SystemExit(result.returncode)


        if args.pipeline:
            import importlib.util
            specification = importlib.util.spec_from_file_location(
                "hevc_metal_fixture", here.parent / "videotoolbox_hevc/run.py")
            helper = importlib.util.module_from_spec(specification)
            specification.loader.exec_module(helper)
            unit = path / "vp9_metal.m"
            text = helper.metal_test_source().replace(
                "VideoToolbox native adapter: exported pixels survive decoder, session and pool teardown",
                "VP9 stream export: decoded planes and FrameView pixels survive stream/pool close")
            text = """#import <VideoToolbox/VideoToolbox.h>
#include <stdio.h>
static unsigned vp9_test_opens;
unsigned videotoolbox_vp9_test_session_opens(void) { return vp9_test_opens; }
static OSStatus vp9_test_session_create(CFAllocatorRef allocator,
      CMVideoFormatDescriptionRef format, CFDictionaryRef specification,
      CFDictionaryRef attributes, const VTDecompressionOutputCallbackRecord *callback,
      VTDecompressionSessionRef *session)
{
   OSStatus status = VTDecompressionSessionCreate(allocator, format,
         specification, attributes, callback, session);
   fprintf(stderr, "VP9 test-only native session open %u: status %ld\\n",
         ++vp9_test_opens, (long)status);
   return status;
}
#define VTDecompressionSessionCreate vp9_test_session_create
""" + text
            export = "if (!metal_frame_create_cv(&source, image, (void*)pool->cache, out)) return -1;"
            if text.count(export) != 1:
                raise RuntimeError("shipping Metal export diagnostic anchor changed")
            text = text.replace(export, """if (!metal_frame_create_cv(&source, image, (void*)pool->cache, out))
      {
         CFStringRef keys[3] = { kCVImageBufferColorPrimariesKey,
            kCVImageBufferTransferFunctionKey, kCVImageBufferYCbCrMatrixKey };
         unsigned tag;
         fprintf(stderr, "VP9 test-only Metal CV export refused: source %u %ux%u color %u/%u/%u range %u; native format %lu planes %lu\\n",
               source.pixfmt, source.width, source.height, source.color.primaries,
               source.color.transfer, source.color.matrix, source.color.range,
               (unsigned long)CVPixelBufferGetPixelFormatType(image),
               (unsigned long)CVPixelBufferGetPlaneCount(image));
         for (tag = 0; tag < 3; tag++)
         {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CFTypeRef value = CVBufferGetAttachment(image, keys[tag], NULL);
#pragma clang diagnostic pop
            if (value) CFShow(value);
         }
         fflush(stderr);
         return -1;
      }""")
            unit.write_text(text, encoding="utf-8")
            metal_object = path / "vp9_metal.o"
            command = compiler + ["-DHAVE_THREADS", "-DHAVE_RVP9", "-O2",
                "-fobjc-arc", "-fblocks", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-function", "-Wframe-larger-than=4096",
                "-mmacosx-version-min=10.14", "-I" + str(root / "libretro-common/include"),
                "-c", str(unit), "-o", str(metal_object)]
            print("+", shlex.join(command), flush=True)
            subprocess.run(command, check=True)
            objects = [str(thread_object), str(queue_object), str(feature_object),
                       codec_objects["vp9"], codec_objects["hevc"], str(metal_object)]
            sources = ["mp4/rmp4.c", "mp4/rmp4_video.c", "webm/rwebm.c",
                "webm/rwebm_video.c", "vp8/rvp8.c", "h264/rh264.c",
                "image/image_blit_bands.c", "image/image_hdr_blit.c", "../time/rtime.c"]
            for index, source in enumerate(sources):
                obj = path / ("stream-" + str(index) + ".o")
                source_path = root / "libretro-common/formats" / source
                if source == "mp4/rmp4_video.c":
                    probe = path / "mp4_state.c"
                    probe.write_text('#include "' + source_path.as_posix() + '"\n'
                        'void videotoolbox_vp9_test_mp4_state(rmp4_video_stream_t *s, unsigned out[4])\n'
                        '{ out[0] = s->vp9_hw != NULL; out[1] = s->vp9 != NULL;\n'
                        '  out[2] = s->rndr_kind; out[3] = s->hwf_untaken; }\n',
                        encoding="utf-8")
                    source_path = probe
                command = compiler + ["-O2", "-std=c89", "-pedantic", "-Wall",
                    "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-long-long",
                    "-Wno-c99-extensions", "-Wframe-larger-than=2048", "-fstack-usage",
                    "-DHAVE_RMP4", "-DHAVE_RWEBM", "-DHAVE_RVP9",
                    "-mmacosx-version-min=10.14", "-I" + str(root / "libretro-common/include"),
                    "-c", str(source_path), "-o", str(obj)]
                subprocess.run(command, check=True)
                objects.append(str(obj))
            command = compiler + flags + [str(here / "native_pipeline_test.c"),
                *objects, "-o", str(binary), "-framework", "Foundation",
                "-framework", "Metal", "-lm"] + libraries
            print("+", shlex.join(command), flush=True)
            subprocess.run(command, check=True)
            result = subprocess.run([str(binary)], check=False, timeout=180)
            raise SystemExit(result.returncode)
