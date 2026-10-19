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
        carrier = (ROOT / "gfx/video_driver.h").read_text(encoding="utf-8").split("typedef struct video_frame_import\n", 1)[1].split("} video_frame_import_t;", 1)[0]
        preamble = '#include <formats/video_hw_frame.h>\n#include <defines/cocoa_defines.h>\n#include <string.h>\n#include <assert.h>\n'
        preamble += 'typedef struct video_frame_import\n' + carrier + '} video_frame_import_t;\n'
        fixture = HERE / ("mock.m" if args.mock else "native.m")
        text = fixture.read_text(encoding="utf-8").replace("/* SHIPPING_LEASE */", preamble + section)
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
            # Link BlocksRuntime first: GNUstep also exports older block helpers.
            libs = ["-lBlocksRuntime"] + libs
        else:
            flags += ["-fobjc-arc", "-mmacosx-version-min=10.14"]
            libs = ["-framework", "Foundation", "-framework", "Metal", "-framework", "CoreFoundation"]
        exe = tmp / "lease"
        run([os.environ.get("OBJC", "clang")] + flags + [str(unit), "-o", str(exe)] + libs)
        result = subprocess.run([str(exe)], check=False)
        if args.native and result.returncode == 77:
            sys.exit(77)
        result.check_returncode()


if __name__ == "__main__":
    main()
