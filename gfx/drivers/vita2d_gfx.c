/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Sergi Granell (xerpi)
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <vita2d.h>

#include <retro_inline.h>
#include <string/stdstring.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#include "../../menu/widgets/menu_widgets.h"
#endif
#endif

#include "../font_driver.h"

#include "../common/vita2d_common.h"
#include "../../driver.h"
#include "../video_coord_array.h"
#include "../../verbosity.h"
#include "../../configuration.h"

#include "../../defines/psp_defines.h"

extern void *memcpy_neon(void *dst, const void *src, size_t n);

static void vita2d_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate);

static void *vita2d_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   vita_video_t *vita   = (vita_video_t *)calloc(1, sizeof(vita_video_t));
   unsigned temp_width                = PSP_FB_WIDTH;
   unsigned temp_height               = PSP_FB_HEIGHT;

   if (!vita)
      return NULL;

   *input         = NULL;
   *input_data    = NULL;

   RARCH_LOG("vita2d_gfx_init: w: %i  h: %i\n", video->width, video->height);
   RARCH_LOG("RARCH_SCALE_BASE: %i input_scale: %i = %i\n",
	 RARCH_SCALE_BASE, video->input_scale, RARCH_SCALE_BASE * video->input_scale);

   vita2d_init();
   vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
   vita2d_set_vblank_wait(video->vsync);

   if (video->rgb32)
   {
      RARCH_LOG("Format: SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB\n");
      vita->format = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB;
   }
   else
   {
      RARCH_LOG("Format: SCE_GXM_TEXTURE_FORMAT_R5G6B5\n");
      vita->format = SCE_GXM_TEXTURE_FORMAT_R5G6B5;
   }

   vita->fullscreen = video->fullscreen;

   vita->texture      = NULL;
   vita->menu.texture = NULL;
   vita->menu.active  = 0;
   vita->menu.width   = 0;
   vita->menu.height  = 0;

   vita->vsync        = video->vsync;
   vita->rgb32        = video->rgb32;

   vita->tex_filter   = video->smooth
      ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;

   video_driver_set_size(&temp_width, &temp_height);
   vita2d_gfx_set_viewport(vita, temp_width, temp_height, false, true);

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      void *pspinput       = input_psp.init(settings->arrays.input_joypad_driver);
      *input               = pspinput ? &input_psp : NULL;
      *input_data          = pspinput;
   }

   vita->keep_aspect        = true;
   vita->should_resize      = true;
#ifdef HAVE_OVERLAY
   vita->overlay_enable     = false;
#endif
   font_driver_init_osd(vita, false,
         video->is_threaded,
         FONT_DRIVER_RENDER_VITA2D);

   return vita;
}

#ifdef HAVE_OVERLAY
static void vita2d_render_overlay(void *data);
static void vita2d_free_overlay(vita_video_t *vita)
{
   unsigned i;

   vita2d_wait_rendering_done();

   for (i = 0; i < vita->overlays; i++)
      vita2d_free_texture(vita->overlay[i].tex);
   free(vita->overlay);
   vita->overlay = NULL;
   vita->overlays = 0;
}
#endif

static void vita2d_gfx_update_viewport(vita_video_t* vita,
      video_frame_info_t *video_info);

