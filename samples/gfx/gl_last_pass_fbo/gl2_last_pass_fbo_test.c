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

int main(void)
{
   return run_cases(run_chain, -1);
}
