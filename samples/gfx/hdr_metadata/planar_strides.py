#!/usr/bin/env python3
"""A decoder-plane borrow must not turn one upload into one per row."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
gl2 = (root / "gfx/drivers/gl2.c").read_text(encoding="utf-8")
common = (root / "gfx/common/gl_common.c").read_text(encoding="utf-8")


def function(source, name):
    start = source.index(name)
    return source[start:source.index("\n}\n", start) + 3]


start = common.index("#if", common.index("/* Rows of any stride"))
end = common.index("\n#endif", start) + len("\n#endif")
row_length = common[start:end]
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <formats/image.h>
#define HAVE_GL_PLANAR 1
#define GL2_FLAG_HAVE_FBO 1
#define GL_PLANAR_FAILED 1
#define GL_PLANAR_OK(p) (!((p)->flags & GL_PLANAR_FAILED))
#define GL_UNPACK_ALIGNMENT 1
#define GL_UNPACK_ROW_LENGTH 2
#define GL_TEXTURE_2D 3
#define GL_UNSIGNED_BYTE 4
typedef unsigned GLenum;
typedef int GLint;
typedef struct {
   unsigned flags;
   bool fp16_textures;
   struct { bool fp16_ok; } scrgb;
   struct { unsigned flags; } planar;
} gl2_t;
static unsigned uploads;
static bool gl_query_extension(const char *name) { (void)name; return false; }
static void glPixelStorei(GLenum name, GLint value) { (void)name; (void)value; }
static void glTexSubImage2D(GLenum target, int level, int x, int y,
      unsigned w, unsigned h, GLenum format, GLenum type, const void *pixels)
{
   (void)target; (void)level; (void)x; (void)y; (void)w; (void)h;
   (void)format; (void)type; (void)pixels;
   uploads++;
}
@@ROW_LENGTH@@
FUNCTIONS
int main(void)
{
   gl2_t gl;
   unsigned char pixels[32] = {0};
   memset(&gl, 0, sizeof(gl)); gl.flags = GL2_FLAG_HAVE_FBO;
   assert(gl2_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_YUV420));
   assert(gl2_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_YUV420_STRIDED)
         == EXPECT_STRIDED);
   uploads = 0;
   gl_planar_plane(0, 1, 5, 4, pixels, 8);
   assert(uploads == (EXPECT_STRIDED ? 1u : 4u));
   uploads = 0;
   gl_planar_plane(0, 1, 5, 4, pixels, 5);
   assert(uploads == 1);
   gl.flags = 0;
   assert(!gl2_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_YUV420_STRIDED));
   gl.flags = GL2_FLAG_HAVE_FBO; gl.planar.flags = GL_PLANAR_FAILED;
   assert(!gl2_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_YUV420_STRIDED));
   puts("PASS: plane-stride capability matches real upload call counts");
   return 0;
}
'''
text = fixture.replace("@@ROW_LENGTH@@", row_length).replace("FUNCTIONS", "\n".join((
    function(gl2, "static bool gl2_supports_texture_format("),
    function(common, "static void gl_planar_plane("))))
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="planar-strides-") as directory:
    src = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if "mingw" in cc[0] else "test")
    src.write_text(text, encoding="utf-8")
    for defines, expected in (([], 1), (["HAVE_OPENGLES"], 0),
                              (["HAVE_OPENGLES", "HAVE_OPENGLES3"], 1)):
        subprocess.run(cc + flags + ["-std=c89", "-O1", "-Wall", "-Wextra", "-Werror",
                       "-I" + str(root / "libretro-common/include"),
                       "-DEXPECT_STRIDED=" + str(expected)] +
                       ["-D" + define for define in defines] +
                       [str(src), "-o", str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
