/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2020 Google
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <retro_inline.h>
#include <retro_math.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../common/rsx_common.h"
#include "../font_driver.h"

#include "../../configuration.h"
#include "../../command.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifndef HAVE_THREADS
#include "../../tasks/tasks_internal.h"
#endif

#include <defines/ps3_defines.h>

#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <ppu-types.h>

#define CB_SIZE		0x100000
#define HOST_SIZE	(32*1024*1024)

#include <ppu-lv2.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <time.h>
#include <math.h>

#define rsx_context_bind_hw_render(rsx, enable) \
   if (rsx->shared_context_use) \
      rsx->ctx_driver->bind_hw_render(rsx->ctx_data, enable)

static void rsx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate);
static void rsx_load_texture_data(rsx_t* rsx, rsx_texture_t *texture,
      const void *frame, unsigned width, unsigned height, unsigned pitch,
      bool rgb32, bool menu, enum texture_filter_type filter_type);

static const gfx_ctx_driver_t* rsx_get_context(rsx_t* rsx)
{
   const gfx_ctx_driver_t* gfx_ctx = NULL;
   void* ctx_data = NULL;
   settings_t* settings = config_get_ptr();
   struct retro_hw_render_callback* hwr = video_driver_get_hw_context();
   
   bool video_shared_context = settings->bools.video_shared_context;
   enum gfx_ctx_api api = GFX_CTX_RSX_API;

   rsx->shared_context_use = video_shared_context && hwr->context_type != RETRO_HW_CONTEXT_NONE;
   
   if ((libretro_get_shared_context())
      && (hwr->context_type != RETRO_HW_CONTEXT_NONE))
      rsx->shared_context_use = true;

   gfx_ctx = video_context_driver_init_first(rsx,
      settings->arrays.video_context_driver,
      api, 1, 0, rsx->shared_context_use, &ctx_data);
   
   if (ctx_data)
      rsx->ctx_data = ctx_data;

   return gfx_ctx;
}

#ifndef HAVE_THREADS
static bool rsx_tasks_finder(retro_task_t *task,void *userdata)
{
   return task;
}
task_finder_data_t rsx_tasks_finder_data = {rsx_tasks_finder, NULL};
#endif

static int rsx_make_buffer(rsxBuffer * buffer, u16 width, u16 height, int id)
{
   int depth = sizeof(u32);
   int pitch = depth * width;
   int size = depth * width * height;

   buffer->ptr = (uint32_t*)rsxMemalign (64, size);
   if (!buffer->ptr)
      goto error;

   if (rsxAddressToOffset (buffer->ptr, &buffer->offset) != 0)
      goto error;

   /* Register the display buffer with the RSX */
   if (gcmSetDisplayBuffer (id, buffer->offset, pitch, width, height) != 0)
      goto error;

   buffer->width = width;
   buffer->height = height;
   buffer->id = id;

   return TRUE;

error:
   if (buffer->ptr)
      rsxFree (buffer->ptr);

   return FALSE;
}

static int rsx_flip(gcmContextData *context, s32 buffer)
{
   if (gcmSetFlip(context, buffer) == 0)
   {
      rsxFlushBuffer (context);
      /* Prevent the RSX from continuing until the flip has finished. */
      gcmSetWaitFlip (context);

      return TRUE;
   }
   return FALSE;
}

#define GCM_LABEL_INDEX		255

static void rsx_wait_rsx_idle(gcmContextData *context);

static void rsx_wait_flip(void)
{
  while (gcmGetFlipStatus() != 0)
    usleep (200);  /* Sleep, to not stress the cpu. */
  gcmResetFlipStatus();
}

