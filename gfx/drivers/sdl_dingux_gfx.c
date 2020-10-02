/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2011-2017 - Higor Euripedes
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

#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_video.h>

#include <retro_assert.h>
#include <gfx/video_frame.h>
#include <retro_assert.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../dingux/dingux_utils.h"

#include "../../verbosity.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"
#include "../../configuration.h"
#include "../../retroarch.h"

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define SDL_DINGUX_MENU_WIDTH  320
#define SDL_DINGUX_MENU_HEIGHT 240

#define SDL_DINGUX_NUM_FONT_GLYPHS 256

#define VERBOSE 0

typedef struct sdl_dingux_video
{
   SDL_Surface *screen;
   uint32_t font_colour32;
   uint16_t font_colour16;
   uint16_t menu_texture[SDL_DINGUX_MENU_WIDTH * SDL_DINGUX_MENU_HEIGHT];
   bool font_lut[SDL_DINGUX_NUM_FONT_GLYPHS][FONT_WIDTH * FONT_HEIGHT];
   bool rgb32;
   bool vsync;
   bool integer_scaling;
   bool menu_active;
   bool was_in_menu;
   bool quitting;
} sdl_dingux_video_t;

static void sdl_dingux_init_font_color(sdl_dingux_video_t *vid)
{
   settings_t *settings = config_get_ptr();
   uint32_t red         = 0xFF;
   uint32_t green       = 0xFF;
   uint32_t blue        = 0xFF;

   if (settings)
   {
      red   = (uint32_t)((settings->floats.video_msg_color_r * 255.0f) + 0.5f) & 0xFF;
      green = (uint32_t)((settings->floats.video_msg_color_g * 255.0f) + 0.5f) & 0xFF;
      blue  = (uint32_t)((settings->floats.video_msg_color_b * 255.0f) + 0.5f) & 0xFF;
   }

   /* Convert to XRGB8888 */
   vid->font_colour32 = (red << 16) | (green << 8) | blue;

   /* Convert to RGB565 */
   red   = red   >> 3;
   green = green >> 3;
   blue  = blue  >> 3;

   vid->font_colour16 = (red << 11) | (green << 6) | blue;
}

static void sdl_dingux_init_font_lut(sdl_dingux_video_t *vid)
{
   size_t symbol_index;
   size_t i, j;

   /* Loop over all possible characters */
   for (symbol_index = 0;
        symbol_index < SDL_DINGUX_NUM_FONT_GLYPHS;
        symbol_index++)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset  = (i + j * FONT_WIDTH) >> 3;

            /* LUT value is 'true' if specified glyph
             * position contains a pixel */
            vid->font_lut[symbol_index][i + (j * FONT_WIDTH)] =
                  (bitmap_bin[FONT_OFFSET(symbol_index) + offset] & rem) > 0;
         }
      }
   }
}

