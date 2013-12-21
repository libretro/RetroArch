/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include <SDL/SDL.h>
#include "../driver.h"
#include <stdlib.h>
#include <string.h>
#include "../general.h"
#include "scaler/scaler.h"
#include "gfx_common.h"
#include "gfx_context.h"
#include "fonts/fonts.h"

#ifdef HAVE_X11
#include "context/x11_common.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "SDL/SDL_syswm.h"

static void sdl_gfx_get_poke_interface(void *data, const video_poke_interface_t **iface);
static void sdl_set_texture_frame(void *data, const void *frame, bool rgb32, unsigned width, unsigned height, float alpha);
static int rotate = 0;
static bool _rgui = false;
static void *rgui_buffer = NULL;

typedef struct sdl_video
{
   SDL_Surface *screen;
   bool quitting;

   void *font;
   const font_renderer_driver_t *font_driver;
   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;

   struct scaler_ctx scaler;
   unsigned last_width;
   unsigned last_height;
} sdl_video_t;

static void sdl_gfx_free(void *data)
{
   sdl_video_t *vid = (sdl_video_t*)data;
   if (!vid)
      return;

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   if (vid->font)
      vid->font_driver->free(vid->font);

   scaler_ctx_gen_reset(&vid->scaler);

   free(vid);

   if (rgui_buffer)
   {
      free(rgui_buffer);
      rgui_buffer = NULL;
   }
}

static void sdl_init_font(sdl_video_t *vid, const char *font_path, unsigned font_size)
{
   if (!g_settings.video.font_enable)
      return;

   if (font_renderer_create_default(&vid->font_driver, &vid->font))
   {
         int r = g_settings.video.msg_color_r * 255;
         int g = g_settings.video.msg_color_g * 255;
         int b = g_settings.video.msg_color_b * 255;

         r = r < 0 ? 0 : (r > 255 ? 255 : r);
         g = g < 0 ? 0 : (g > 255 ? 255 : g);
         b = b < 0 ? 0 : (b > 255 ? 255 : b);

         vid->font_r = r;
         vid->font_g = g;
         vid->font_b = b;
   }
   else
      RARCH_LOG("Could not initialize fonts.\n");
}

static void sdl_render_msg(sdl_video_t *vid, SDL_Surface *buffer,
      const char *msg, unsigned width, unsigned height, const SDL_PixelFormat *fmt)
{
   int x, y;
   if (!vid->font)
      return;

   struct font_output_list out;
   vid->font_driver->render_msg(vid->font, msg, &out);
   struct font_output *head = out.head;

   int msg_base_x = g_settings.video.msg_pos_x * width;
   int msg_base_y = (1.0 - g_settings.video.msg_pos_y) * height;

   unsigned rshift = fmt->Rshift;
   unsigned gshift = fmt->Gshift;
   unsigned bshift = fmt->Bshift;

   for (; head; head = head->next)
   {
      int base_x = msg_base_x + head->off_x;
      int base_y = msg_base_y - head->off_y - head->height;

      int glyph_width  = head->width;
      int glyph_height = head->height;

      const uint8_t *src = head->output;

      if (base_x < 0)
      {
         src -= base_x;
         glyph_width += base_x;
         base_x = 0;
      }

      if (base_y < 0)
      {
         src -= base_y * (int)head->pitch;
         glyph_height += base_y;
         base_y = 0;
      }

      int max_width  = width - base_x;
      int max_height = height - base_y;

      if (max_width <= 0 || max_height <= 0)
         continue;

      if (glyph_width > max_width)
         glyph_width = max_width;
      if (glyph_height > max_height)
         glyph_height = max_height;

      uint32_t *out = (uint32_t*)buffer->pixels + base_y * (buffer->pitch >> 2) + base_x;

      for (y = 0; y < glyph_height; y++, src += head->pitch, out += buffer->pitch >> 2)
      {
         for (x = 0; x < glyph_width; x++)
         {
            unsigned blend = src[x];
            unsigned out_pix = out[x];
            unsigned r = (out_pix >> rshift) & 0xff;
            unsigned g = (out_pix >> gshift) & 0xff;
            unsigned b = (out_pix >> bshift) & 0xff;

            unsigned out_r = (r * (256 - blend) + vid->font_r * blend) >> 8;
            unsigned out_g = (g * (256 - blend) + vid->font_g * blend) >> 8;
            unsigned out_b = (b * (256 - blend) + vid->font_b * blend) >> 8;
            out[x] = (out_r << rshift) | (out_g << gshift) | (out_b << bshift);
         }
      }
   }

   vid->font_driver->free_output(vid->font, &out);
}