static gcmContextData *rsx_init_screen(rsx_t* gcm)
{
   /* Context to keep track of the RSX buffer. */
   gcmContextData              *context = NULL;
   static gcmContextData *saved_context = NULL;
   videoState state;
   videoConfiguration vconfig;
   videoResolution res; /* Screen Resolution */

   if (!saved_context)
   {
      /* Allocate a 1Mb buffer, alligned to a 1Mb boundary                          
       * to be our shared IO memory with the RSX. */
      void *host_addr = memalign (1024*1024, HOST_SIZE);

      if (!host_addr)
         goto error;

      /* Initialise Reality, which sets up the 
       * command buffer and shared I/O memory */
#ifdef NV40TCL_RENDER_ENABLE
      /* There was an api breakage on 2020-07-10, let's
       * workaround this by using one of the new defines */
      rsxInit (&context, CB_SIZE, HOST_SIZE, host_addr);
#else
      context = rsxInit (CB_SIZE, HOST_SIZE, host_addr);
#endif
      if (!context)
         goto error;
      saved_context = context;
   }
   else
      context = saved_context;

   /* Get the state of the display */
   if (videoGetState (0, 0, &state) != 0)
      goto error;

   /* Make sure display is enabled */
   if (state.state != 0)
      goto error;

   /* Get the current resolution */
   if (videoGetResolution (state.displayMode.resolution, &res) != 0)
      goto error;

   /* Configure the buffer format to xRGB */
   memset (&vconfig, 0, sizeof(videoConfiguration));
   vconfig.resolution = state.displayMode.resolution;
   vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
   vconfig.pitch      = res.width * sizeof(u32);
   vconfig.aspect     = state.displayMode.aspect;

   gcm->width         = res.width;
   gcm->height        = res.height;

   rsx_wait_rsx_idle(context);

   if (videoConfigure (0, &vconfig, NULL, 0) != 0)
      goto error;

   if (videoGetState (0, 0, &state) != 0)
      goto error;

   gcmSetFlipMode (GCM_FLIP_VSYNC); /* Wait for VSYNC to flip */

   gcm->depth_pitch = res.width * sizeof(u32);
   gcm->depth_buffer = (u32 *) rsxMemalign (64, (res.height * gcm->depth_pitch));  //Beware, if was (res.height * gcm->depth_pitch)*2
   
   rsxAddressToOffset (gcm->depth_buffer, &gcm->depth_offset);

   gcmResetFlipStatus();

   return context;

error:
#if 0
   if (context)
      rsxFinish (context, 0);

   if (gcm->host_addr)
      free (gcm->host_addr);
#endif

   return NULL;
}


static void waitFinish(gcmContextData *context, u32 sLabelVal)
{
  rsxSetWriteBackendLabel (context, GCM_LABEL_INDEX, sLabelVal);

  rsxFlushBuffer (context);

  while (*(vu32 *) gcmGetLabelAddress (GCM_LABEL_INDEX) != sLabelVal)
    usleep(30);

  sLabelVal++;
}

static void rsx_wait_rsx_idle(gcmContextData *context)
{
  u32 sLabelVal = 1;

  rsxSetWriteBackendLabel (context, GCM_LABEL_INDEX, sLabelVal);
  rsxSetWaitLabel (context, GCM_LABEL_INDEX, sLabelVal);

  sLabelVal++;

  waitFinish(context, sLabelVal);
}

static void rsx_init_render_target(rsx_t *rsx, rsxBuffer * buffer, int id)
{
   memset(&rsx->surface[id], 0, sizeof(gcmSurface));
   rsx->surface[id].colorFormat		= GCM_SURFACE_X8R8G8B8;
   rsx->surface[id].colorTarget		= GCM_SURFACE_TARGET_0;
   rsx->surface[id].colorLocation[0]	= GCM_LOCATION_RSX;
   rsx->surface[id].colorOffset[0]	= buffer->offset;
   rsx->surface[id].colorPitch[0]	= rsx->width * 4;
   for(u32 i=1; i < GCM_MAX_MRT_COUNT; i++) {
      rsx->surface[id].colorLocation[i]	= GCM_LOCATION_RSX;
      rsx->surface[id].colorOffset[i]		= buffer->offset;
      rsx->surface[id].colorPitch[i]		= 64;
   }
   rsx->surface[id].depthFormat		= GCM_SURFACE_ZETA_Z24S8;
   rsx->surface[id].depthLocation	= GCM_LOCATION_RSX;
   rsx->surface[id].depthOffset		= rsx->depth_offset;
   rsx->surface[id].depthPitch		= rsx->width * 4;
   rsx->surface[id].type			= GCM_SURFACE_TYPE_LINEAR;
   rsx->surface[id].antiAlias		= GCM_SURFACE_CENTER_1;
   rsx->surface[id].width			= rsx->width;
   rsx->surface[id].height			= rsx->height;
   rsx->surface[id].x				= 0;
   rsx->surface[id].y				= 0;
}