static bool vita2d_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   void *tex_p;
   vita_video_t *vita = (vita_video_t *)data;

   if (frame)
   {
      if(!(vita->texture&&vita2d_texture_get_datap(vita->texture)==frame))
      {
         unsigned i;
         unsigned int stride;

         if ((width != vita->width || height != vita->height) && vita->texture)
         {
            vita2d_free_texture(vita->texture);
            vita->texture = NULL;
         }

         if (!vita->texture)
         {
            RARCH_LOG("Creating texture: %ix%i\n", width, height);
            vita->width = width;
            vita->height = height;
            vita->texture = vita2d_create_empty_texture_format(width, height, vita->format);
            vita2d_texture_set_filters(vita->texture,vita->tex_filter,vita->tex_filter);
         }
         tex_p = vita2d_texture_get_datap(vita->texture);
         stride = vita2d_texture_get_stride(vita->texture);

         if (vita->format == SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB)
         {
            stride                     /= 4;
            pitch                      /= 4;
            uint32_t             *tex32 = tex_p;
            const uint32_t     *frame32 = frame;

            for (i = 0; i < height; i++)
               memcpy_neon(&tex32[i*stride],&frame32[i*pitch],pitch*sizeof(uint32_t));
         }
         else
         {
            stride                 /= 2;
            pitch                  /= 2;
            uint16_t *tex16         = tex_p;
            const uint16_t *frame16 = frame;

            for (i = 0; i < height; i++)
               memcpy_neon(&tex16[i*stride],&frame16[i*pitch],width*sizeof(uint16_t));
         }
      }
   }

   if (vita->should_resize)
      vita2d_gfx_update_viewport(vita, video_info);

   vita2d_start_drawing();

   vita2d_draw_rectangle(0,0,PSP_FB_WIDTH,PSP_FB_HEIGHT,vita2d_get_clear_color());

   if (vita->texture)
   {
      if (vita->fullscreen)
         vita2d_draw_texture_scale(vita->texture,
               0, 0,
               PSP_FB_WIDTH  / (float)vita->width,
               PSP_FB_HEIGHT / (float)vita->height);
      else
      {
         const float radian = 90 * 0.0174532925f;
         const float rad = vita->rotation * radian;
         float scalex = vita->vp.width / (float)vita->width;
         float scaley = vita->vp.height / (float)vita->height;
         vita2d_draw_texture_scale_rotate(vita->texture,vita->vp.x,
               vita->vp.y, scalex, scaley, rad);
      }
   }

   if (vita->menu.active)
   {
#ifdef HAVE_MENU
      menu_driver_frame(video_info);
#endif

      if(vita->menu.texture)
      {
         if (vita->fullscreen)
            vita2d_draw_texture_scale(vita->menu.texture,
                  0, 0,
                  PSP_FB_WIDTH  / (float)vita->menu.width,
                  PSP_FB_HEIGHT / (float)vita->menu.height);
         else
         {
            if (vita->menu.width > vita->menu.height)
            {
               float scale = PSP_FB_HEIGHT / (float)vita->menu.height;
               float w = vita->menu.width * scale;
               vita2d_draw_texture_scale(vita->menu.texture,
                     PSP_FB_WIDTH / 2.0f - w/2.0f, 0.0f,
                     scale, scale);
            }
            else
            {
               float scale = PSP_FB_WIDTH / (float)vita->menu.width;
               float h = vita->menu.height * scale;
               vita2d_draw_texture_scale(vita->menu.texture,
                     0.0f, PSP_FB_HEIGHT / 2.0f - h/2.0f,
                     scale, scale);
            }
         }
      }
   }
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
         font_driver_render_msg(vita, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
   }

   #ifdef HAVE_OVERLAY
   if (vita->overlay_enable)
      vita2d_render_overlay(vita);
   #endif

   #ifdef HAVE_MENU_WIDGETS
      menu_widgets_frame(video_info);
   #endif

   if(!string_is_empty(msg))
      font_driver_render_msg(vita, video_info, msg, NULL, NULL);

   vita2d_end_drawing();
   vita2d_swap_buffers();

   return true;
}

static void vita2d_gfx_set_nonblock_state(void *data, bool toggle)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->vsync = !toggle;
      vita2d_set_vblank_wait(vita->vsync);
   }
}

static bool vita2d_gfx_alive(void *data)
{
   (void)data;
   return true;
}

static bool vita2d_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool vita2d_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void vita2d_gfx_free(void *data)
{
   vita_video_t *vita = (vita_video_t *)data;

   vita2d_fini();

   if (vita->menu.texture)
   {
      vita2d_free_texture(vita->menu.texture);
      vita->menu.texture = NULL;
   }

   if (vita->texture)
   {
      vita2d_free_texture(vita->texture);
      vita->texture = NULL;
   }

   font_driver_free_osd();
}