static void sdl_gfx_set_handles(void)
{
   // SysWMinfo headers are broken on OSX. :(
#if defined(_WIN32)
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (SDL_GetWMInfo(&info) == 1)
   {
      driver.display_type  = RARCH_DISPLAY_WIN32;
      driver.video_display = 0;
      driver.video_window  = (uintptr_t)info.window;
   }
#elif defined(HAVE_X11)
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (SDL_GetWMInfo(&info) == 1)
   {
      driver.display_type  = RARCH_DISPLAY_X11;
      driver.video_display = (uintptr_t)info.info.x11.display;
      driver.video_window  = (uintptr_t)info.info.x11.window;
   }
#endif
}

static void *sdl_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
#ifdef _WIN32
   gfx_set_dwm();
#endif

#ifdef HAVE_X11
   XInitThreads();
#endif

   SDL_InitSubSystem(SDL_INIT_VIDEO);

   sdl_video_t *vid = (sdl_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

   const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
   rarch_assert(video_info);
   unsigned full_x = video_info->current_w;
   unsigned full_y = video_info->current_h;
   RARCH_LOG("Detecting desktop resolution %ux%u.\n", full_x, full_y);

   void *sdl_input = NULL;

   if (!video->fullscreen)
      RARCH_LOG("Creating window @ %ux%u\n", video->width, video->height);

   vid->screen = SDL_SetVideoMode(video->width, video->height, 32,
         SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF | (video->fullscreen ? SDL_FULLSCREEN : 0));

   // We assume that SDL chooses ARGB8888.
   // Assuming this simplifies the driver *a ton*.

   if (!vid->screen)
   {
      RARCH_ERR("Failed to init SDL surface: %s\n", SDL_GetError());
      goto error;
   }

   if (video->fullscreen)
      SDL_ShowCursor(SDL_DISABLE);

   sdl_gfx_set_handles();

   if (input && input_data)
   {
      sdl_input = input_sdl.init();
      if (sdl_input)
      {
         *input = &input_sdl;
         *input_data = sdl_input;
      }
      else
      {
         *input = NULL;
         *input_data = NULL;
      }
   }

   sdl_init_font(vid, g_settings.video.font_path, g_settings.video.font_size);

   vid->scaler.scaler_type = video->smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;
   vid->scaler.in_fmt  = video->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
   vid->scaler.out_fmt = SCALER_FMT_ARGB8888;

   return vid;

error:
   sdl_gfx_free(vid);
   return NULL;
}

static void check_window(sdl_video_t *vid)
{
   SDL_Event event;
   while (SDL_PollEvent(&event))
   {
      switch (event.type)
      {
         case SDL_QUIT:
            vid->quitting = true;
            break;

         default:
            break;
      }
   }
}

static bool sdl_gfx_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   if (!frame || _rgui)
      return true;

   sdl_video_t *vid = (sdl_video_t*)data;

   vid->scaler.in_stride = pitch;
   if (width != vid->last_width || height != vid->last_height)
   {
      vid->scaler.in_width  = width;
      vid->scaler.in_height = height;
      if (vid->scaler.in_stride == width * sizeof(uint16_t))
         vid->scaler.in_fmt = SCALER_FMT_RGB565;
      else
         vid->scaler.in_fmt = SCALER_FMT_ARGB8888;

      vid->scaler.out_width  = vid->screen->w;
      vid->scaler.out_height = vid->screen->h;
      vid->scaler.out_stride = vid->screen->pitch;
      vid->scaler.out_fmt = SCALER_FMT_ARGB8888;

      scaler_ctx_gen_filter(&vid->scaler);

      vid->last_width  = width;
      vid->last_height = height;
   }

   if (SDL_MUSTLOCK(vid->screen))
      SDL_LockSurface(vid->screen);

   RARCH_PERFORMANCE_INIT(sdl_scale);
   RARCH_PERFORMANCE_START(sdl_scale);
   scaler_ctx_scale(&vid->scaler, vid->screen->pixels, frame);
   RARCH_PERFORMANCE_STOP(sdl_scale);

   if (msg)
      sdl_render_msg(vid, vid->screen, msg, vid->screen->w, vid->screen->h, vid->screen->format);

   if (SDL_MUSTLOCK(vid->screen))
      SDL_UnlockSurface(vid->screen);

   char buf[128];
   if (gfx_get_fps(buf, sizeof(buf), NULL, 0))
      SDL_WM_SetCaption(buf, NULL);

   SDL_Flip(vid->screen);
   g_extern.frame_count++;

   return true;
}

