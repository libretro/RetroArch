/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Sergi Granell (xerpi)
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

#include <retro_inline.h>
#include "../../defines/psp_defines.h"
#include "../../general.h"
#include "../../driver.h"
#include "../video_viewport.h"
#include "../video_monitor.h"

#include <vita2d.h>

typedef struct vita_menu_frame
{
   bool active;
   int width;
   int height;
   vita2d_texture *texture;
} vita_menu_t;

typedef struct vita_video
{
   vita2d_texture *texture;
   SceGxmTextureFormat format;
   int width;
   int height;
   SceGxmTextureFilter tex_filter;

   bool fullscreen;
   bool vsync;
   bool rgb32;

   video_viewport_t vp;

   unsigned rotation;
   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;

   vita_menu_t menu;
} vita_video_t;

static void *vita2d_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   void *pspinput = NULL;
   *input = NULL;
   *input_data = NULL;
   (void)video;

   vita_video_t *vita = (vita_video_t *)calloc(1, sizeof(vita_video_t));

   if (!vita)
      return NULL;

   RARCH_LOG("vita2d_gfx_init: w: %i  h: %i\n", video->width, video->height);
   RARCH_LOG("RARCH_SCALE_BASE: %i input_scale: %i = %i\n",
	RARCH_SCALE_BASE, video->input_scale, RARCH_SCALE_BASE * video->input_scale);

   vita2d_init();
   vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
   vita2d_set_vblank_wait(video->vsync);

   if (vita->rgb32)
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

   vita->tex_filter = video->smooth? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;

   if (input && input_data)
   {
      pspinput = input_psp.init();
      *input = pspinput ? &input_psp : NULL;
      *input_data = pspinput;
   }

   return vita;
}

static bool vita2d_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   int i, j;
   void *tex_p;
   unsigned int stride;
   vita_video_t *vita = (vita_video_t *)data;

   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;

   if (frame)
   {
      if (width != vita->width && height != vita->height && vita->texture)
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
         vita2d_texture_set_texture_filter(vita->texture,vita->tex_filter);
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
            for (j = 0; j < width; j++)
               tex32[j + i*stride] = frame32[j + i*pitch];
      }
      else
      {
         stride                 /= 2;
         pitch                  /= 2;
         uint16_t *tex16         = tex_p;
         const uint16_t *frame16 = frame;

         for (i = 0; i < height; i++)
            for (j = 0; j < width; j++)
               tex16[j + i*stride] = frame16[j + i*pitch];
      }
   }

   vita2d_start_drawing();
   vita2d_clear_screen();

   if (vita->texture)
   {
      if (vita->fullscreen)
         vita2d_draw_texture_scale(vita->texture,
               0, 0,
               PSP_FB_WIDTH  / (float)vita->width,
               PSP_FB_HEIGHT / (float)vita->height);
      else
      {
         if (vita->width > vita->height)
         {
            float scale = PSP_FB_HEIGHT / (float)vita->height;
            float     w = vita->width * scale;

            vita2d_draw_texture_scale(vita->texture,
                  PSP_FB_WIDTH / 2.0f - w/2.0f, 0.0f,
                  scale, scale);
         }
         else
         {
            float scale = PSP_FB_WIDTH / (float)vita->width;
            float     h = vita->height * scale;

            vita2d_draw_texture_scale(vita->texture,
                  0.0f, PSP_FB_HEIGHT / 2.0f - h/2.0f,
                  scale, scale);
         }
      }
   }

   if (vita->menu.active && vita->menu.texture)
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

static bool vita2d_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void vita2d_gfx_free(void *data)
{
   vita_video_t *vita = (vita_video_t *)data;

   RARCH_LOG("vita2d_gfx_free()\n");

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

   RARCH_LOG("vita2d_gfx_free() done\n");
}

static bool vita2d_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void vita2d_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void vita2d_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

static bool vita2d_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static void vita_set_filtering(void *data, unsigned index, bool smooth)
{
  vita_video_t *vita = (vita_video_t *)data;

  if (vita){
    vita->tex_filter = smooth? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    vita2d_texture_set_texture_filter(vita->texture,vita->tex_filter);
  }
}

static void vita_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   switch (aspectratio_index)
   {
      case ASPECT_RATIO_SQUARE:
         video_viewport_set_square_pixel(
               av_info->geometry.base_width,
               av_info->geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         video_viewport_set_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_viewport_set_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(aspectratio_lut[aspectratio_index].value);
}

static void vita_apply_state_changes(void *data)
{
   (void)data;
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
         RARCH_LOG("Creating Frame RGBA8 texture: w: %i  h: %i\n", width, height);
      }
      else
      {
         vita->menu.texture = vita2d_create_empty_texture_format(width, height, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA);
         RARCH_LOG("Creating Frame R5G6B5 texture: w: %i  h: %i\n", width, height);
      }
      vita->menu.width = width;
      vita->menu.height = height;
   }
   vita2d_texture_set_texture_filter(vita->menu.texture,SCE_GXM_TEXTURE_FILTER_LINEAR);
   tex_p = vita2d_texture_get_datap(vita->menu.texture);
   stride = vita2d_texture_get_stride(vita->menu.texture);

   if (rgb32)
   {
      stride                 /= 4;
      uint32_t         *tex32 = tex_p;
      const uint32_t *frame32 = frame;

      for (i = 0; i < height; i++)
         for (j = 0; j < width; j++)
            tex32[j + i*stride] = frame32[j + i*width];
   }
   else
   {
      stride                       /= 2;
      uint16_t               *tex16 = tex_p;
      const uint16_t       *frame16 = frame;

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

static const video_poke_interface_t vita_poke_interface = {
   NULL,
   vita_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   vita_set_aspect_ratio,
   vita_apply_state_changes,
#ifdef HAVE_MENU
   vita_set_texture_frame,
   vita_set_texture_enable,
#endif
   NULL,
   NULL,
   NULL
};

static void vita2d_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &vita_poke_interface;
}

video_driver_t video_vita2d = {
   vita2d_gfx_init,
   vita2d_gfx_frame,
   vita2d_gfx_set_nonblock_state,
   vita2d_gfx_alive,
   vita2d_gfx_focus,
   vita2d_gfx_suppress_screensaver,
   vita2d_gfx_has_windowed,
   vita2d_gfx_set_shader,
   vita2d_gfx_free,
   "vita2d",
   NULL, /* set_viewport */
   vita2d_gfx_set_rotation,
   vita2d_gfx_viewport_info,
   vita2d_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
   vita2d_gfx_get_poke_interface,
};