static void rsx_init_vertices(rsx_t *rsx)
{
   rsx->vertices = (rsx_vertex_t *)rsxMemalign(128, sizeof(rsx_vertex_t) * 4);

   rsx->vertices[0].x = 0.0f;
   rsx->vertices[0].y = 0.0f;
   rsx->vertices[0].z = 0.0f;
   rsx->vertices[0].u = 0.0f;
   rsx->vertices[0].v = 1.0f;
   rsx->vertices[0].r = 1.0f;
   rsx->vertices[0].g = 1.0f;
   rsx->vertices[0].b = 1.0f;
   rsx->vertices[0].a = 1.0f;

   rsx->vertices[1].x = 1.0f;
   rsx->vertices[1].y = 0.0f;
   rsx->vertices[1].z = 0.0f;
   rsx->vertices[1].u = 1.0f;
   rsx->vertices[1].v = 1.0f;
   rsx->vertices[1].r = 1.0f;
   rsx->vertices[1].g = 1.0f;
   rsx->vertices[1].b = 1.0f;
   rsx->vertices[1].a = 1.0f;

   rsx->vertices[2].x = 0.0f;
   rsx->vertices[2].y = 1.0f;
   rsx->vertices[2].z = 0.0f;
   rsx->vertices[2].u = 0.0f;
   rsx->vertices[2].v = 0.0f;
   rsx->vertices[2].r = 1.0f;
   rsx->vertices[2].g = 1.0f;
   rsx->vertices[2].b = 1.0f;
   rsx->vertices[2].a = 1.0f;

   rsx->vertices[3].x = 1.0f;
   rsx->vertices[3].y = 1.0f;
   rsx->vertices[3].z = 0.0f;
   rsx->vertices[3].u = 1.0f;
   rsx->vertices[3].v = 0.0f;
   rsx->vertices[3].r = 1.0f;
   rsx->vertices[3].g = 1.0f;
   rsx->vertices[3].b = 1.0f;
   rsx->vertices[3].a = 1.0f;

   rsxAddressToOffset(&rsx->vertices[0].x, &rsx->pos_offset);
   rsxAddressToOffset(&rsx->vertices[0].u, &rsx->uv_offset);
   rsxAddressToOffset(&rsx->vertices[0].r, &rsx->col_offset);
}

static void rsx_init_shader(rsx_t *rsx)
{
   u32 fpsize = 0;
   u32 vpsize = 0;
   rsx->vp_ucode = NULL;
   rsx->fp_ucode = NULL;
   rsx->vpo = (rsxVertexProgram *)vpshader_basic_vpo;
   rsx->fpo = (rsxFragmentProgram *)fpshader_basic_fpo;
   rsxVertexProgramGetUCode(rsx->vpo, &rsx->vp_ucode, &vpsize);
   rsxFragmentProgramGetUCode(rsx->fpo, &rsx->fp_ucode, &fpsize);
   rsx->fp_buffer = (u32 *)rsxMemalign(64, fpsize);
   if (!rsx->fp_buffer)
   {
      RARCH_LOG("failed to allocate fp_buffer\n");
      return;
   }
   memcpy(rsx->fp_buffer, rsx->fp_ucode, fpsize);
   rsxAddressToOffset(rsx->fp_buffer ,&rsx->fp_offset);

   rsx->proj_matrix = rsxVertexProgramGetConst(rsx->vpo, "modelViewProj");
   rsx->pos_index = rsxVertexProgramGetAttrib(rsx->vpo, "position");
   rsx->col_index = rsxVertexProgramGetAttrib(rsx->vpo, "color");
   rsx->uv_index = rsxVertexProgramGetAttrib(rsx->vpo, "texcoord");
   rsx->tex_unit = rsxFragmentProgramGetAttrib(rsx->fpo, "texture");
}

static void* rsx_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   int i;
   rsx_t* rsx = malloc(sizeof(rsx_t));

   if (!rsx)
      return NULL;

   memset(rsx, 0, sizeof(rsx_t));

   rsx->texture.data = NULL;
   rsx->menu_texture.data = NULL;

   rsx->context = rsx_init_screen(rsx);
   const gfx_ctx_driver_t* ctx_driver = rsx_get_context(rsx);

   if (!ctx_driver)
      return NULL;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);
   rsx->ctx_driver = ctx_driver;
   rsx->video_info = *video;

   for (i = 0; i < MAX_BUFFERS; i++)
   {
      rsx_make_buffer(&rsx->buffers[i], rsx->width, rsx->height, i);
      rsx_init_render_target(rsx, &rsx->buffers[i], i);
   }

#if defined(HAVE_MENU_BUFFER)
   for (i = 0; i < MAX_MENU_BUFFERS; i++)
   {
      rsx_make_buffer(&rsx->menuBuffers[i], rsx->width, rsx->height, i+MAX_BUFFERS);
      rsx_init_render_target(rsx, &rsx->menuBuffers[i], i+MAX_BUFFERS);
   }
