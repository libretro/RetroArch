/* gfx/drivers/gl2.c's GLSL/Cg render chain against a scripted GL and
 * shader backend (gl_last_pass_fakes.h has the cases): the last pass
 * gets an FBO only when drawing it straight to the backbuffer would give
 * a different image, and a one-pass preset that needs no FBO sets up no
 * chain at all. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/gl2.c"
#include "gl_last_pass_fakes.h"

/* The FBO count, or -1 for no chain. */
static int run_chain(void)
{
   static shader_backend_t       backend;
   static gl2_renderchain_data_t chain;
   static gl2_t                  gl;

   backend_init(&backend);
   memset(&chain, 0, sizeof(chain));
   memset(&gl, 0, sizeof(gl));
   gl.shader           = &backend;
   gl.renderchain_data = &chain;
   gl.flags            = GL2_FLAG_HAVE_FBO;
   gl.tex_dims         = VIDEO_SCALE_PACK(256, 256);
   gl.video_dims       = VIDEO_SCALE_PACK(320, 240);
   gl.vp.dims          = VIDEO_SCALE_PACK(1280, 960);

   gl2_renderchain_init(&gl, &chain);

   if (!(gl.flags & GL2_FLAG_FBO_INITED))
      return -1;
   return chain.fbo_pass;
}

static void viewer_uploaded(gl_media_shader_t *state)
{
   gl2_t *gl = (gl2_t*)calloc(1, sizeof(*gl));
   struct texture_image image;
   memset(&image, 0, sizeof(image));
   image.width = 320;
   image.height = 240;
   gl->media_shader = state;
   CHECK(gl2_update_texture_internal(gl, 999, &image) == VIDEO_TEXTURE_UPDATE_DONE,
         "managed source updated in place");
   CHECK(gl->media_last_texture == 999, "uploaded source is managed");
   free(gl);
}

static void viewer_unloaded(gl_media_shader_t *state)
{
   gl2_t *gl = (gl2_t*)calloc(1, sizeof(*gl));
   gl->media_shader = state;
   gl->media_last_texture = 999;
   gl2_unload_texture(gl, false, 999);
   CHECK(!gl->media_last_texture, "deleted names cannot remain managed");
   free(gl);
}

#define VIEWER_DRAW gl2_media_shader_draw
#define VIEWER_FREE gl2_media_shader_free
#include "viewer_cases.h"

int main(void)
{
   if (viewer_cases()) return 1;
   return run_cases(run_chain, -1);
}