static bool vita2d_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void vita2d_set_projection(vita_video_t *vita,
      struct video_ortho *ortho, bool allow_rotate)
{
   math_matrix_4x4 rot;

   /* Calculate projection. */
   matrix_4x4_ortho(vita->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      vita->mvp = vita->mvp_no_rot;
      return;
   }

   matrix_4x4_rotate_z(rot, M_PI * vita->rotation / 180.0f);
   matrix_4x4_multiply(vita->mvp, rot, vita->mvp_no_rot);
}

static void vita2d_gfx_update_viewport(vita_video_t* vita,
      video_frame_info_t *video_info)
{
   int x                = 0;
   int y                = 0;
   float device_aspect  = ((float)PSP_FB_WIDTH) / PSP_FB_HEIGHT;
   float width          = PSP_FB_WIDTH;
   float height         = PSP_FB_HEIGHT;
   settings_t *settings = config_get_ptr();

   if (settings->bools.video_scale_integer)
   {
      video_viewport_get_scaled_integer(&vita->vp, PSP_FB_WIDTH,
            PSP_FB_HEIGHT, video_driver_get_aspect_ratio(), vita->keep_aspect);
      width  = vita->vp.width;
      height = vita->vp.height;
   }
   else if (vita->keep_aspect)
   {
      float desired_aspect = video_driver_get_aspect_ratio();
      if ( (vita->rotation == ORIENTATION_VERTICAL) ||
           (vita->rotation == ORIENTATION_FLIPPED_ROTATED))
      {
         device_aspect = 1.0 / device_aspect;
         width = PSP_FB_HEIGHT;
         height = PSP_FB_WIDTH;
      }
#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         x      = video_info->custom_vp_x;
         y      = video_info->custom_vp_y;
         width  = video_info->custom_vp_width;
         height = video_info->custom_vp_height;
      }
      else
#endif
      {
         float delta;

         if ((fabsf(device_aspect - desired_aspect) < 0.0001f))
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x     = (int)roundf(width * (0.5f - delta));
            width = (unsigned)roundf(2.0f * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y      = (int)roundf(height * (0.5f - delta));
            height = (unsigned)roundf(2.0f * height * delta);
         }

         if ( (vita->rotation == ORIENTATION_VERTICAL) ||
              (vita->rotation == ORIENTATION_FLIPPED_ROTATED)
            )
         {
            x = (PSP_FB_WIDTH - width) * 0.5f;
            y = (PSP_FB_HEIGHT - height) * 0.5f;
         }
      }

      vita->vp.x      = x;
      vita->vp.y      = y;
      vita->vp.width  = width;
      vita->vp.height = height;
   }
   else
   {
      vita->vp.x = vita->vp.y = 0;
      vita->vp.width = width;
      vita->vp.height = height;
   }

   vita->vp.width += vita->vp.width&0x1;
   vita->vp.height += vita->vp.height&0x1;

   vita->should_resize = false;

}

static void vita2d_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   gfx_ctx_aspect_t aspect_data;
   int x                    = 0;
   int y                    = 0;
   float device_aspect      = (float)viewport_width / viewport_height;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};
   settings_t *settings     = config_get_ptr();
   vita_video_t *vita       = (vita_video_t*)data;

   aspect_data.aspect       = &device_aspect;
   aspect_data.width        = viewport_width;
   aspect_data.height       = viewport_height;

   if (settings->bools.video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&vita->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(), vita->keep_aspect);
      viewport_width  = vita->vp.width;
      viewport_height = vita->vp.height;
   }
   else if (vita->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
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

      vita->vp.x      = x;
      vita->vp.y      = y;
      vita->vp.width  = viewport_width;
      vita->vp.height = viewport_height;
   }
   else
   {
      vita->vp.x      = 0;
      vita->vp.y      = 0;
      vita->vp.width  = viewport_width;
      vita->vp.height = viewport_height;
   }

   vita2d_set_viewport(vita->vp.x, vita->vp.y, vita->vp.width, vita->vp.height);
   vita2d_set_projection(vita, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      vita->vp.width  = viewport_width;
      vita->vp.height = viewport_height;
   }