static void sdl_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data; // Can SDL even do this?
   (void)state;
}

static bool sdl_gfx_alive(void *data)
{
   sdl_video_t *vid = (sdl_video_t*)data;
   check_window(vid);
   return !vid->quitting;
}

static bool sdl_gfx_focus(void *data)
{
   (void)data;
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
}

static void sdl_gfx_viewport_info(void *data, struct rarch_viewport *vp)
{
   sdl_video_t *vid = (sdl_video_t*)data;
   vp->x = vp->y = 0;
   vp->width  = vp->full_width  = vid->screen->w;
   vp->height = vp->full_height = vid->screen->h;
}

static void sdl_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   (void)data;
   g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
}

static void sdl_apply_state_changes(void *data)
{
   (void)data;
}

static void sdl_set_texture_frame(void *data, const void *frame, bool rgb32, unsigned width, unsigned height, float alpha)
{
   if (!rgui_buffer)
	  rgui_buffer = (uint32_t*)malloc(width * height * sizeof(uint32_t));

   sdl_video_t *vid = (sdl_video_t*)data;

   vid->scaler.in_stride = width * sizeof(uint32_t);
   vid->scaler.in_width  = width;
   vid->scaler.in_height = height;
   vid->scaler.in_fmt = SCALER_FMT_ARGB8888;

   vid->scaler.out_width  = vid->screen->w;
   vid->scaler.out_height = vid->screen->h;
   vid->scaler.out_stride = vid->screen->pitch;
   vid->scaler.out_fmt = SCALER_FMT_ARGB8888;

   scaler_ctx_gen_filter(&vid->scaler);

   vid->last_width  = width;
   vid->last_height = height;

   uint32_t *dst = (uint32_t*)rgui_buffer;
   const uint16_t *src = (const uint16_t*)frame;
   for (unsigned h = 0; h < height; h++, dst += width * sizeof(uint32_t) >> 2, src += width)
   {
      for (unsigned w = 0; w < width; w++)
      {
         uint16_t c = src[w];
         uint32_t r = (c >> 12) & 0xf;
         uint32_t g = (c >>  8) & 0xf;
         uint32_t b = (c >>  4) & 0xf;
         uint32_t a = (c >>  0) & 0xf;
         r = ((r << 4) | r) << 16;
         g = ((g << 4) | g) <<  8;
         b = ((b << 4) | b) <<  0;
         a = ((a << 4) | a) << 24;
         dst[w] = r | g | b | a;
      }
   }

   if (SDL_MUSTLOCK(vid->screen))
      SDL_LockSurface(vid->screen);

   scaler_ctx_scale(&vid->scaler, vid->screen->pixels, rgui_buffer);

   if (SDL_MUSTLOCK(vid->screen))
      SDL_UnlockSurface(vid->screen);

   char buf[128];
   if (gfx_get_fps(buf, sizeof(buf), NULL, 0))
      SDL_WM_SetCaption(buf, NULL);

   SDL_Flip(vid->screen);
   g_extern.frame_count++;
}

static void sdl_set_texture_enable(void *data, bool state, bool full_screen)
{
   (void)data;
   _rgui = state;
}

static void sdl_set_osd_msg(void *data, const char *msg, void *userdata)
{
   (void)data;
   (void)userdata;
}

static void sdl_show_mouse(void *data, bool state)
{
   (void)data;
}

static const video_poke_interface_t sdl_poke_interface = {
   NULL,
#ifdef HAVE_FBO
   NULL,
   NULL,
#endif
   sdl_set_aspect_ratio,
   sdl_apply_state_changes,
#if defined(HAVE_RGUI) || defined(HAVE_RMENU)
   sdl_set_texture_frame,
   sdl_set_texture_enable,
#endif
   sdl_set_osd_msg,

   sdl_show_mouse,
};

static void sdl_gfx_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &sdl_poke_interface;
}

const video_driver_t video_sdl = {
   sdl_gfx_init,
   sdl_gfx_frame,
   sdl_gfx_set_nonblock_state,
   sdl_gfx_alive,
   sdl_gfx_focus,
   NULL,
   sdl_gfx_free,
   "sdl",

#ifdef HAVE_MENU
   NULL,
#endif

   NULL,
   sdl_gfx_viewport_info,
   NULL,

#ifdef HAVE_OVERLAY
   NULL,
#endif
   sdl_gfx_get_poke_interface,
};

