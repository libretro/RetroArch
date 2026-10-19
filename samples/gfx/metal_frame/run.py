#!/usr/bin/env python3
"""Exercise geometry and the shipping Metal lease with mock or Apple objects."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def run(args):
    print("+", shlex.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mock", action="store_true")
    parser.add_argument("--native", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="metal-frame-") as name:
        tmp = Path(name)
        binary = tmp / ("geometry.exe" if os.name == "nt" else "geometry")
        run(shlex.split(os.environ.get("CC", "cc")) + ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-Wframe-larger-than=4096", "-I" + str(ROOT / "libretro-common/include"), str(HERE / "geometry_test.c"), "-o", str(binary)])
        run(shlex.split(os.environ.get("RUNNER", "")) + [str(binary)])
        if not args.mock and not args.native:
            return
        source = (ROOT / "gfx/drivers/metal.m").read_text(encoding="utf-8")
        section = source.split("/* METAL_FRAME_LEASE_BEGIN */", 1)[1].split("/* METAL_FRAME_LEASE_END */", 1)[0]
        section = section.replace('#include "../common/metal_frame.h"', '#include "' + (ROOT / "gfx/common/metal_frame.h").as_posix() + '"')
        driver_header = (ROOT / "gfx/video_driver.h").read_text(encoding="utf-8")
        carrier = driver_header.split("typedef struct video_frame_import\n", 1)[1].split("} video_frame_import_t;", 1)[0]
        preamble = '#include <formats/video_hw_frame.h>\n#include <formats/image_yuv_blit.h>\n#include <defines/cocoa_defines.h>\n#include <string.h>\n#include <assert.h>\n'
        preamble += 'enum video_texture_update' + driver_header.split('enum video_texture_update', 1)[1].split('};', 1)[0] + '};\n'
        preamble += 'typedef struct video_frame_import\n' + carrier + '} video_frame_import_t;\n'
        fixture = HERE / ("mock.m" if args.mock else "native.m")
        text = fixture.read_text(encoding="utf-8").replace("/* SHIPPING_LEASE */", preamble + section)
        if args.native:
            native_import = (HERE / "native_import.m").read_text(encoding="utf-8")
            allocation = "- (Texture *)newImportedTexture:" + source.rsplit("- (Texture *)newImportedTexture:", 1)[1].split("\n}", 1)[0] + "\n}"
            native_import = native_import.replace("/* SHIPPING_IMPORT_ALLOCATION */", allocation)
        cv = source.split("/* METAL_FRAME_CV_BEGIN */", 1)[1].split("/* METAL_FRAME_CV_END */", 1)[0]
        cv_fixture = (HERE / ("cv_mock.m" if args.mock else "cv_native.m")).read_text(encoding="utf-8")
        text = text.replace("/* COREVIDEO */", cv_fixture.replace("/* SHIPPING_CV */", cv))
        convert = source.split("/* METAL_FRAME_SDR_CONVERT_BEGIN */", 1)[1].split("/* METAL_FRAME_SDR_CONVERT_END */", 1)[0]
        convert = convert.replace('#include "../common/metal_frame_convert.h"', '#include "' + (ROOT / "gfx/common/metal_frame_convert.h").as_posix() + '"')
        pipeline = source.split("/* METAL_FRAME_SDR_PIPELINE_BEGIN */", 1)[1].split("/* METAL_FRAME_SDR_PIPELINE_END */", 1)[0] if args.native else ""
        sdr_fixture = (HERE / ("sdr_mock.m" if args.mock else "sdr_native.m")).read_text(encoding="utf-8")
        text = text.replace("/* SDR_CONVERT */", sdr_fixture.replace("/* SHIPPING_SDR */", pipeline + convert))
        if args.native:
            text = text.replace("/* NATIVE_IMPORT */", native_import)
        importer = source.split("/* METAL_FRAME_IMPORT_BEGIN */", 1)[1].split("/* METAL_FRAME_IMPORT_END */", 1)[0]
        text = text.replace("/* SHIPPING_IMPORT */", importer)
        if args.mock:
            text = text.replace("/* SDR_IMPORT_TEST */", (HERE / "sdr_import_mock.m").read_text(encoding="utf-8"))
        text = text.replace('#include "sdr_pixels.h"', '#include "' + (HERE / "sdr_pixels.h").as_posix() + '"')
        unit = tmp / "lease.m"
        unit.write_text(text, encoding="utf-8")
        flags = shlex.split(os.environ.get("OBJCFLAGS", "")) + ["-fblocks", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-I" + str(ROOT / "libretro-common/include")]
        if args.mock:
            blocks = tmp / "objc"
            blocks.mkdir()
            (tmp / "AvailabilityMacros.h").write_text("/* Supplied by GNUstep Foundation. */\n", encoding="utf-8")
            (blocks / "blocks_runtime.h").write_text("#include <Block.h>\n", encoding="utf-8")
            flags += shlex.split(subprocess.check_output(["gnustep-config", "--objc-flags"], text=True))
            flags += ["-I" + str(tmp), "-I" + subprocess.check_output(["gcc", "-print-file-name=include"], text=True).strip()]
            libs = shlex.split(subprocess.check_output(["gnustep-config", "--base-libs"], text=True))
            objc_library = Path(subprocess.check_output(["gcc", "-print-file-name=libobjc.so"], text=True).strip())
            if not objc_library.is_file():
                raise RuntimeError("GCC Objective-C runtime library is unavailable: " + str(objc_library))
            # Link BlocksRuntime first: GNUstep also exports older block helpers.
            libs = ["-lBlocksRuntime", "-L" + str(objc_library.parent)] + libs
        else:
            flags += ["-fobjc-arc", "-mmacosx-version-min=10.14"]
            libs = ["-framework", "Foundation", "-framework", "Metal", "-framework", "CoreFoundation", "-framework", "CoreVideo"]
        exe = tmp / "lease"
        run([os.environ.get("OBJC", "clang")] + flags + [str(unit), "-o", str(exe)] + libs)
        result = subprocess.run([str(exe)], check=False)
        if args.native and result.returncode == 77:
            sys.exit(77)
        result.check_returncode()


if __name__ == "__main__":
    main()
