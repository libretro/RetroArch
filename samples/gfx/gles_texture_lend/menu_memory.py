#!/usr/bin/env python3
"""Exercise GL2/GL3 menu uploads from persistent mapped buffers."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
windows = os.name == "nt"
def function(source, name):
    start = source.index(name)
    return source[start:source.index("\n}\n", start) + 3]
with tempfile.TemporaryDirectory(prefix="gl-menu-memory-") as directory:
    temp = Path(directory)
    for backend in (2, 3):
        source = (root / ("gfx/drivers/gl%d.c" % backend)).read_text()
        functions = ["static void *gl%d_menu_mem_alloc(" % backend,
                     "static bool gl%d_frame_mem_ready(" % backend,
                     "static void gl%d_frame_mem_free(" % backend,
                     "static void gl%d_set_texture_frame(" % backend]
        if backend == 2:
            functions = ["static unsigned gl2_get_alignment(",
                         "static void gl_load_texture_data("] + functions
        (temp / "menu.inc").write_text("\n".join(function(source, name) for name in functions))
        for api in (("gl",) if windows else ("gl", "es3")):
            exe = temp / ("menu-%d-%s%s" % (backend, api, ".exe" if windows else ""))
            flags = ["-std=c89", "-ffunction-sections", "-Wl,--gc-sections", "-O1", "-g1", "-Wall", "-Wextra",
                     "-Werror=implicit-function-declaration", "-Werror=declaration-after-statement",
                     "-DBACKEND=%d" % backend,
                     "-I" + str(temp), "-I" + str(root / "libretro-common/include"),
                     "-I" + str(root / "gfx/include")]
            flags += ["-DHAVE_OPENGL"] if api == "gl" else ["-DHAVE_OPENGLES", "-DHAVE_OPENGLES3"]
            if not windows:
                flags += ["-DHAVE_EGL"]
            flags += shlex.split(os.environ.get("CFLAGS", ""))
            sources = [str(Path(__file__).with_name("menu_memory_test.c").resolve()),
                       str(root / "gfx/common/gl_common.c"),
                       str(root / "libretro-common/formats/image/image_yuv_blit.c"),
                       str(root / ("libretro-common/glsym/glsym_%s.c" % api)),
                       str(root / "libretro-common/glsym/rglgen.c")]
            libs = ["-lopengl32", "-lgdi32"] if windows else ["-lEGL", "-lGL" if api == "gl" else "-lGLESv2"]
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags + sources +
                           ["-o", str(exe)] + libs, check=True)
            subprocess.run([str(exe)], check=True)