#endif

   rsx->texture.height = rsx->height;
   rsx->texture.width = rsx->width;
   rsx->menu_texture.height = rsx->height;
   rsx->menu_texture.width = rsx->width;

   rsx_init_shader(rsx);
   rsx_init_vertices(rsx);

   rsx_flip(rsx->context, MAX_BUFFERS - 1);

   rsx->vp.x = 0;
   rsx->vp.y = 0;
   rsx->vp.width = rsx->width;
   rsx->vp.height = rsx->height;
   rsx->vp.full_width = rsx->width;
   rsx->vp.full_height = rsx->height;
   rsx->rgb32 = video->rgb32;
   video_driver_set_size(rsx->vp.width, rsx->vp.height);
   rsx_set_viewport(rsx, rsx->vp.width, rsx->vp.height, false, true);

   if (input && input_data)
   {
      void *ps3input       = input_driver_init_wrap(&input_ps3, ps3_joypad.ident);
      *input               = ps3input ? &input_ps3 : NULL;
      *input_data          = ps3input;
   }

   rsx_context_bind_hw_render(rsx, true);

   if (video->font_enable)
   {
      font_driver_init_osd(rsx,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_RSX);
      rsx->msg_rendering_enabled = true;
   }

   return rsx;
}

static void rsx_set_projection(rsx_t *rsx,
      struct video_ortho *ortho, bool allow_rotate)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;

   /* Calculate projection. */
   matrix_4x4_ortho(rsx->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      rsx->mvp = rsx->mvp_no_rot;
      return;
   }

   radians                 = M_PI * rsx->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(rsx->mvp, rot, rsx->mvp_no_rot);
}

static void rsx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   int x                     = 0;
   int y                     = 0;
   float device_aspect       = (float)viewport_width / viewport_height;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};
   settings_t *settings      = config_get_ptr();
   rsx_t *rsx        = (rsx_t*)data;
   bool video_scale_integer  = settings->bools.video_scale_integer;
   unsigned aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;
   rsx_viewport_t vp;

   if (video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&rsx->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(), rsx->keep_aspect);
      viewport_width  = rsx->vp.width;
      viewport_height = rsx->vp.height;
   }
   else if (rsx->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport *custom = video_viewport_get_custom();

         x               = custom->x;
         y               = custom->y;
         viewport_width  = custom->width;
         viewport_height = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta          = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x              = (int)roundf(viewport_width * (0.5f - delta));
            viewport_width = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            delta           = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y               = (int)roundf(viewport_height * (0.5f - delta));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      rsx->vp.x      = x;
      rsx->vp.y      = y;
      rsx->vp.width  = viewport_width;
      rsx->vp.height = viewport_height;
   }
   else
   {
      rsx->vp.x      = 0;
      rsx->vp.y      = 0;
      rsx->vp.width  = viewport_width;
      rsx->vp.height = viewport_height;
   }

   vp.min = 0.0f;
   vp.max = 1.0f;
   vp.x = rsx->vp.x;
   vp.y = rsx->height - rsx->vp.y - rsx->vp.height;
   vp.w = rsx->vp.width;
   vp.h = rsx->vp.height;
   vp.scale[0] = vp.w*0.5f;
   vp.scale[1] = vp.h*-0.5f;
   vp.scale[2] = (vp.max - vp.min)*0.5f;
   vp.scale[3] = 0.0f;
   vp.offset[0] = vp.x + vp.w*0.5f;
   vp.offset[1] = vp.y + vp.h*0.5f;
   vp.offset[2] = (vp.max + vp.min)*0.5f;
   vp.offset[3] = 0.0f;

   rsxSetViewport(rsx->context, vp.x, vp.y, vp.w, vp.h, vp.min, vp.max, vp.scale, vp.offset);
   for(int i = 0; i < 8; i++)
      rsxSetViewportClip(rsx->context, i, rsx->width, rsx->height);
   rsxSetScissor(rsx->context, vp.x, vp.y, vp.w, vp.h);

   rsx_set_projection(rsx, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      rsx->vp.width  = viewport_width;
      rsx->vp.height = viewport_height;
   }

#if 0
   RARCH_LOG("Setting viewport @ %ux%u\n", viewport_width, viewport_height);
#endif
}

static unsigned rsx_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_BORDER:
      case RARCH_WRAP_EDGE:
         return GCM_TEXTURE_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GCM_TEXTURE_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GCM_TEXTURE_MIRRORED_REPEAT;
      default:
	       break;
   }

   return 0;
}

static uintptr_t rsx_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   rsx_t *rsx = (rsx_t *)video_data;
   struct texture_image *image    = (struct texture_image*)data;

   rsx_texture_t *texture = (rsx_texture_t *)malloc(sizeof(rsx_texture_t));

   texture->width = image->width;
   texture->height = image->height;
   texture->data = (u32*)rsxMemalign(128, (image->height * image->width*4));
	 rsxAddressToOffset(texture->data, &texture->offset);
   rsx_load_texture_data(rsx, texture, image->pixels, image->width, image->height, image->width*4, true, false, filter_type);

   return (uintptr_t)texture;;
}

static void rsx_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   rsx_texture_t *texture = (rsx_texture_t *)handle;
   if (texture) {
      if(texture->data)
         rsxFree(texture->data);
      free(texture);
   }
}

