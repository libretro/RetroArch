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
static size_t pack_capacity[32];
static unsigned oversized_reads;
static GLenum sync_status[128];
static bool sync_live[128];
static unsigned sync_created, sync_deleted;
static bool sync_ready, fence_fails;

static GLsync APIENTRY readback_fence(GLenum condition, GLbitfield flags)
{
   unsigned id;
   (void)condition; (void)flags;
   if (fence_fails)
      return NULL;
   id = ++sync_created;
   if (id >= 128)
      abort();
   sync_live[id] = true;
   sync_status[id] = sync_ready ? GL_ALREADY_SIGNALED : GL_TIMEOUT_EXPIRED;
   return (GLsync)(uintptr_t)id;
}

static GLenum APIENTRY readback_wait(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
   unsigned id = (unsigned)(uintptr_t)sync;
   if (flags || timeout || !sync_live[id])
      abort();
   return sync_status[id];
}

static void APIENTRY readback_delete_sync(GLsync sync)
{
   unsigned id = (unsigned)(uintptr_t)sync;
   if (!sync_live[id])
      abort();
   sync_live[id] = false;
   sync_deleted++;
}

static void APIENTRY readback_delete_buffers(GLsizei count, const GLuint *buffers)
{
   GLsizei i;
   for (i = 0; i < count; i++)
      if (buffers[i] < 32)
         pack_capacity[buffers[i]] = 0;
}

static void APIENTRY readback_gen_buffers(GLsizei count, GLuint *buffers)
{
   GLsizei i;
   for (i = 0; i < count; i++)
      buffers[i] = 7 + i;
}

static void APIENTRY readback_buffer_data(GLenum target, GLsizeiptr size,
      const void *data, GLenum usage)
{
   (void)target; (void)data; (void)usage;
   if (pack_binding < 32)
      pack_capacity[pack_binding] = size;
}

static void readback_extent(GLsizei width, GLsizei height)
{
   if (pack_binding >= 32
         || (size_t)width * height * 4 > pack_capacity[pack_binding])
      oversized_reads++;
}

static void queue_readback(gl3_t *gl)
{
   unsigned index = gl->pbo_readback_index;
   gl3_pbo_async_readback(gl);
   gl->pbo_readback_index = index;
}

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
   __rglgen_glFenceSync = readback_fence;
   __rglgen_glClientWaitSync = readback_wait;
   __rglgen_glDeleteSync = readback_delete_sync;
   __rglgen_glDeleteBuffers = readback_delete_buffers;
   __rglgen_glGenBuffers = readback_gen_buffers;
   __rglgen_glBufferData = readback_buffer_data;
   sync_ready = true;
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
         queue_readback(&gl);
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
            queue_readback(&gl);
            gl.pbo_readback_valid[0] = true;
            CHECK(gl3_read_viewport_internal(&gl, out, true, true),
                  "successful map delivers BGRX");
            CHECK(!memcmp(out, readback_pixels, sizeof(out)), "BGRX pixels preserved");
            CHECK(unmap_calls == 1 && pack_binding == 0, "successful map unmapped once");
            unmap_result = GL_FALSE;
            queue_readback(&gl);
            gl.pbo_readback_valid[0] = true;
            CHECK(!gl3_read_viewport_internal(&gl, out, true, true),
                  "failed unmap rejects corrupted frame");
            CHECK(unmap_calls == 2 && pack_binding == 0, "failed unmap restores binding");
         }
         gl3_deinit_pbo_readback(&gl);
      }
}

