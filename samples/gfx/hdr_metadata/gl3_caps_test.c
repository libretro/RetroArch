#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <formats/image.h>

#define CHECK(x) do { if (!(x)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

typedef struct
{
   struct
   {
      bool active;
      bool tex_fp16;
      unsigned ui_fbo;
      retro_atomic_int_t texture_scrgb_published;
   } scrgb;
   struct { bool source_hdr10; } video_info;
} gl3_t;

static bool gl_query_extension(const char *name)
{
   return strcmp(name, "EXT_texture_compression_s3tc") == 0;
}

#include "gl3.inc"

static void rebuild(void *data)
{
   gl3_t *gl = (gl3_t*)data;
   unsigned i;
   for (i = 0; i < 100000; i++)
   {
      gl->scrgb.active = (i & 1) != 0;
      gl->scrgb.tex_fp16 = (i & 2) != 0;
      gl->video_info.source_hdr10 = (i & 4) != 0;
      gl->scrgb.ui_fbo = (i & 8) ? 7 : 0;
      gl3_publish_texture_formats(gl);
   }
}

int main(void)
{
   gl3_t gl;
   unsigned i;
   sthread_t *worker;
   volatile unsigned observed = 0;
   memset(&gl, 0, sizeof(gl));
   CHECK(!gl3_supports_texture_format(NULL, TEXTURE_GPU_FORMAT_SCRGB));
   for (i = 0; i < 16; i++)
   {
      bool expected;
      gl.scrgb.active = (i & 1) != 0;
      gl.scrgb.tex_fp16 = (i & 2) != 0;
      gl.video_info.source_hdr10 = (i & 4) != 0;
      gl.scrgb.ui_fbo = (i & 8) ? 7 : 0;
      expected = gl.scrgb.active && (gl.video_info.source_hdr10
            ? gl.scrgb.ui_fbo != 0 : gl.scrgb.tex_fp16);
      gl3_publish_texture_formats(&gl);
      CHECK(gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_SCRGB)
            == expected);
   }
   CHECK(gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_RGBA16F));
   CHECK(gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_RGB10A2));
   CHECK(!gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_YUV420));
   CHECK(gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_BC1));
   CHECK(!gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_BC7));
   worker = sthread_create(rebuild, &gl);
   CHECK(worker);
   for (i = 0; i < 200000; i++)
      observed += gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_SCRGB);
   sthread_join(worker);
   gl.scrgb.active = false;
   gl3_publish_texture_formats(&gl);
   CHECK(!gl3_supports_texture_format(&gl, TEXTURE_GPU_FORMAT_SCRGB));
   printf("PASS: published GLCore scRGB capability; %u observations\n", observed);
   return 0;
}
