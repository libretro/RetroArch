/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <caca.h>

#include <stdlib.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../common/caca_defines.h"
#include "../font_driver.h"

#include "../../driver.h"
#include "../../verbosity.h"

typedef struct
{
   const font_renderer_driver_t *font_driver;
   void *font_data;
   caca_t *caca;
} caca_raster_t;

/*
 * FORWARD DECLARATIONS
 */
static void caca_free(void *data);

/*
 * FONT DRIVER
 */

static void *caca_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   caca_raster_t *font  = (caca_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->caca = (caca_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
      return NULL;

   return font;
}

static void caca_font_free(void *data, bool is_threaded)
{
  caca_raster_t *font = (caca_raster_t*)data;

  if (!font)
     return;

  if (font->font_driver && font->font_data)
     font->font_driver->free(font->font_data);

  free(font);
}

static int caca_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale) { return 0; }
static const struct font_glyph *caca_font_get_glyph(
      void *data, uint32_t code) { return NULL; }

static void caca_font_render_msg(
      void *userdata,
      void *data, const char *msg,
      const struct font_params *params)
{
   float x, y, scale;
   unsigned width, height;
   unsigned newX, newY;
   unsigned align;
   size_t msg_len;
   caca_raster_t              *font = (caca_raster_t*)data;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;

   if (!font || string_is_empty(msg))
      return;

   if (params)
   {
      x     = params->x;
      y     = params->y;
      scale = params->scale;
      align = params->text_align;
   }
   else
   {
      x     = video_msg_pos_x;
      y     = video_msg_pos_y;
      scale = 1.0f;
      align = TEXT_ALIGN_LEFT;
   }

   if (   !font->caca
       || !font->caca->cv
       || !font->caca->display
       || !font->caca->cv
       || !font->caca->display)
      return;

   width    = caca_get_canvas_width(font->caca->cv);
   height   = caca_get_canvas_height(font->caca->cv);
   newY     = height - (y * height * scale);
   msg_len  = strlen(msg);

   switch (align)
   {
      case TEXT_ALIGN_RIGHT:
         newX = (x * width * scale) - msg_len;
         break;
      case TEXT_ALIGN_CENTER:
         newX = (x * width * scale) - (msg_len / 2);
         break;
      case TEXT_ALIGN_LEFT:
      default:
         newX = x * width * scale;
         break;
   }

   caca_put_str(font->caca->cv, newX, newY, msg);

   caca_refresh_display(font->caca->display);
}

font_renderer_t caca_font = {
   caca_font_init,
   caca_font_free,
   caca_font_render_msg,
   "caca",
   caca_font_get_glyph,
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   caca_font_get_message_width,
   NULL                       /* get_line_metrics */
};

/*
 * VIDEO DRIVER
 */
static void caca_create(caca_t *caca)
{
   caca->display = caca_create_display(NULL);
   caca->cv      = caca_get_canvas(caca->display);

   if (!caca->video_width || !caca->video_height)
   {
      caca->video_width  = caca_get_canvas_width(caca->cv);
      caca->video_height = caca_get_canvas_height(caca->cv);
   }

   if (caca->rgb32)
      caca->dither = caca_create_dither(32, caca->video_width,
            caca->video_height, caca->video_pitch,
            0x00FF0000, 0xFF00, 0xFF, 0x0);
   else
      caca->dither = caca_create_dither(16, caca->video_width,
            caca->video_height, caca->video_pitch,
            0xF800, 0x7E0, 0x1F, 0x0);

   video_driver_set_size(caca->video_width, caca->video_height);
}

static void *caca_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   caca_t *caca        = (caca_t*)calloc(1, sizeof(*caca));

   if (!caca)
      return NULL;

   *input               = NULL;
   *input_data          = NULL;

   caca->video_width    = video->width;
   caca->video_height   = video->height;
   caca->rgb32          = video->rgb32;

   if (video->rgb32)
      caca->video_pitch = video->width * 4;
   else
      caca->video_pitch = video->width * 2;

   caca_create(caca);

   if (!caca->cv || !caca->dither || !caca->display)
   {
      free(caca);
      return NULL;
   }

   if (video->font_enable)
      font_driver_init_osd(caca, video,
            false, video->is_threaded,
            FONT_DRIVER_RENDER_CACA);

   return caca;
}