static void rsx_fill_black(uint32_t *dst, uint32_t *dst_end, size_t sz)
{
  if (sz > dst_end - dst)
    sz = dst_end - dst;
  memset (dst, 0, sz * 4);
}

static void rsx_blit_buffer(
      rsxBuffer *buffer, const void *frame, unsigned width,
      unsigned height, unsigned pitch, int rgb32, bool do_scaling)
{
   int i;
   uint32_t *dst;
   uint32_t *dst_end;
   int pre_clean;
   int scale = 1, xofs = 0, yofs = 0;

   if (width > buffer->width)
      width = buffer->width;
   if (height > buffer->height)
      height = buffer->height;

   if (do_scaling)
   {
      scale = buffer->width / width;
      if (scale > buffer->height / height)
         scale = buffer->height / height;
      if (scale >= 10)
         scale = 10;
      if (scale >= 1)
      {
         xofs = (buffer->width - width * scale) / 2;
         yofs = (buffer->height - height * scale) / 2;
      }
      else
         scale = 1;
   }

   /* TODO/FIXME: let RSX do the copy */
   pre_clean = xofs + buffer->width * yofs;
   dst       = buffer->ptr;
   dst_end   = buffer->ptr + buffer->width * buffer->height;

   memset(dst, 0, pre_clean * 4);
   dst      += pre_clean;

   if (scale == 1)
   {
      if (rgb32)
      {
         const uint8_t *src = frame;
         for (i = 0; i < height; i++)
         {
            memcpy(dst, src, width * 4);
            rsx_fill_black(dst + width, dst_end, buffer->width - width);
            dst += buffer->width;
            src += pitch;
         }
      }
      else
      {
         const uint16_t *src = frame;
         for (i = 0; i < height; i++)
         {
            for (int j = 0; j < width; j++, src++, dst++)
            {
               u16 rgb565 = *src;
               u8 r = ((rgb565 >> 8) & 0xf8);
               u8 g = ((rgb565 >> 3) & 0xfc);
               u8 b = ((rgb565 << 3) & 0xfc);
               *dst = (r<<16) | (g<<8) | b;
            }
            rsx_fill_black(dst, dst_end, buffer->width - width);

            dst += buffer->width - width;
            src += pitch / 2 - width;
         }
      }
   }
   else
   {
      if (rgb32)
      {
         const uint32_t *src = frame;
         for (i = 0; i < height; i++)
         {
            for (int j = 0; j < width; j++, src++)
            {
               u32 c = *src;
               for (int k = 0; k < scale; k++, dst++)
                  for (int l = 0; l < scale; l++)
                     dst[l * buffer->width] = c;
            }
            for (int l = 0; l < scale; l++)
               rsx_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

            dst += buffer->width * scale - width * scale;
            src += pitch / 4 - width;
         }
      } else {
         const uint16_t *src = frame;
         for (i = 0; i < height; i++)
         {
            for (int j = 0; j < width; j++, src++)
            {
               u16 rgb565 = *src;
               u8 r = ((rgb565 >> 8) & 0xf8);
               u8 g = ((rgb565 >> 3) & 0xfc);
               u8 b = ((rgb565 << 3) & 0xfc);
               u32 c = (r<<16) | (g<<8) | b;
               for (int k = 0; k < scale; k++, dst++)
                  for (int l = 0; l < scale; l++)
                     dst[l * buffer->width] = c;
            }
            for (int l = 0; l < scale; l++)
               rsx_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

            dst += buffer->width * scale - width * scale;
            src += pitch / 2 - width;
         }
      }
   }

   if (dst < dst_end)
      memset(dst, 0, 4 * (dst_end - dst));
}