static void sdl_dingux_blit_text16(
      sdl_dingux_video_t *vid,
      unsigned x, unsigned y,
      const char *str)
{
   uint16_t *screen_buf         = (uint16_t*)vid->screen->pixels;
   /* 16 bit - divide pitch by 2 */
   uint16_t screen_stride       = (uint16_t)(vid->screen->pitch >> 1);
   uint16_t screen_width        = vid->screen->w;
   uint16_t screen_height       = vid->screen->h;
   uint16_t shadow_color_buf[2] = {0};
   uint16_t color_buf[2];

   color_buf[0] = vid->font_colour16;
   color_buf[1] = 0;

   /* Check for out of bounds y coordinates */
   if (y + FONT_HEIGHT + 1 >= screen_height)
      return;

   while (!string_is_empty(str))
   {
      /* Check for out of bounds x coordinates */
      if (x + FONT_WIDTH_STRIDE + 1 >= screen_width)
         return;

      /* Deal with spaces first, for efficiency */
      if (*str == ' ')
         str++;
      else
      {
         uint16_t i, j;
         uint32_t symbol = utf8_walk(&str);

         /* Stupid hack: 'oe' ligatures are not really
          * standard extended ASCII, so we have to waste
          * CPU cycles performing a conversion from the
          * unicode values... */
         if (symbol == 339) /* Latin small ligature oe */
            symbol = 156;
         if (symbol == 338) /* Latin capital ligature oe */
            symbol = 140;

         if (symbol >= SDL_DINGUX_NUM_FONT_GLYPHS)
            continue;

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            uint32_t buff_offset = ((y + j) * screen_stride) + x;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (vid->font_lut[symbol][i + (j * FONT_WIDTH)])
               {
                  uint16_t *screen_buf_ptr = screen_buf + buff_offset + i;

                  /* Text pixel + right shadow */
                  memcpy(screen_buf_ptr, color_buf, sizeof(uint16_t));

                  /* Bottom shadow */
                  screen_buf_ptr += screen_stride;
                  memcpy(screen_buf_ptr, shadow_color_buf, sizeof(uint16_t));
               }
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
   }
}

static void sdl_dingux_blit_text32(
      sdl_dingux_video_t *vid,
      unsigned x, unsigned y,
      const char *str)
{
   uint32_t *screen_buf         = (uint32_t*)vid->screen->pixels;
   /* 32 bit - divide pitch by 4 */
   uint32_t screen_stride       = (uint32_t)(vid->screen->pitch >> 2);
   uint32_t screen_width        = vid->screen->w;
   uint32_t screen_height       = vid->screen->h;
   uint32_t shadow_color_buf[2] = {0};
   uint32_t color_buf[2];

   color_buf[0] = vid->font_colour32;
   color_buf[1] = 0;

   /* Check for out of bounds y coordinates */
   if (y + FONT_HEIGHT + 1 >= screen_height)
      return;

   while (!string_is_empty(str))
   {
      /* Check for out of bounds x coordinates */
      if (x + FONT_WIDTH_STRIDE + 1 >= screen_width)
         return;

      /* Deal with spaces first, for efficiency */
      if (*str == ' ')
         str++;
      else
      {
         uint32_t i, j;
         uint32_t symbol = utf8_walk(&str);

         /* Stupid hack: 'oe' ligatures are not really
          * standard extended ASCII, so we have to waste
          * CPU cycles performing a conversion from the
          * unicode values... */
         if (symbol == 339) /* Latin small ligature oe */
            symbol = 156;
         if (symbol == 338) /* Latin capital ligature oe */
            symbol = 140;

         if (symbol >= SDL_DINGUX_NUM_FONT_GLYPHS)
            continue;

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            uint32_t buff_offset = ((y + j) * screen_stride) + x;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (vid->font_lut[symbol][i + (j * FONT_WIDTH)])
               {
                  uint32_t *screen_buf_ptr = screen_buf + buff_offset + i;

                  /* Text pixel + right shadow */
                  memcpy(screen_buf_ptr, color_buf, sizeof(uint32_t));

                  /* Bottom shadow */
                  screen_buf_ptr += screen_stride;
                  memcpy(screen_buf_ptr, shadow_color_buf, sizeof(uint32_t));
               }
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
   }
}

static void sdl_dingux_gfx_free(void *data)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (!vid)
      return;

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   /* It is good manners to leave IPU scaling
    * parameters in the default state when
    * shutting down */
   dingux_ipu_set_aspect_ratio_enable(true);
   dingux_ipu_set_integer_scaling_enable(false);

   free(vid);
}

static void *sdl_dingux_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   sdl_dingux_video_t *vid         = NULL;
   settings_t *settings            = config_get_ptr();
   bool ipu_keep_aspect            = settings->bools.video_dingux_ipu_keep_aspect;
   bool ipu_integer_scaling        = settings->bools.video_scale_integer;
   const char *input_joypad_driver = settings->arrays.input_joypad_driver;
   uint32_t surface_flags          = (video->vsync) ?
         (SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_FULLSCREEN) :
         (SDL_HWSURFACE | SDL_FULLSCREEN);

   dingux_ipu_set_downscaling_enable(true);
   dingux_ipu_set_aspect_ratio_enable(ipu_keep_aspect);
   dingux_ipu_set_integer_scaling_enable(ipu_integer_scaling);

   if (SDL_WasInit(0) == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         return NULL;
   }
   else if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
      return NULL;

   vid = (sdl_dingux_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

#if defined(VERBOSE)
   RARCH_LOG("[sdl_dingux_gfx_init]\n"
             "   video %dx%d\n"
             "   rgb32 %d\n"
             "   smooth %d\n"
             "   input_scale %u\n"
             "   force_aspect %d\n"
             "   fullscreen %d\n"
             "   vsync %d\n"
             "   flags %u\n",
         video->width, video->height, (int)video->rgb32, (int)video->smooth,
         video->input_scale, (int)video->force_aspect, (int)video->fullscreen,
         (int)video->vsync, surface_flags);
#endif

   vid->screen = SDL_SetVideoMode(
         SDL_DINGUX_MENU_WIDTH, SDL_DINGUX_MENU_HEIGHT,
         video->rgb32 ? 32 : 16,
         surface_flags);

   if (!vid->screen)
   {
      RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());
      goto error;
   }

   vid->rgb32           = video->rgb32;
   vid->vsync           = video->vsync;
   vid->integer_scaling = ipu_integer_scaling;
   vid->menu_active     = false;
   vid->was_in_menu     = false;

   SDL_ShowCursor(SDL_DISABLE);

   if (input && input_data)
   {
      void *sdl_input = input_driver_init_wrap(
            &input_sdl, input_joypad_driver);

      if (sdl_input)
      {
         *input      = &input_sdl;
         *input_data = sdl_input;
      }
      else
      {
         *input      = NULL;
         *input_data = NULL;
      }
   }

   sdl_dingux_init_font_color(vid);
   sdl_dingux_init_font_lut(vid);

   return vid;

error:
   sdl_dingux_gfx_free(vid);
   return NULL;
}

