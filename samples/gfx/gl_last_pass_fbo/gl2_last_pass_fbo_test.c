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
#define VIEWER_PRESERVE gl2_media_shader_preserve
#define VIEWER_FREE gl2_media_shader_free
#include "viewer_cases.h"

/* Full driver integration: public handle resolution, accepted updates,
 * shader input selection and both levels of producer/history cleanup. */
static unsigned pool_width[1024],pool_height[1024],pool_deleted[1024];
static gl2_t *pool_driver;
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
   gl2_t *gl=(gl2_t*)calloc(1,sizeof(*gl));shader_backend_t backend;
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
   gl->shader=&backend;gl->shader_data=&backend;gl->flags|=GL2_FLAG_HAVE_FBO;
   gl->media_shader_enabled=true;gl->media_last_texture=999;pool_driver=gl;
   pool_width[999]=320;pool_height[999]=240;viewer_pixels[999]=42;
   memset(&image,0,sizeof(image));image.width=320;image.height=240;
   for(j=0;j<3;j++)CHECK(gfx_display_gl2_media_shader_draw(gl,999,VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(800,600),0,VIDEO_SCALE_PACK(800,600)),"public source rendered");
   for(frame=1;frame<=32;frame++)
   {
      GLuint readers[2];unsigned values[2];
      for(j=0;j<2;j++){readers[j]=gl->media_shader->history[gl->media_shader->history_order[j]].source->texture;values[j]=viewer_pixels[readers[j]];}
      image.pixels=&frame;before=gl11_draws;
      if(frame==1)
      {
         pool_fail=true;
         CHECK(gl2_update_texture_internal(gl,999,&image)==VIDEO_TEXTURE_UPDATE_DROPPED,"allocation failure drops without changing the public source");
         CHECK(gl_media_pool_resolve(gl->media_pool,999)==999 && viewer_pixels[999]==42,"dropped update preserves current pixels");
         pool_fail=false;
      }
      CHECK(gl2_update_texture_internal(gl,999,&image)==VIDEO_TEXTURE_UPDATE_DONE,"pooled driver update accepted");
      CHECK(!viewer_raw_copies && gl11_draws==before,"update adds no preservation copy or draw");
      for(j=0;j<2;j++)CHECK(viewer_pixels[readers[j]]==values[j],"native history readers remain immutable");
      CHECK(viewer_pixels[gl_media_pool_resolve(gl->media_pool,999)]==frame,"stable public handle resolves the new pixels");
      CHECK(gfx_display_gl2_media_shader_draw(gl,999,VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(800,600),0,VIDEO_SCALE_PACK(800,600)),"resolved public source rendered");
      if(frame==8)names=gl->media_pool->count;
      if(frame>8)CHECK(gl->media_pool->count==names,"warm pool needs no new storage");
   }
   count=gl->media_pool->count;for(j=0;j<count;j++)versions[j]=gl->media_pool->versions[j];
   gl2_unload_texture(gl,false,999);CHECK(!gl->media_pool,"unload removes public alias registry");
   gl2_media_shader_free(&gl->media_shader);
   for(j=0;j<count;j++)CHECK(pool_deleted[versions[j]]==1,"every retired pool version deleted exactly once");
   free(gl);pool_driver=NULL;
   gl11_texture_info=NULL;gl11_allocate_texture=NULL;gl11_upload_texture=NULL;gl11_delete_texture=NULL;gl11_copy_texture=NULL;
}

int main(void)
{
   if (viewer_cases() || viewer_borrow_cases()) return 1;
   run_pool_cases();
   if(fails)return 1;
   return run_cases(run_chain, -1);
}