static void rsx_load_texture_data(rsx_t* rsx, rsx_texture_t *texture,
      const void *frame, unsigned width, unsigned height, unsigned pitch,
      bool rgb32, bool menu, enum texture_filter_type filter_type)
{
   u32 mag_filter, min_filter;

   if (!texture->data)
   {
      texture->data = (u32 *)rsxMemalign(128, texture->height * pitch);
      rsxAddressToOffset(texture->data, &texture->offset);
   }

   u8 *texbuffer = (u8 *)texture->data;
   const u8 *data = (u8 *)frame;
   memcpy(texbuffer, data, height * pitch);

	 texture->tex.format = (rgb32 ? GCM_TEXTURE_FORMAT_A8R8G8B8 :
                           menu ? GCM_TEXTURE_FORMAT_A4R4G4B4 : GCM_TEXTURE_FORMAT_R5G6B5) | GCM_TEXTURE_FORMAT_LIN;
	 texture->tex.mipmap = 1;
	 texture->tex.dimension = GCM_TEXTURE_DIMS_2D;
	 texture->tex.cubemap = GCM_FALSE;
   texture->tex.remap = ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
                         (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
                         (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
                         (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
   texture->tex.width = width;
   texture->tex.height = height;
   texture->tex.depth = 1;
   texture->tex.location = GCM_LOCATION_RSX;
   texture->tex.pitch = pitch;
   texture->tex.offset = texture->offset;

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_NEAREST:
      case TEXTURE_FILTER_NEAREST:
         min_filter = GCM_TEXTURE_NEAREST;
         mag_filter = GCM_TEXTURE_NEAREST;
         break;
      case TEXTURE_FILTER_MIPMAP_LINEAR:
      case TEXTURE_FILTER_LINEAR:
         default:
         min_filter = GCM_TEXTURE_LINEAR;
         mag_filter = GCM_TEXTURE_LINEAR;
         break;
   }
   texture->min_filter = min_filter;
   texture->mag_filter = mag_filter;
   texture->wrap_s = GCM_TEXTURE_CLAMP_TO_EDGE;
   texture->wrap_t = GCM_TEXTURE_CLAMP_TO_EDGE;
}

static void rsx_set_texture(rsx_t* rsx, rsx_texture_t *texture)
{
   rsxInvalidateTextureCache(rsx->context, GCM_INVALIDATE_TEXTURE);
   rsxLoadTexture(rsx->context, rsx->tex_unit->index, &texture->tex);
   rsxTextureControl(rsx->context, rsx->tex_unit->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxTextureFilter(rsx->context, rsx->tex_unit->index, 0, texture->min_filter, texture->mag_filter, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
   rsxTextureWrapMode(rsx->context, rsx->tex_unit->index, texture->wrap_s, texture->wrap_t, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
}

static void rsx_clear_surface(rsx_t* rsx)
{
   rsxSetColorMask(rsx->context,
               GCM_COLOR_MASK_R |
               GCM_COLOR_MASK_G |
               GCM_COLOR_MASK_B |
               GCM_COLOR_MASK_A);

   rsxSetColorMaskMrt(rsx->context, 0);

   rsxSetUserClipPlaneControl(rsx->context,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE);

   rsxSetClearColor(rsx->context, 0);
   rsxSetClearDepthStencil(rsx->context, 0xffffff00);
   rsxClearSurface(rsx->context,
                  GCM_CLEAR_R |
                  GCM_CLEAR_G |
                  GCM_CLEAR_B |
                  GCM_CLEAR_A |
                  GCM_CLEAR_S |
                  GCM_CLEAR_Z);
   rsxSetZControl(rsx->context, 0, 1, 1);
}

static void rsx_draw_vertices(rsx_t* rsx)
{
   if (rsx->should_resize)
      rsx_set_viewport(rsx, rsx->width, rsx->height, false, true);

   rsx->vertices[0].r = 1.0f;
   rsx->vertices[0].g = 1.0f;
   rsx->vertices[0].b = 1.0f;
   rsx->vertices[0].a = 1.0f;
   rsx->vertices[1].r = 1.0f;
   rsx->vertices[1].g = 1.0f;
   rsx->vertices[1].b = 1.0f;
   rsx->vertices[1].a = 1.0f;
   rsx->vertices[2].r = 1.0f;
   rsx->vertices[2].g = 1.0f;
   rsx->vertices[2].b = 1.0f;
   rsx->vertices[2].a = 1.0f;
   rsx->vertices[3].r = 1.0f;
   rsx->vertices[3].g = 1.0f;
   rsx->vertices[3].b = 1.0f;
   rsx->vertices[3].a = 1.0f;

   rsxBindVertexArrayAttrib(rsx->context, rsx->pos_index->index, 0, rsx->pos_offset, sizeof(rsx_vertex_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->uv_index->index, 0, rsx->uv_offset, sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->col_index->index, 0, rsx->col_offset, sizeof(rsx_vertex_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxLoadVertexProgram(rsx->context, rsx->vpo, rsx->vp_ucode);
   rsxSetVertexProgramParameter(rsx->context, rsx->vpo, rsx->proj_matrix, (float *)&rsx->mvp_no_rot);
   rsxLoadFragmentProgramLocation(rsx->context, rsx->fpo, rsx->fp_offset, GCM_LOCATION_RSX);

   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLE_STRIP, 0, 4);
}

#if defined(HAVE_MENU)
static void rsx_draw_menu_vertices(rsx_t* rsx)
{
   if (rsx->should_resize)
      rsx_set_viewport(rsx, rsx->width, rsx->height, false, true);

   rsx->vertices[0].r = 1.0f;
   rsx->vertices[0].g = 1.0f;
   rsx->vertices[0].b = 1.0f;
   rsx->vertices[0].a = rsx->menu_texture_alpha;
   rsx->vertices[1].r = 1.0f;
   rsx->vertices[1].g = 1.0f;
   rsx->vertices[1].b = 1.0f;
   rsx->vertices[1].a = rsx->menu_texture_alpha;
   rsx->vertices[2].r = 1.0f;
   rsx->vertices[2].g = 1.0f;
   rsx->vertices[2].b = 1.0f;
   rsx->vertices[2].a = rsx->menu_texture_alpha;
   rsx->vertices[3].r = 1.0f;
   rsx->vertices[3].g = 1.0f;
   rsx->vertices[3].b = 1.0f;
   rsx->vertices[3].a = rsx->menu_texture_alpha;

   rsxBindVertexArrayAttrib(rsx->context, rsx->pos_index->index, 0, rsx->pos_offset, sizeof(rsx_vertex_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->uv_index->index, 0, rsx->uv_offset, sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->col_index->index, 0, rsx->col_offset, sizeof(rsx_vertex_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxLoadVertexProgram(rsx->context, rsx->vpo, rsx->vp_ucode);
   rsxSetVertexProgramParameter(rsx->context, rsx->vpo, rsx->proj_matrix, (float *)&rsx->mvp_no_rot);
   rsxLoadFragmentProgramLocation(rsx->context, rsx->fpo, rsx->fp_offset, GCM_LOCATION_RSX);

   rsxSetBlendEnable(rsx->context, GCM_TRUE);
   rsxSetBlendFunc(rsx->context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
   rsxSetBlendEquation(rsx->context, GCM_FUNC_ADD, GCM_FUNC_ADD);

   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLE_STRIP, 0, 4);

   rsxSetBlendEnable(rsx->context, GCM_FALSE);
}
#endif

static void rsx_update_screen(rsx_t* gcm)
{
   rsxBuffer *buffer;

#if defined(HAVE_MENU_BUFFER)
   if (gcm->menu_frame_enable) {
      buffer = &gcm->menuBuffers[gcm->menuBuffer];
      gcm->menuBuffer = (gcm->menuBuffer+1)%MAX_MENU_BUFFERS;
      gcm->nextBuffer = MAX_BUFFERS + gcm->menuBuffer;
   } else {
      buffer = &gcm->buffers[gcm->currentBuffer];
      gcm->currentBuffer = (gcm->currentBuffer+1)%MAX_BUFFERS;
      gcm->nextBuffer = gcm->currentBuffer;
   }
#else
   buffer = &gcm->buffers[gcm->currentBuffer];
   gcm->currentBuffer = (gcm->currentBuffer+1)%MAX_BUFFERS;
   gcm->nextBuffer = gcm->currentBuffer;
#endif


   rsx_flip(gcm->context, buffer->id);
   if (gcm->vsync)
      rsx_wait_flip();
   rsxSetSurface(gcm->context, &gcm->surface[gcm->nextBuffer]);
#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif
}

static bool rsx_frame(void* data, const void* frame,
      unsigned width, unsigned height,
      uint64_t frame_count,
      unsigned pitch, const char* msg, video_frame_info_t *video_info)
{
   rsx_t* gcm = (rsx_t*)data;
#ifdef HAVE_MENU
   bool statistics_show           = video_info->statistics_show;
   struct font_params *osd_params = (struct font_params*)
      &video_info->osd_stat_params;
   bool menu_is_alive               = video_info->menu_is_alive;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active              = video_info->widgets_active;
#endif

   if(frame && width && height)
   {
      rsx_load_texture_data(gcm, &gcm->texture, frame, width, height, pitch, gcm->rgb32, false,
                            gcm->smooth ? TEXTURE_FILTER_LINEAR : TEXTURE_FILTER_NEAREST);
      rsx_set_texture(gcm, &gcm->texture);
      rsx_draw_vertices(gcm);
   }

#ifdef HAVE_MENU
   if (gcm->menu_frame_enable && menu_is_alive)
   {
      menu_driver_frame(menu_is_alive, video_info);
      if (gcm->menu_texture.data)
      {
         rsx_set_texture(gcm, &gcm->menu_texture);
         rsx_draw_menu_vertices(gcm);
      }
   };

   if (statistics_show)
      if (osd_params)
         font_driver_render_msg(gcm,
               video_info->stat_text,
               osd_params, NULL);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg)
      font_driver_render_msg(gcm, msg, NULL, NULL);

#if 0
   /* TODO: translucid menu */
#endif
   rsx_update_screen(gcm);
   rsx_clear_surface(gcm);

   return true;
}

static void rsx_set_nonblock_state(void* data, bool toggle,
      bool a, unsigned b)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      gcm->vsync = !toggle;
}

static bool rsx_alive(void* data)
{
   (void)data;
   return true;
}

static bool rsx_focus(void* data)
{
   (void)data;
   return true;
}

static bool rsx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void rsx_free(void* data)
{
   int i;
   rsx_t* gcm = (rsx_t*)data;

   if (!gcm)
      return;

   rsxClearSurface(gcm->context, GCM_CLEAR_Z);
   gcmSetWaitFlip(gcm->context);
   for (i = 0; i < MAX_BUFFERS; i++)
     rsxFree(gcm->buffers[i].ptr);
#if defined(HAVE_MENU_BUFFER)
   for (i = 0; i < MAX_MENU_BUFFERS; i++)
     rsxFree(gcm->menuBuffers[i].ptr);
#endif

   if (gcm->vertices)
      rsxFree(gcm->vertices);
   if (gcm->texture.data)
     rsxFree(gcm->texture.data);
   if (gcm->menu_texture.data)
     rsxFree(gcm->menu_texture.data);
   if (gcm->depth_buffer)
     rsxFree(gcm->depth_buffer);
   if (gcm->fp_buffer)
     rsxFree(gcm->fp_buffer);

#if 0
   rsxFinish(gcm->context, 1);
   free(gcm->host_addr);
#endif
   free (gcm);
}

static void rsx_set_texture_frame(void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   rsx_t* gcm = (rsx_t*)data;

   gcm->menu_texture_alpha = alpha;
   gcm->menu_width = width;
   gcm->menu_height = height;
   rsx_load_texture_data(gcm, &gcm->menu_texture, frame, width, height, width * (rgb32 ? 4 : 2),
                         rgb32, true, gcm->smooth ? TEXTURE_FILTER_LINEAR : TEXTURE_FILTER_NEAREST);
}

static void rsx_set_texture_enable(void* data, bool state, bool full_screen)
{
   rsx_t* gcm = (rsx_t*)data;

   if (!gcm)
     return;

   gcm->menu_frame_enable = state;
}

static void rsx_set_rotation(void* data, unsigned rotation)
{
   rsx_t* gcm = (rsx_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!gcm)
      return;

   gcm->rotation = 90 * rotation;
   gcm->should_resize = true;
   rsx_set_projection(gcm, &ortho, true);
}

static void rsx_set_filtering(void* data, unsigned index, bool smooth, bool ctx_scaling)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      gcm->smooth = smooth;
}

static void rsx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   rsx_t* gcm = (rsx_t*)data;

   if(!gcm)
      return;

   gcm->keep_aspect   = true;
   gcm->should_resize = true;
}

static void rsx_apply_state_changes(void* data)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      gcm->should_resize = true;

}

static void rsx_viewport_info(void* data, struct video_viewport* vp)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      *vp = gcm->vp;
}

static void rsx_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm && gcm->msg_rendering_enabled)
      font_driver_render_msg(data, msg, params, font);
}

static uint32_t rsx_get_flags(void *data)
{
   uint32_t             flags   = 0;

   return flags;
}

static const video_poke_interface_t rsx_poke_interface = {
   rsx_get_flags,
   rsx_load_texture,
   rsx_unload_texture,
   NULL,
   NULL,
   rsx_set_filtering,
   NULL,                                  /* get_video_output_size */
   NULL,                                  /* get_video_output_prev */
   NULL,                                  /* get_video_output_next */
   NULL,                                  /* get_current_framebuffer */
   NULL,
   rsx_set_aspect_ratio,
   rsx_apply_state_changes,
   rsx_set_texture_frame,
   rsx_set_texture_enable,
   font_driver_render_msg,
   NULL,                   /* show_mouse */
   NULL,                   /* grab_mouse_toggle */
   NULL,                   /* get_current_shader */
   NULL,                   /* get_current_software_framebuffer */
   NULL,                   /* get_hw_render_interface */
   NULL,                   /* set_hdr_max_nits */
   NULL,                   /* set_hdr_paper_white_nits */
   NULL,                   /* set_hdr_contrast */
   NULL                    /* set_hdr_expand_gamut */
};

static void rsx_get_poke_interface(void* data,
      const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &rsx_poke_interface;
}

static bool rsx_set_shader(void* data,
      enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

#ifdef HAVE_GFX_WIDGETS
static bool rsx_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

video_driver_t video_gcm =
{
   rsx_init,
   rsx_frame,
   rsx_set_nonblock_state,
   rsx_alive,
   rsx_focus,
   rsx_suppress_screensaver,
   NULL, /* has_windowed */
   rsx_set_shader,
   rsx_free,
   "rsx",
   rsx_set_viewport,
   rsx_set_rotation,
   rsx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   rsx_get_poke_interface,
   rsx_wrap_type_to_enum,
#ifdef HAVE_GFX_WIDGETS
   rsx_widgets_enabled
#endif
};
