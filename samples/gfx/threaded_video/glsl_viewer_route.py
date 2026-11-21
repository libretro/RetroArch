#!/usr/bin/env python3
"""Compile the shipping context capability blocks and deferred-load guard."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
contexts = ["drm_ctx", "drm_go2_ctx", "orbis_ctx", "switch_ctx", "sdl3_gl_ctx",
            "wayland_ctx", "wgl_ctx", "x_ctx", "xegl_ctx"]
parts = []
for name in contexts:
    source = (root / "gfx/drivers_context" / (name + ".c")).read_text(encoding="utf-8")
    begin = source.index("#if defined(HAVE_SLANG) && (defined(HAVE_SPIRV_CROSS) || defined(HAVE_RSLC))")
    end = source.index("GFX_CTX_FLAGS_SHADERS_GLSL", begin)
    end = source.index("#endif", end) + len("#endif")
    parts.append("static unsigned " + name + "(void) { unsigned flags = 0;\n"
                 + source[begin:end] + "\nreturn flags; }\n")

source = (root / "gfx/drivers/gl3.c").read_text(encoding="utf-8")
begin = source.index("static bool gl3_shader_load_begin(")
end = source.index("   ds = (gl3_deferred_state_t*)calloc", begin)
guard = source[begin:end] + "   (void)ds; return true;\n}\n"
fixture = r'''
#include <assert.h>
#include <boolean.h>
#include <stdio.h>
#include <string.h>
#define BIT32_SET(v,n) ((v) |= (1u << (n)))
#define GFX_CTX_FLAGS_SHADERS_GLSL 0
#define GFX_CTX_FLAGS_SHADERS_SLANG 1
#define RARCH_SHADER_SLANG 1
static const char *video_driver_get_ident(void) { return "glcore"; }
#define string_is_equal(a,b) (!strcmp(a,b))
typedef struct { struct { bool active; } chain; } gl3_t;
typedef struct { const char *preset_path; } shader_load_deferred_t;
typedef struct { int unused; } gl3_deferred_state_t;
static int video_shader_parse_type(const char *p)
{ return !strcmp(p,"test.slangp") ? RARCH_SHADER_SLANG : 0; }
FUNCTIONS
int main(void)
{
   gl3_t gl = {{false}};
   shader_load_deferred_t d = {"test.glslp"};
   assert(!gl3_shader_load_begin(&gl,&d));
   d.preset_path = "test.slangp";
   assert(gl3_shader_load_begin(&gl,&d));
   gl.chain.active = true;
   assert(!gl3_shader_load_begin(&gl,&d));
   assert(!gl3_shader_load_begin(NULL,&d));
CONTEXT_CHECKS
   puts("PASS GLSL/Slang context capabilities and loader routing");
   return 0;
}
'''
checks = "\n".join("   assert(" + name + "() == EXPECTED);" for name in contexts)
fixture = fixture.replace("FUNCTIONS", "".join(parts) + guard).replace("CONTEXT_CHECKS", checks)
with tempfile.TemporaryDirectory(prefix="glsl-viewer-route-") as directory:
    path = Path(directory)
    unit = path / "route.c"
    unit.write_text(fixture, encoding="utf-8")
    for defs, expected in [(["-DHAVE_GLSL"], 1),
                           (["-DHAVE_SLANG", "-DHAVE_RSLC"], 2),
                           (["-DHAVE_GLSL", "-DHAVE_SLANG", "-DHAVE_RSLC"], 3)]:
        exe = path / "route.exe"
        subprocess.run(shlex.split(os.environ.get("CC", "cc"))
                       + ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                          "-I" + str(root / "libretro-common/include")]
                       + shlex.split(os.environ.get("CFLAGS", ""))
                       + defs + ["-DEXPECTED=" + str(expected), str(unit), "-o", str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