static void sdl_dingux_set_output(
      sdl_dingux_video_t* vid,
      int width, int height, int pitch, bool rgb32)
{
   uint32_t surface_flags = (vid->vsync) ?
         (SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_FULLSCREEN) :
         (SDL_HWSURFACE | SDL_FULLSCREEN);

#if defined(VERBOSE)
    RARCH_LOG("[sdl_dingux_set_output]\n"
              "   current w %d h %d pitch %d\n"
              "   new_w %d new_h %d pitch %d\n"
              "   rgb32 %d\n"
              "   vsync %d\n"
              "   flags %u\n",
            vid->screen->w, vid->screen->h, vid->screen->pitch,
            width, height, pitch,
            (int)rgb32, (int)vid->vsync, surface_flags);
#endif

   vid->screen = SDL_SetVideoMode(
         width, height, rgb32 ? 32 : 16,
         surface_flags);

   if (!vid->screen)
      RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());
}

static void sdl_dingux_blit_frame16(uint16_t* dst, uint16_t* src,
      unsigned width, unsigned height,
      unsigned dst_pitch, unsigned src_pitch)
{
   uint16_t *in_ptr    = src;
   uint16_t *out_ptr   = dst;
   /* 16 bit - divide pitch by 2 */
   uint16_t in_stride  = (uint16_t)(src_pitch >> 1);
   uint16_t out_stride = (uint16_t)(dst_pitch >> 1);
   size_t y;

   for (y = 0; y < height; y++)
   {
      memcpy(out_ptr, in_ptr, width * sizeof(uint16_t));
      in_ptr  += in_stride;
      out_ptr += out_stride;
   }
}

static void sdl_dingux_blit_frame32(uint32_t* dst, uint32_t* src,
      unsigned width, unsigned height,
      unsigned dst_pitch, unsigned src_pitch)
{
   uint32_t *in_ptr    = src;
   uint32_t *out_ptr   = dst;
   /* 32 bit - divide pitch by 4 */
   uint32_t in_stride  = (uint32_t)(src_pitch >> 2);
   uint32_t out_stride = (uint32_t)(dst_pitch >> 2);
   size_t y;

   for (y = 0; y < height; y++)
   {
      memcpy(out_ptr, in_ptr, width * sizeof(uint32_t));
      in_ptr  += in_stride;
      out_ptr += out_stride;
   }
}

static bool sdl_dingux_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   sdl_dingux_video_t* vid = (sdl_dingux_video_t*)data;
#ifdef HAVE_MENU
   bool menu_is_alive      = video_info->menu_is_alive;
#endif

   if (unlikely(!frame))
      return true;

   /* Update video mode if width/height have changed */
   if (unlikely(
         ((vid->screen->w != width) ||
          (vid->screen->h != height)) &&
               !vid->menu_active))
      sdl_dingux_set_output(vid, width, height,
            pitch, vid->rgb32);

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (likely(!vid->menu_active))
   {
      /* Must always lock SDL surface before
       * manipulating raw pixel buffer */
      if (SDL_MUSTLOCK(vid->screen))
         SDL_LockSurface(vid->screen);

      /* Blit frame to SDL surface */
      if (vid->rgb32)
         sdl_dingux_blit_frame32(
               (uint32_t*)vid->screen->pixels,
               (uint32_t*)frame,
               width, height,
               vid->screen->pitch, pitch);
      else
         sdl_dingux_blit_frame16(
               (uint16_t*)vid->screen->pixels,
               (uint16_t*)frame,
               width, height,
               vid->screen->pitch, pitch);

      vid->was_in_menu = false;
   }
   else
   {
      /* If this is the first frame that the menu
       * is active, update video mode */
      if (!vid->was_in_menu)
      {
         sdl_dingux_set_output(vid,
               SDL_DINGUX_MENU_WIDTH, SDL_DINGUX_MENU_HEIGHT,
               SDL_DINGUX_MENU_WIDTH * sizeof(uint16_t), false);

         vid->was_in_menu = true;
      }

      if (SDL_MUSTLOCK(vid->screen))
         SDL_LockSurface(vid->screen);

      /* Fast copy of menu texture to SDL surface */
      memcpy(vid->screen->pixels, vid->menu_texture,
            SDL_DINGUX_MENU_WIDTH * SDL_DINGUX_MENU_HEIGHT * sizeof(uint16_t));
   }

   /* Print OSD text, if required */
   if (msg)
   {
      /* If menu is active, colour depth is overriden
       * to 16 bit */
      if (vid->rgb32 && !vid->menu_active)
         sdl_dingux_blit_text32(vid, FONT_WIDTH_STRIDE,
               vid->screen->h - (FONT_HEIGHT + FONT_WIDTH_STRIDE), msg);
      else
         sdl_dingux_blit_text16(vid, FONT_WIDTH_STRIDE,
               vid->screen->h - (FONT_HEIGHT + FONT_WIDTH_STRIDE), msg);
   }

   /* Pixel manipulation complete - unlock
    * SDL surface */
   if (SDL_MUSTLOCK(vid->screen))
      SDL_UnlockSurface(vid->screen);

   SDL_Flip(vid->screen);

   return true;
}

