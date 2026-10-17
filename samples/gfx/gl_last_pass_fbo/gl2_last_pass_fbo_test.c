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


static unsigned ring_waits, ring_deletes;
static bool ring_alloc_fails;
static void APIENTRY ring_wait(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
   CHECK(sync == (GLsync)(uintptr_t)1 && !flags && timeout == GL_TIMEOUT_IGNORED,
         "HW ring queues the source dependency");
   ring_waits++;
}
static GLsync APIENTRY ring_fence(GLenum condition, GLbitfield flags)
{
   (void)condition; (void)flags;
   return ring_alloc_fails ? NULL : (GLsync)(uintptr_t)2;
}
static void APIENTRY ring_delete(GLsync sync)
{
   CHECK(sync == (GLsync)(uintptr_t)1, "producer retires the consumed source fence");
   ring_deletes++;
}
static void run_ring_cases(void)
{
   gl2_t gl;
   void *old;
   memset(&gl, 0, sizeof(gl));
   gl.textures = 3; gl.flags = GL2_FLAG_HAVE_SYNC;
   __rglgen_glWaitSync = ring_wait;
   __rglgen_glFenceSync = ring_fence;
   __rglgen_glDeleteSync = ring_delete;
   GL_HW_RING_SYNC_SWAP(&gl.hw_ring_sync[1], (void*)(uintptr_t)1, old);
   (void)old;
   CHECK(gl2_hw_ring_present_slot(&gl, 1), "HW frame presented");
   CHECK(ring_waits == 1 && !ring_deletes, "video thread never deletes the source fence");
   CHECK(gl2_hw_ring_present_slot(&gl, 1), "duplicate reuses HW frame");
   CHECK(ring_waits == 1 && !ring_deletes, "duplicate does not consume the fence twice");
   ring_alloc_fails = true;
   CHECK(!gl2_hw_ring_capture(&gl, 1, NULL, 0), "failed source fence refuses publication");
   CHECK(!ring_deletes, "failed capture retains returned fence");
   ring_alloc_fails = false;
   CHECK(gl2_hw_ring_capture(&gl, 1, NULL, 0), "producer captures the reused slot");
   CHECK(ring_deletes == 1, "producer retires returned fence once");
}

int main(void)
{
   int result = run_cases(run_chain, -1);
   run_ring_cases();
   return result || fails;
}