static bool caca_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   size_t _len               = 0;
   void *buffer              = NULL;
   const void *frame_to_copy = frame;
   unsigned width            = 0;
   unsigned height           = 0;
   bool draw                 = true;
   caca_t *caca              = (caca_t*)data;
#ifdef HAVE_MENU
   bool menu_is_alive        = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (!frame || !frame_width || !frame_height)
      return true;

   if (     (caca->video_width  != frame_width)
         || (caca->video_height != frame_height)
         || (caca->video_pitch  != pitch))
   {
      if (frame_width > 4 && frame_height > 4)
      {
         caca->video_width  = frame_width;
         caca->video_height = frame_height;
         caca->video_pitch  = pitch;
         caca_free(caca);
         caca_create(caca);
      }
   }

   if (!caca->cv)
      return true;

#ifdef HAVE_MENU
   if (caca->menu_frame && menu_is_alive)
      frame_to_copy = caca->menu_frame;
#endif

   width  = caca_get_canvas_width(caca->cv);
   height = caca_get_canvas_height(caca->cv);

   if (     (frame_to_copy == frame)
         && (frame_width   == 4)
         && (frame_height  == 4)
         && (frame_width < width && frame_height < height))
      draw = false;

#ifdef HAVE_MENU
   if (menu_is_alive)
      draw = false;
#endif

   caca_clear_canvas(caca->cv);

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (msg)
      font_driver_render_msg(data, msg, NULL, NULL);

   if (draw)
   {
      caca_dither_bitmap(caca->cv, 0, 0,
            width,
            height,
            caca->dither, frame_to_copy);

      buffer = caca_export_canvas_to_memory(caca->cv, "caca", &_len);

      if (buffer)
      {
         if (_len > 0)
            caca_refresh_display(caca->display);

         free(buffer);
      }
   }

   return true;
}

static bool caca_alive(void *data)
{
   caca_t *caca              = (caca_t*)data;
   video_driver_set_size(caca->video_width, caca->video_height);
   return true;
}

static void caca_set_nonblock_state(void *data, bool a,
      bool b, unsigned c) { }
static bool caca_focus(void *data) { return true; }
static bool caca_suppress_screensaver(void *data, bool enable) { return false; }
static bool caca_has_windowed(void *data) { return true; }

static void caca_free(void *data)
{
   caca_t *caca = (caca_t*)data;

   if (caca->display)
      caca_free_display(caca->display);
   caca->display = NULL;

   if (caca->dither)
      caca_free_dither(caca->dither);
   caca->dither = NULL;

   if (caca->menu_frame)
      free(caca->menu_frame);
   caca->menu_frame = NULL;
}

static bool caca_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }
static void caca_set_rotation(void *a, unsigned b) { }

static void caca_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   caca_t *caca   = (caca_t*)data;
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (caca->menu_frame)
      free(caca->menu_frame);
   caca->menu_frame = NULL;

   if (    (!caca->menu_frame)
         || (caca->menu_width  != width)
         || (caca->menu_height != height)
         || (caca->menu_pitch  != pitch))
   {
      if (pitch && height)
         caca->menu_frame = (unsigned char*)malloc(pitch * height);
   }

   if (caca->menu_frame && frame && pitch && height)
      memcpy(caca->menu_frame, frame, pitch * height);
}

static const video_poke_interface_t caca_poke_interface = {
   NULL, /* get_flags */
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   caca_set_texture_frame,
   NULL, /* set_texture_enable */
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void caca_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { *iface = &caca_poke_interface; }
static void caca_set_viewport(void *data, unsigned vp_width,
      unsigned vp_height, bool force_full, bool allow_rotate) { }

video_driver_t video_caca = {
   caca_init,
   caca_frame,
   caca_set_nonblock_state,
   caca_alive,
   caca_focus,
   caca_suppress_screensaver,
   caca_has_windowed,
   caca_set_shader,
   caca_free,
   "caca",
   caca_set_viewport,
   caca_set_rotation,
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   caca_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