static void run_fence_cases(void)
{
   gl3_t gl;
   uint8_t out[32], canary[32];
   unsigned i, created, deleted;
   memset(&gl, 0, sizeof(gl));
   gl.flags = GL3_FLAG_PBO_READBACK_ENABLE;
   gl.vp.dims = VIDEO_SCALE_PACK(4, 2);
   gl.pbo_readback_scaler.in_width = 4;
   gl.pbo_readback_scaler.in_height = 2;
   for (i = 0; i < GL_CORE_NUM_PBOS; i++)
      gl.pbo_readback[i] = 7 + i;
   sync_ready = false;
   map_fails = false;
   unmap_result = GL_TRUE;
   map_calls = 0;
   created = sync_created;
   for (i = 0; i < GL_CORE_NUM_PBOS; i++)
      gl3_pbo_async_readback(&gl);
   CHECK(sync_created == created + GL_CORE_NUM_PBOS, "each recording slot fenced");
   memset(out, 0xa5, sizeof(out));
   memcpy(canary, out, sizeof(out));
   CHECK(!gl3_read_viewport_internal(&gl, out, true, true), "pending recording slot skipped");
   CHECK(map_calls == 0 && !memcmp(out, canary, sizeof(out)), "pending slot never mapped");
   CHECK(gl.pbo_readback_valid[0], "pending recording slot retained");
   gl3_pbo_async_readback(&gl);
   CHECK(sync_created == created + GL_CORE_NUM_PBOS && gl.pbo_readback_index == 0,
         "pending slot not overwritten or advanced");
   sync_status[created + 1] = GL_CONDITION_SATISFIED;
   CHECK(gl3_read_viewport_internal(&gl, out, true, true), "completed slot delivers pixels");
   CHECK(map_calls == 1 && !memcmp(out, readback_pixels, sizeof(out)), "completed pixels preserved");
   CHECK(!gl3_read_viewport_internal(&gl, out, true, true), "completed slot consumed once");
   gl3_pbo_async_readback(&gl);
   CHECK(sync_created == created + GL_CORE_NUM_PBOS + 1, "consumed slot reused with fresh fence");
   sync_status[created + 2] = GL_WAIT_FAILED;
   CHECK(!gl3_read_viewport_internal(&gl, out, true, true), "failed completion query never maps");
   CHECK(map_calls == 1, "failed completion query preserves map count");
   gl3_pbo_async_readback(&gl);
   CHECK(gl.pbo_readback_index == 1, "failed completion query never reuses destination");
   deleted = sync_deleted;
   gl3_deinit_pbo_readback(&gl);
   CHECK(sync_deleted == deleted + GL_CORE_NUM_PBOS, "teardown retires all remaining fences");
   CHECK(gl.pbo_readback_index == 0 && !gl.pbo_readback_valid[0], "teardown clears recording state");
   gl3_deinit_pbo_readback(&gl);
   CHECK(sync_deleted == deleted + GL_CORE_NUM_PBOS, "repeated teardown does not delete twice");
   sync_ready = true;
   CHECK(gl3_init_pbo_readback(&gl), "recording ring restarts after teardown");
   queue_readback(&gl);
   deleted = sync_deleted;
   queue_readback(&gl);
   CHECK(sync_deleted == deleted + 1, "replacement retires completed unread fence");
   gl.flags |= GL3_FLAG_GPU_RECORDING;
   gl.vp.dims = VIDEO_SCALE_PACK(2, 4);
   CHECK(!gl3_read_viewport_internal(&gl, out, true, true), "resized ring has no stale frame");
   CHECK(sync_deleted == deleted + 2, "resize retires old recording fence");
   CHECK(gl.pbo_readback_scaler.in_width == 2 && gl.pbo_readback_scaler.in_height == 4,
         "resize reinitializes recording dimensions");
   gl3_deinit_pbo_readback(&gl);
   fence_fails = true;
   gl.flags |= GL3_FLAG_PBO_READBACK_ENABLE;
   gl.pbo_readback[0] = 7;
   gl3_pbo_async_readback(&gl);
   CHECK(!(gl.flags & GL3_FLAG_PBO_READBACK_ENABLE), "fence allocation failure disables unsafe ring");
   fence_fails = false;
   CHECK(sync_created == sync_deleted, "all recording fences retired");
}

static void run_resize_cases(void)
{
   gl3_t gl;
   unsigned deleted;
   memset(&gl, 0, sizeof(gl));
   gl.flags = GL3_FLAG_PBO_READBACK_ENABLE;
   gl.vp.dims = VIDEO_SCALE_PACK(4, 2);
   sync_ready = false;
   CHECK(gl3_init_pbo_readback(&gl), "resize source ring initialized");
   gl11_read_pixels = readback_extent;
   gl3_pbo_async_readback(&gl);
   deleted = sync_deleted;
   gl.vp.dims = VIDEO_SCALE_PACK(8, 4);
   gl3_pbo_async_readback(&gl);
   CHECK(!oversized_reads, "growth resizes PBO before issuing readback");
   CHECK(sync_deleted == deleted + 1, "growth retires old pending fence");
   CHECK(gl.pbo_readback_scaler.in_width == 8 && gl.pbo_readback_scaler.in_height == 4,
         "growth configures new recording extent before read");
   CHECK(pack_capacity[7] == 8 * 4 * 4, "growth allocates matching PBO capacity");
   deleted = sync_deleted;
   gl.vp.dims = VIDEO_SCALE_PACK(2, 2);
   gl3_pbo_async_readback(&gl);
   CHECK(sync_deleted == deleted + 1, "shrink retires pending recording fence");
   CHECK(gl.pbo_readback_scaler.in_width == 2 && gl.pbo_readback_scaler.in_height == 2,
         "shrink configures new extent before read");
   CHECK(pack_capacity[7] == 2 * 2 * 4 && !oversized_reads, "shrink capacity matches extent");
   gl3_deinit_pbo_readback(&gl);
   gl11_read_pixels = NULL;
   CHECK(sync_created == sync_deleted, "resize teardown retires every recording fence");
}

int main(void)
{
   int result = run_cases(run_chain, 0);
   run_readback_cases();
   run_fence_cases();
   run_resize_cases();
   return result || fails;
}