#if 0
   vita->vp.x          = (float)vita->vp.x;
   vita->vp.y          = (float)vita->vp.y;
   vita->vp.width      = (float)vita->vp.width;
   vita->vp.height     = (float)vita->vp.height;
   vita->vp.minDepth   = 0.0f;
   vita->vp.maxDepth   = 1.0f;

   RARCH_LOG("Setting viewport @ %ux%u\n", viewport_width, viewport_height);
#endif
}

static void vita2d_gfx_set_rotation(void *data,
      unsigned rotation)
{
  vita_video_t *vita = (vita_video_t*)data;
  struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

  if (!vita)
     return;

  vita->rotation = rotation;
  vita->should_resize = true;
  vita2d_set_projection(vita, &ortho, true);

}

static void vita2d_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
    vita_video_t *vita = (vita_video_t*)data;

    if (vita)
       *vp = vita->vp;
}

static void vita_set_filtering(void *data, unsigned index, bool smooth)
{
   vita_video_t *vita = (vita_video_t *)data;

   if (vita)
   {
      vita->tex_filter = smooth ?
         SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
      vita2d_texture_set_filters(vita->texture,vita->tex_filter,
            vita->tex_filter);
   }
}

static void vita_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   vita_video_t               *vita = (vita_video_t*)data;

   if (!vita)
      return;
   vita->keep_aspect = true;
   vita->should_resize = true;
}

static void vita_apply_state_changes(void *data)
{
  vita_video_t *vita = (vita_video_t*)data;

  if (vita)
     vita->should_resize = true;
}

static void vita_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   int i, j;
   void *tex_p;
   unsigned int stride;
   vita_video_t *vita = (vita_video_t*)data;

   (void)alpha;

   if (width != vita->menu.width && height != vita->menu.height && vita->menu.texture)
   {
      vita2d_free_texture(vita->menu.texture);
      vita->menu.texture = NULL;
   }

   if (!vita->menu.texture)
   {
      if (rgb32)
      {
         vita->menu.texture = vita2d_create_empty_texture(width, height);
         RARCH_LOG("Creating Frame RGBA8 texture: w: %i  h: %i\n",
               width, height);
      }
      else
      {
         vita->menu.texture = vita2d_create_empty_texture_format(
               width, height, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA);
         RARCH_LOG("Creating Frame R5G6B5 texture: w: %i  h: %i\n",
               width, height);
      }
      vita->menu.width  = width;
      vita->menu.height = height;
   }

   vita2d_texture_set_filters(vita->menu.texture,
         SCE_GXM_TEXTURE_FILTER_LINEAR,
         SCE_GXM_TEXTURE_FILTER_LINEAR);

   tex_p  = vita2d_texture_get_datap(vita->menu.texture);
   stride = vita2d_texture_get_stride(vita->menu.texture);

   if (rgb32)
   {
      uint32_t         *tex32 = tex_p;
      const uint32_t *frame32 = frame;

      stride                 /= 4;

      for (i = 0; i < height; i++)
         for (j = 0; j < width; j++)
            tex32[j + i*stride] = frame32[j + i*width];
   }
   else
   {
      uint16_t               *tex16 = tex_p;
      const uint16_t       *frame16 = frame;

      stride                       /= 2;

      for (i = 0; i < height; i++)
         for (j = 0; j < width; j++)
            tex16[j + i*stride] = frame16[j + i*width];
   }
}

static void vita_set_texture_enable(void *data, bool state, bool full_screen)
{
   vita_video_t *vita = (vita_video_t*)data;
   (void)full_screen;

   vita->menu.active = state;
}

