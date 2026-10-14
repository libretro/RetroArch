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

static GLuint pack_binding;
static unsigned map_calls, unmap_calls, context_calls;
static bool map_fails, core_bound;
static GLboolean unmap_result;
static uint32_t readback_pixels[8];

static void APIENTRY readback_bind(GLenum target, GLuint buffer)
{
   (void)target;
   pack_binding = buffer;
}

static void *APIENTRY readback_map(GLenum target, GLintptr offset,
      GLsizeiptr length, GLbitfield access)
{
   (void)target; (void)offset; (void)length; (void)access;
   map_calls++;
   return map_fails ? NULL : readback_pixels;
}

static GLboolean APIENTRY readback_unmap(GLenum target)
{
   (void)target;
   unmap_calls++;
   return unmap_result;
}

static void readback_context(void *data, bool enable)
{
   (void)data;
   core_bound = enable;
   context_calls++;
}

static void run_readback_cases(void)
{
   gl3_t gl;
   gfx_ctx_driver_t context;
   uint8_t out[32], canary[32];
   unsigned bgrx, mains;
   memset(&context, 0, sizeof(context));
   context.bind_hw_render = readback_context;
   __rglgen_glBindBuffer = readback_bind;
   __rglgen_glMapBufferRange = readback_map;
   __rglgen_glUnmapBuffer = readback_unmap;
   memset(readback_pixels, 0x37, sizeof(readback_pixels));
   memset(canary, 0xa5, sizeof(canary));
   for (bgrx = 0; bgrx < 2; bgrx++)
      for (mains = 0; mains < 2; mains++)
      {
         memset(&gl, 0, sizeof(gl));
         gl.ctx_driver = &context;
         gl.flags = GL3_FLAG_PBO_READBACK_ENABLE | GL3_FLAG_USE_SHARED_CONTEXT;
         if (mains)
            gl.flags |= GL3_FLAG_HW_RING;
         gl.vp.dims = VIDEO_SCALE_PACK(4, 2);
         gl.pbo_readback_scaler.in_width = 4;
         gl.pbo_readback_scaler.in_height = 2;
         gl.pbo_readback[0] = 7;
         gl.pbo_readback_valid[0] = true;
         map_fails = true;
         map_calls = unmap_calls = context_calls = 0;
         unmap_result = GL_TRUE;
         memcpy(out, canary, sizeof(out));
         CHECK(!gl3_read_viewport_internal(&gl, out, true, bgrx),
               "failed PBO map rejects the frame");
         CHECK(!memcmp(out, canary, sizeof(out)), "failed map preserves destination");
         CHECK(map_calls == 1 && unmap_calls == 0, "failed map is never unmapped");
         CHECK(pack_binding == 0, "failed map restores pack binding");
         CHECK(!gl.pbo_readback_valid[0], "failed map retires the slot");
         CHECK(context_calls == (mains ? 1 : 2) && core_bound == !mains,
               "failed map restores the correct context ownership");
         if (bgrx)
         {
            map_fails = false;
            gl.pbo_readback_valid[0] = true;
            CHECK(gl3_read_viewport_internal(&gl, out, true, true),
                  "successful map delivers BGRX");
            CHECK(!memcmp(out, readback_pixels, sizeof(out)), "BGRX pixels preserved");
            CHECK(unmap_calls == 1 && pack_binding == 0, "successful map unmapped once");
            unmap_result = GL_FALSE;
            gl.pbo_readback_valid[0] = true;
            CHECK(!gl3_read_viewport_internal(&gl, out, true, true),
                  "failed unmap rejects corrupted frame");
            CHECK(unmap_calls == 2 && pack_binding == 0, "failed unmap restores binding");
         }
      }
}

int main(void)
{
   int result = run_cases(run_chain, 0);
   run_readback_cases();
   return result || fails;
}
