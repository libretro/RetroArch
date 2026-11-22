#!/usr/bin/env python3
"""Run real headless OpenGL pixels through the shared viewer history chain."""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile
here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix="gl-history-pixels-") as directory:
    common = (root / "gfx/common/gl_common.c").read_text(encoding="utf-8")
    planar = common[common.index("#ifdef HAVE_GL_PLANAR", common.index("#ifdef HAVE_GL_PLANAR") + 1):]
    header = (root / "gfx/common/gl_common.h").read_text(encoding="utf-8")
    start = header.index("typedef struct gl_planar_tex")
    end = header.index("/* Whether planar frames", start)
    (Path(directory) / "planar.inc").write_text(header[start:end] + planar, encoding="utf-8")
    exe = Path(directory) / "pixels"
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    command += shlex.split(os.environ.get("CFLAGS", ""))
    command += ["-I" + str(directory), "-I" + str(root), "-I" + str(root / "libretro-common/include"),
                str(here / "history_pixels.c"), str(root / "libretro-common/formats/image/image_yuv_blit.c"), "-o", str(exe), "-lEGL", "-lGL", "-lm"]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True)
    legacy = Path(directory) / "legacy-pixels"
    legacy_command = [str(legacy) if item == str(exe) else item for item in command]
    legacy_command.insert(1, "-DGL_POOL_LEGACY")
    subprocess.run(legacy_command, check=True)
    subprocess.run([str(legacy)], check=True, env={**os.environ,
                   "MESA_GL_VERSION_OVERRIDE": "2.1", "MESA_GLSL_VERSION_OVERRIDE": "120"})

# Compile the shipping GLCore lifecycle separately with real texture/FBO pixels.
source = (root / "gfx/drivers_shader/shader_gl3.c").read_text(encoding="utf-8")
def function(signature):
    match = re.search(re.escape(signature) + r"[^;]*?\n\{", source)
    assert match, signature
    return source[match.start():source.index("\n}\n", match.end()) + 3]
with tempfile.TemporaryDirectory(prefix="slang-history-pixels-") as directory:
    directory = Path(directory)
    header = (root / "gfx/drivers_shader/shader_gl3.h").read_text(encoding="utf-8")
    start = header.index("struct gl3_filter_chain_texture\n{")
    (directory / "texture.inc").write_text(header[start:header.index("\n};", start) + 3], encoding="utf-8")
    (directory / "slang_history.inc").write_text("\n".join(function(signature) for signature in (
        "static void gl3_chain_release_history_slot(",
        "static void gl3_chain_release_history_leases(",
        "static bool gl3_framebuffer_preserve(",
        "bool gl3_filter_chain_preserve_history(",
        "bool gl3_filter_chain_retire_history(",
        "bool gl3_filter_chain_history_references(",
        "void gl3_filter_chain_set_history_borrow(",
        "static void gl3_chain_end_frame(",
        "static void gl3_chain_update_history_info(")), encoding="utf-8")
    exe = directory / "pixels"
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    command += shlex.split(os.environ.get("CFLAGS", ""))
    command += ["-I" + str(directory), "-I" + str(root), "-I" + str(root / "libretro-common/include"),
                str(here / "slang_history_pixels.c"), "-o", str(exe), "-lEGL", "-lGL", "-lm"]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True)

# GLES creators register storage explicitly instead of querying texture levels.
with tempfile.TemporaryDirectory(prefix="gles-history-pixels-") as directory:
    for version in (2, 3):
        exe = Path(directory) / ("gles" + str(version))
        command = shlex.split(os.environ.get("CC", "cc"))
        command += ["-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-DHAVE_OPENGLES", "-DHAVE_OPENGLES" + str(version),
                    str(here / "gles_history_pixels.c"), "-o", str(exe), "-lEGL", "-lGLESv2"]
        subprocess.run(command, check=True)
        subprocess.run([str(exe)], check=True)
