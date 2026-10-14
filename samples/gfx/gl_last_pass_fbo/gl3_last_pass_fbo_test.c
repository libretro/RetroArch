/* gfx/drivers/gl3.c's GLSL/Cg render chain against a scripted GL and
 * shader backend (gl_last_pass_fakes.h has the cases): the last pass
 * gets an FBO only when drawing it straight to the backbuffer would give
 * a different image, and a one-pass preset that needs no FBO is drawn
 * with none. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/gl3.c"
#include "gl_last_pass_fakes.h"

/* What the chain asks of the frontend; neither changes its FBOs. */
unsigned retroarch_get_rotation(void) { return 0; }
bool video_driver_is_hw_context(void) { return false; }

/* The FBO count. */
static int run_chain(void)
{
   static shader_backend_t backend;
   static gl3_t            gl;

   backend_init(&backend);
   memset(&gl, 0, sizeof(gl));
   gl.chain.shader            = &backend;
   gl.video_info.input_scale  = 1;
   gl.video_dims              = VIDEO_SCALE_PACK(1280, 960);

   if (!gl3_create_fbo_targets(&gl))
      return -2;
   return (int)gl.chain.num_fbo_passes;
}

int main(void)
{
   return run_cases(run_chain, 0);
}
