#!/usr/bin/env python3
"""Exercise the shipping metadata provider and DXGI submission with CPU mocks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_driver.c").read_text(encoding="utf-8")
start = source.index("static retro_atomic_int_t video_hdr_output_mode;")
end = source.index("void video_driver_set_display_peak_nits(", start)
dxgi = (root / "gfx/common/dxgi_common.c").read_text(encoding="utf-8")
dxgi = dxgi[dxgi.index("void dxgi_swapchain_color_space("):]
dxgi = dxgi[:dxgi.index("/* ------------------------------------------------------------------")]
with tempfile.TemporaryDirectory(prefix="retroarch-hdr-query-") as directory:
    directory = Path(directory)
    (directory / "provider.inc").write_text(source[start:end], encoding="utf-8")
    (directory / "dxgi.inc").write_text(dxgi, encoding="utf-8")
    cc = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    libs = [] if os.name == "nt" else ["-pthread"]
    exe = directory / ("provider.exe" if os.name == "nt" else "provider")
    subprocess.run(cc + ["-std=gnu89", "-O1", "-g1", "-Wall", "-Wextra",
                        "-Werror=declaration-after-statement",
                        "-Werror=implicit-function-declaration", "-DHAVE_THREADS",
                        "-I" + str(root / "libretro-common/include"),
                        "-I" + str(directory)] + flags +
                   [str(Path(__file__).with_name("provider_test.c")),
                    str(root / "libretro-common/rthreads/rthreads.c"),
                    "-o", str(exe)] + libs, check=True)
    subprocess.run([str(exe)], check=True)