static uintptr_t vita_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   unsigned int stride, pitch, j;
   uint32_t             *tex32    = NULL;
   const uint32_t *frame32        = NULL;
   struct texture_image *image    = (struct texture_image*)data;
   struct vita2d_texture *texture = vita2d_create_empty_texture_format(image->width,
     image->height,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);

   if (!texture)
      return 0;

   if ((filter_type == TEXTURE_FILTER_MIPMAP_LINEAR) ||
      (filter_type == TEXTURE_FILTER_LINEAR))
      vita2d_texture_set_filters(texture,
            SCE_GXM_TEXTURE_FILTER_LINEAR,
            SCE_GXM_TEXTURE_FILTER_LINEAR);

   stride                      = vita2d_texture_get_stride(texture);
   stride                     /= 4;

   tex32                       = vita2d_texture_get_datap(texture);
   frame32                     = image->pixels;
   pitch                       = image->width;

   for (j = 0; j < image->height; j++)
         memcpy_neon(
               &tex32[j*stride],
               &frame32[j*pitch],
               pitch * sizeof(uint32_t));

   return (uintptr_t)texture;
}

static void vita_unload_texture(void *data, uintptr_t handle)
{
   struct vita2d_texture *texture = (struct vita2d_texture*)handle;
   if (!texture)
      return;

   /* TODO: We really want to defer this deletion instead,
    * but this will do for now. */
   vita2d_wait_rendering_done();
   vita2d_free_texture(texture);

   //free(texture);
}

static bool vita_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *framebuffer)
{
   vita_video_t *vita = (vita_video_t*)data;

   if (!vita->texture || vita->width != framebuffer->width ||
         vita->height != framebuffer->height)
   {
      if(vita->texture)
      {
         vita2d_wait_rendering_done();
         vita2d_free_texture(vita->texture);
         vita->texture = NULL;
      }

      vita->width = framebuffer->width;
      vita->height = framebuffer->height;
      vita->texture = vita2d_create_empty_texture_format(
            vita->width, vita->height, vita->format);
      vita2d_texture_set_filters(vita->texture,
            vita->tex_filter,vita->tex_filter);
   }

   framebuffer->data         = vita2d_texture_get_datap(vita->texture);
   framebuffer->pitch        = vita2d_texture_get_stride(vita->texture);
   framebuffer->format       = vita->rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   framebuffer->memory_flags = 0;

   return true;
}

static uint32_t vita_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

   return flags;
}

static const video_poke_interface_t vita_poke_interface = {
   vita_get_flags,
   vita_load_texture,
   vita_unload_texture,
   NULL,
   NULL, /* get_refresh_rate */
   vita_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   vita_set_aspect_ratio,
   vita_apply_state_changes,
   vita_set_texture_frame,
   vita_set_texture_enable,
   font_driver_render_msg,
   NULL,
   NULL,
   NULL,
   vita_get_current_sw_framebuffer,
   NULL
 };

static void vita2d_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &vita_poke_interface;
}

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool vita2d_gfx_menu_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

#ifdef HAVE_OVERLAY
static void vita2d_overlay_tex_geom(void *data, unsigned image, float x, float y, float w, float h);
static void vita2d_overlay_vertex_geom(void *data, unsigned image, float x, float y, float w, float h);

