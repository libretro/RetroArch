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

static void viewer_uploaded(gl_media_shader_t *state)
{
   gl3_t *gl = (gl3_t*)calloc(1, sizeof(*gl));
   struct texture_image image;
   memset(&image, 0, sizeof(image));
   image.width = 320;
   image.height = 240;
   gl->media_legacy_shader = state;
   CHECK(gl3_update_texture_internal(gl, 999, &image) == VIDEO_TEXTURE_UPDATE_DONE,
         "managed source updated in place");
   CHECK(gl->media_last_texture == 999, "uploaded source is managed");
   free(gl);
}

static void viewer_unloaded(gl_media_shader_t *state)
{
   gl3_t *gl = (gl3_t*)calloc(1, sizeof(*gl));
   gl->media_legacy_shader = state;
   gl->media_last_texture = 999;
   gl3_unload_texture(gl, false, 999);
   CHECK(!gl->media_last_texture, "deleted names cannot remain managed");
   free(gl);
}

#define VIEWER_DRAW gl3_media_shader_draw
#define VIEWER_PRESERVE gl3_media_shader_preserve
#define VIEWER_FREE gl3_media_shader_free
#include "viewer_cases.h"

/* Full driver integration: public handle resolution, accepted updates,
 * shader input selection and both levels of producer/history cleanup. */
static unsigned pool_width[1024],pool_height[1024],pool_deleted[1024];
static gl3_t *pool_driver;
static bool pool_fail;
static void pool_info(GLuint image,GLint level,GLenum name,GLint *out)
{
   CHECK(image<1024,"pool name remains bounded");
   *out=level?0:name==GL_TEXTURE_WIDTH?(GLint)pool_width[image]:name==GL_TEXTURE_HEIGHT?(GLint)pool_height[image]:GL_RGBA8;
}
static void pool_allocate(GLuint image,GLint format,GLsizei w,GLsizei h)
{(void)format;CHECK(image<1024,"new pool storage is bounded");pool_width[image]=pool_fail?0:w;pool_height[image]=pool_fail?0:h;}
static void pool_upload(GLuint image,const void *pixels)
{CHECK(image<1024 && pixels,"driver uploads once into the selected version");viewer_pixels[image]=*(const unsigned*)pixels;}
static void pool_delete(GLuint image)
{CHECK(image<1024 && !pool_deleted[image],"native texture deleted once");pool_deleted[image]++;}
static void pool_params(void *params,void *data)
{
   video_shader_ctx_params_t *p=(video_shader_ctx_params_t*)params;
   const struct video_tex_info *original=(const struct video_tex_info*)p->info;
   (void)data;CHECK(original->tex==gl_media_pool_resolve(pool_driver->media_pool,999),"shader receives resolved storage, not stale public storage");
}
static void run_pool_cases(void)
{
   gl3_t *gl=(gl3_t*)calloc(1,sizeof(*gl));shader_backend_t backend;
   struct texture_image image;unsigned frame,j,names=0,before;
   GLuint versions[GFX_MAX_FRAME_HISTORY+2];unsigned count;
   gl_fakes_install();backend_init(&backend);gl11_next_name=1;
   memset(pool_width,0,sizeof(pool_width));memset(pool_height,0,sizeof(pool_height));memset(pool_deleted,0,sizeof(pool_deleted));
   __rglgen_glActiveTexture=viewer_active_texture;__rglgen_glBindBuffer=viewer_lease_bind_buffer;
   gl11_texture_info=pool_info;gl11_allocate_texture=pool_allocate;gl11_upload_texture=pool_upload;gl11_delete_texture=pool_delete;
   gl11_copy_texture=viewer_lease_copy;viewer_raw_copies=0;viewer_pixel_state=NULL;
   backend.use=viewer_use;backend.set_params=pool_params;backend.set_coords=viewer_coords;backend.set_mvp=viewer_mvp;
   backend.get_prev_textures=viewer_history;backend.set_media_colour=viewer_colour;
   viewer_history_slots=2;num_passes=1;feedback_pass=-1;memset(scales,0,sizeof(scales));
   gl->chain.active=true;gl->chain.shader=&backend;gl->chain.shader_data=&backend;
   gl->media_shader_enabled=true;gl->media_last_texture=999;pool_driver=gl;
   pool_width[999]=320;pool_height[999]=240;viewer_pixels[999]=42;
   memset(&image,0,sizeof(image));image.width=320;image.height=240;
   for(j=0;j<3;j++)CHECK(gfx_display_gl3_media_shader_draw(gl,999,VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(800,600),0,VIDEO_SCALE_PACK(800,600)),"public source rendered");
   for(frame=1;frame<=32;frame++)
   {
      GLuint readers[2];unsigned values[2];
      for(j=0;j<2;j++){readers[j]=gl->media_legacy_shader->history[gl->media_legacy_shader->history_order[j]].source->texture;values[j]=viewer_pixels[readers[j]];}
      image.pixels=&frame;before=gl11_draws;
      if(frame==1)
      {
         pool_fail=true;
         CHECK(gl3_update_texture_internal(gl,999,&image)==VIDEO_TEXTURE_UPDATE_DROPPED,"allocation failure drops without changing the public source");
         CHECK(gl_media_pool_resolve(gl->media_pool,999)==999 && viewer_pixels[999]==42,"dropped update preserves current pixels");
         pool_fail=false;
      }
      CHECK(gl3_update_texture_internal(gl,999,&image)==VIDEO_TEXTURE_UPDATE_DONE,"pooled driver update accepted");
      CHECK(!viewer_raw_copies && gl11_draws==before,"update adds no preservation copy or draw");
      for(j=0;j<2;j++)CHECK(viewer_pixels[readers[j]]==values[j],"native history readers remain immutable");
      CHECK(viewer_pixels[gl_media_pool_resolve(gl->media_pool,999)]==frame,"stable public handle resolves the new pixels");
      CHECK(gfx_display_gl3_media_shader_draw(gl,999,VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(800,600),0,VIDEO_SCALE_PACK(800,600)),"resolved public source rendered");
      if(frame==8)names=gl->media_pool->count;
      if(frame>8)CHECK(gl->media_pool->count==names,"warm pool needs no new storage");
   }
   count=gl->media_pool->count;for(j=0;j<count;j++)versions[j]=gl->media_pool->versions[j];
   gl3_unload_texture(gl,false,999);CHECK(!gl->media_pool,"unload removes public alias registry");
   gl3_media_shader_free(&gl->media_legacy_shader);
   for(j=0;j<count;j++)CHECK(pool_deleted[versions[j]]==1,"every retired pool version deleted exactly once");
   free(gl);pool_driver=NULL;
   gl11_texture_info=NULL;gl11_allocate_texture=NULL;gl11_upload_texture=NULL;gl11_delete_texture=NULL;gl11_copy_texture=NULL;
}

int main(void)
{
   int result = viewer_cases() || viewer_borrow_cases();
   run_pool_cases();
   result |= run_cases(run_chain, 0);
   run_readback_cases();
   run_fence_cases();
   run_resize_cases();
   return result || fails;
}