static void sdl_dingux_set_texture_enable(void *data, bool state, bool full_screen)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;
   (void)full_screen;

   if (vid->menu_active != state)
      vid->menu_active = state;
}

static void sdl_dingux_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (unlikely(
         rgb32 ||
         (width > SDL_DINGUX_MENU_WIDTH) ||
         (height > SDL_DINGUX_MENU_HEIGHT)))
      return;

   memcpy(vid->menu_texture, frame, width * height * sizeof(uint16_t));
}

static void sdl_dingux_gfx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;
   bool vsync              = !toggle;

   if (!vid || !vid->screen)
      return;

   /* Check whether vsync status has changed */
   if (vid->vsync != vsync)
   {
      vid->vsync = vsync;

      /* Update video mode */
      sdl_dingux_set_output(vid, vid->screen->w, vid->screen->h,
            vid->screen->pitch, vid->rgb32);
   }
}

static void sdl_dingux_gfx_check_window(sdl_dingux_video_t *vid)
{
   SDL_Event event;

   SDL_PumpEvents();
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUITMASK))
   {
      if (event.type != SDL_QUIT)
         continue;

      vid->quitting = true;
      break;
   }
}

static bool sdl_dingux_gfx_alive(void *data)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   sdl_dingux_gfx_check_window(vid);
   return !vid->quitting;
}

static bool sdl_dingux_gfx_focus(void *data)
{
   return true;
}

static bool sdl_dingux_gfx_suppress_screensaver(void *data, bool enable)
{
   return false;
}

static bool sdl_dingux_gfx_has_windowed(void *data)
{
   return false;
}

static void sdl_dingux_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = vp->full_width  = vid->screen->w;
   vp->height = vp->full_height = vid->screen->h;
}

static void sdl_dingux_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
}

static void sdl_dingux_apply_state_changes(void *data)
{
   sdl_dingux_video_t *vid  = (sdl_dingux_video_t*)data;
   settings_t *settings     = config_get_ptr();
   bool ipu_integer_scaling = (settings) ? settings->bools.video_scale_integer : false;

   if (!vid || !settings)
      return;

   /* Update integer scaling state, if required */
   if (vid->integer_scaling != ipu_integer_scaling)
   {
      dingux_ipu_set_integer_scaling_enable(ipu_integer_scaling);
      vid->integer_scaling = ipu_integer_scaling;
   }
}

static uint32_t sdl_dingux_get_flags(void *data)
{
   return 0;
}

static const video_poke_interface_t sdl_dingux_poke_interface = {
   sdl_dingux_get_flags,
   NULL,
   NULL,
   NULL,
   NULL, /* get_refresh_rate */
   sdl_dingux_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL,
   sdl_dingux_apply_state_changes,
   sdl_dingux_set_texture_frame,
   sdl_dingux_set_texture_enable,
   NULL,
   NULL, /* sdl_show_mouse */
   NULL, /* sdl_grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL  /* get_hw_render_interface */
};

static void sdl_dingux_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   *iface = &sdl_dingux_poke_interface;
}

static bool sdl_dingux_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   return false;
}

video_driver_t video_sdl_dingux = {
   sdl_dingux_gfx_init,
   sdl_dingux_gfx_frame,
   sdl_dingux_gfx_set_nonblock_state,
   sdl_dingux_gfx_alive,
   sdl_dingux_gfx_focus,
   sdl_dingux_gfx_suppress_screensaver,
   sdl_dingux_gfx_has_windowed,
   sdl_dingux_gfx_set_shader,
   sdl_dingux_gfx_free,
   "sdl_dingux",
   NULL,
   NULL, /* set_rotation */
   sdl_dingux_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   sdl_dingux_get_poke_interface
};