static bool vita2d_overlay_load(void *data, const void *image_data, unsigned num_images)
{
   unsigned i,j,k;
   unsigned int stride, pitch;
   vita_video_t *vita = (vita_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)image_data;

   vita2d_free_overlay(vita);
   vita->overlay = (struct vita_overlay_data*)calloc(num_images, sizeof(*vita->overlay));
   if (!vita->overlay)
      return false;

   vita->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct vita_overlay_data *o = (struct vita_overlay_data*)&vita->overlay[i];
      o->width = images[i].width;
      o->height = images[i].height;
      o->tex = vita2d_create_empty_texture_format(o->width , o->height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
      vita2d_texture_set_filters(o->tex,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
      stride = vita2d_texture_get_stride(o->tex);
      stride                     /= 4;
      uint32_t             *tex32 = vita2d_texture_get_datap(o->tex);
      const uint32_t     *frame32 = images[i].pixels;
      pitch = o->width;
      for (j = 0; j < o->height; j++)
         for (k = 0; k < o->width; k++)
            tex32[k + j*stride] = frame32[k + j*pitch];

      vita2d_overlay_tex_geom(vita, i, 0, 0, 1, 1); /* Default. Stretch to whole screen. */
      vita2d_overlay_vertex_geom(vita, i, 0, 0, 1, 1);
      vita->overlay[i].alpha_mod = 1.0f;
   }

   return true;
}

static void vita2d_overlay_tex_geom(void *data, unsigned image,
      float x, float y, float w, float h)
{
   vita_video_t          *vita = (vita_video_t*)data;
   struct vita_overlay_data *o = NULL;

   if (vita)
      o = (struct vita_overlay_data*)&vita->overlay[image];

   if (o)
   {
      o->tex_x = x;
      o->tex_y = y;
      o->tex_w = w*o->width;
      o->tex_h = h*o->height;
   }
}

static void vita2d_overlay_vertex_geom(void *data, unsigned image,
         float x, float y, float w, float h)
{
   vita_video_t          *vita = (vita_video_t*)data;
   struct vita_overlay_data *o = NULL;

   /* Flipped, so we preserve top-down semantics. */
   /*y = 1.0f - y;
   h = -h;*/

   if (vita)
      o = (struct vita_overlay_data*)&vita->overlay[image];

   if (o)
   {

      o->w = w*PSP_FB_WIDTH/o->width;
      o->h = h*PSP_FB_HEIGHT/o->height;
      o->x = PSP_FB_WIDTH*(1-w)/2+x;
      o->y = PSP_FB_HEIGHT*(1-h)/2+y;
   }
}

static void vita2d_overlay_enable(void *data, bool state)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->overlay_enable = state;
}

static void vita2d_overlay_full_screen(void *data, bool enable)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->overlay_full_screen = enable;
}

static void vita2d_overlay_set_alpha(void *data, unsigned image, float mod)
{
   vita_video_t *vita = (vita_video_t*)data;
   vita->overlay[image].alpha_mod = mod;
}

static void vita2d_render_overlay(void *data)
{
   unsigned i;
   vita_video_t *vita = (vita_video_t*)data;

   for (i = 0; i < vita->overlays; i++)
   {
      vita2d_draw_texture_tint_part_scale(vita->overlay[i].tex,
            vita->overlay[i].x,
            vita->overlay[i].y,
            vita->overlay[i].tex_x,
            vita->overlay[i].tex_y,
            vita->overlay[i].tex_w,
            vita->overlay[i].tex_h,
            vita->overlay[i].w,
            vita->overlay[i].h,
            RGBA8(0xFF,0xFF,0xFF,(uint8_t)(vita->overlay[i].alpha_mod * 255.0f)));
   }
}

static const video_overlay_interface_t vita2d_overlay_interface = {
   vita2d_overlay_enable,
   vita2d_overlay_load,
   vita2d_overlay_tex_geom,
   vita2d_overlay_vertex_geom,
   vita2d_overlay_full_screen,
   vita2d_overlay_set_alpha,
};

static void vita2d_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &vita2d_overlay_interface;
}
#endif

video_driver_t video_vita2d = {
   vita2d_gfx_init,
   vita2d_gfx_frame,
   vita2d_gfx_set_nonblock_state,
   vita2d_gfx_alive,
   vita2d_gfx_focus,
   vita2d_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   vita2d_gfx_set_shader,
   vita2d_gfx_free,
   "vita2d",
   vita2d_gfx_set_viewport,
   vita2d_gfx_set_rotation,
   vita2d_gfx_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   vita2d_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   vita2d_gfx_get_poke_interface,
   NULL,
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   vita2d_gfx_menu_widgets_enabled
#endif
};
