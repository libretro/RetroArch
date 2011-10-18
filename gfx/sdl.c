/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SDL.h"
#include "driver.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "general.h"
#include "input/ssnes_sdl_input.h"
#include "gfx_common.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FREETYPE
#include "fonts.h"
#endif

static void convert_15bit_15bit_direct(uint16_t *out, unsigned outpitch, const uint16_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt);
static void convert_32bit_32bit_direct(uint32_t *out, unsigned outpitch, const uint32_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt);
static void convert_15bit_15bit_shift(uint16_t *out, unsigned outpitch, const uint16_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt);
static void convert_32bit_32bit_shift(uint32_t *out, unsigned outpitch, const uint32_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt);

typedef struct sdl_video sdl_video_t;
struct sdl_video
{
   SDL_Surface *screen, *buffer;
   bool quitting;
   bool rgb32;
   bool upsample;

   bool render32;

   void (*convert_15_func)(uint16_t*, unsigned, const uint16_t*, unsigned, unsigned, unsigned, const SDL_PixelFormat*);
   void (*convert_32_func)(uint32_t*, unsigned, const uint32_t*, unsigned, unsigned, unsigned, const SDL_PixelFormat*);

#ifdef HAVE_FREETYPE
   font_renderer_t *font;
   uint8_t font_r;
   uint8_t font_g;
   uint8_t font_b;
#endif
};

static void sdl_gfx_free(void *data)
{
   sdl_video_t *vid = data;
   if (!vid)
      return;

   if (vid->buffer)
      SDL_FreeSurface(vid->buffer);

   SDL_QuitSubSystem(SDL_INIT_VIDEO);

#ifdef HAVE_FREETYPE
   if (vid->font)
      font_renderer_free(vid->font);
#endif

   free(vid);
}

static void sdl_init_font(sdl_video_t *vid, const char *font_path, unsigned font_size)
{
#ifdef HAVE_FREETYPE
   const char *path = font_path;
   if (!*path)
      path = font_renderer_get_default_font();

   if (path)
   {
      vid->font = font_renderer_new(path, font_size);
      if (vid->font)
      {
         int r = g_settings.video.msg_color_r * 255;
         int g = g_settings.video.msg_color_g * 255;
         int b = g_settings.video.msg_color_b * 255;

         r = r < 0 ? 0 : (r > 255 ? 255 : r);
         g = g < 0 ? 0 : (g > 255 ? 255 : g);
         b = b < 0 ? 0 : (b > 255 ? 255 : b);

         // RGB888 -> RGB555
         if (!vid->render32)
         {
            r >>= 3;
            g >>= 3;
            b >>= 3;
         }

         vid->font_r = r;
         vid->font_g = g;
         vid->font_b = b;
      }
      else
         SSNES_WARN("Failed to init font.\n");
   }
   else
      SSNES_LOG("Did not find default font.\n");
#else
   (void)vid;
   (void)font_path;
   (void)font_size;
#endif
}

// Not very optimized, but hey :D
static void sdl_render_msg_15(sdl_video_t *vid, SDL_Surface *buffer, const char *msg, unsigned width, unsigned height, const SDL_PixelFormat *fmt)
{
#ifdef HAVE_FREETYPE
   if (!vid->font)
      return;

   struct font_output_list out;
   font_renderer_msg(vid->font, msg, &out);
   struct font_output *head = out.head;

   int base_x = g_settings.video.msg_pos_x * width;
   int base_y = (1.0 - g_settings.video.msg_pos_y) * height;

   unsigned rshift = fmt->Rshift;
   unsigned gshift = fmt->Gshift;
   unsigned bshift = fmt->Bshift;

   while (head)
   {
      int rbase_x = base_x + head->off_x;
      int rbase_y = base_y - head->off_y;
      if (rbase_y >= 0)
      {
         for (int y = 0; y < head->height && (y + rbase_y) < height; y++)
         {
            if (rbase_x < 0)
               continue;

            const uint8_t *a = head->output + head->pitch * y;
            uint16_t *out = (uint16_t*)buffer->pixels + (rbase_y - head->height + y) * (buffer->pitch >> 1) + rbase_x;

            for (int x = 0; x < head->width && (x + rbase_x) < width; x++)
            {
               unsigned blend = a[x];
               unsigned out_pix = out[x];
               unsigned r = (out_pix >> rshift) & 0x1f;
               unsigned g = (out_pix >> gshift) & 0x1f;
               unsigned b = (out_pix >> bshift) & 0x1f;

               unsigned out_r = (r * (256 - blend) + vid->font_r * blend) >> 8;
               unsigned out_g = (g * (256 - blend) + vid->font_g * blend) >> 8;
               unsigned out_b = (b * (256 - blend) + vid->font_b * blend) >> 8;
               out[x] = (out_r << rshift) | (out_g << gshift) | (out_b << bshift);
            }
         }
      }

      head = head->next;
   }

   font_renderer_free_output(&out);

#else
   (void)vid;
   (void)buffer;
   (void)msg;
   (void)width;
   (void)height;
#endif
}

static void sdl_render_msg_32(sdl_video_t *vid, SDL_Surface *buffer, const char *msg, unsigned width, unsigned height, const SDL_PixelFormat *fmt)
{
#ifdef HAVE_FREETYPE
   if (!vid->font)
      return;

   struct font_output_list out;
   font_renderer_msg(vid->font, msg, &out);
   struct font_output *head = out.head;

   int base_x = g_settings.video.msg_pos_x * width;
   int base_y = (1.0 - g_settings.video.msg_pos_y) * height;

   unsigned rshift = fmt->Rshift;
   unsigned gshift = fmt->Gshift;
   unsigned bshift = fmt->Bshift;

   while (head)
   {
      int rbase_x = base_x + head->off_x;
      int rbase_y = base_y - head->off_y;
      if (rbase_y >= 0)
      {
         for (int y = 0; y < head->height && (y + rbase_y) < height; y++)
         {
            if (rbase_x < 0)
               continue;

            const uint8_t *a = head->output + head->pitch * y;
            uint32_t *out = (uint32_t*)buffer->pixels + (rbase_y - head->height + y) * (buffer->pitch >> 2) + rbase_x;

            for (int x = 0; x < head->width && (x + rbase_x) < width; x++)
            {
               unsigned blend = a[x];
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

      head = head->next;
   }

   font_renderer_free_output(&out);

#else
   (void)vid;
   (void)buffer;
   (void)msg;
   (void)width;
   (void)height;
#endif
}

static void* sdl_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
#ifdef _WIN32
   gfx_set_dwm();
#endif

   SDL_InitSubSystem(SDL_INIT_VIDEO);

   sdl_video_t *vid = calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

   const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
   assert(video_info);
   unsigned full_x = video_info->current_w;
   unsigned full_y = video_info->current_h;
   SSNES_LOG("Detecting desktop resolution %ux%u.\n", full_x, full_y);

   if (!video->fullscreen)
      SSNES_LOG("Creating window @ %ux%u\n", video->width, video->height);

   vid->render32 = video->rgb32 && !g_settings.video.force_16bit;
   vid->screen = SDL_SetVideoMode(video->width, video->height, vid->render32 ? 32 : 15, SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF | (video->fullscreen ? SDL_FULLSCREEN : 0));

   if (!vid->screen && !g_settings.video.force_16bit && !video->rgb32)
   {
      vid->upsample = true;
      vid->screen = SDL_SetVideoMode(video->width, video->height, 32, SDL_HWSURFACE | SDL_HWACCEL | SDL_DOUBLEBUF | (video->fullscreen ? SDL_FULLSCREEN : 0));
      SSNES_WARN("SDL: 15-bit colors failed, attempting 32-bit colors.\n");
      vid->render32 = true;
   }

   if (!vid->screen)
   {
      SSNES_ERR("Failed to init SDL surface: %s\n", SDL_GetError());
      goto error;
   }

   SDL_ShowCursor(SDL_DISABLE);

   const SDL_PixelFormat *fmt = vid->screen->format;
   if (vid->render32)
   {
      SSNES_LOG("SDL: Creating 32-bit buffer.\n");
      vid->buffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 256 * video->input_scale, 256 * video->input_scale, 32,
            fmt->Rmask, fmt->Bmask, fmt->Gmask, fmt->Amask);
   }
   else
   {
      SSNES_LOG("SDL: Creating 15-bit buffer.\n");
      vid->buffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 256 * video->input_scale, 256 * video->input_scale, 15,
            fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
   }
   SSNES_LOG("[Debug]: SDL Pixel format: Rshift = %u, Gshift = %u, Bshift = %u\n", 
         (unsigned)fmt->Rshift, (unsigned)fmt->Gshift, (unsigned)fmt->Bshift);

   if (!vid->buffer)
   {
      SSNES_ERR("SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
      goto error;
   }

   sdl_input_t *sdl_input = input_sdl.init();
   if (sdl_input)
   {
      *input = &input_sdl;
      *input_data = sdl_input;
   }
   else
      *input = NULL;

   vid->rgb32 = video->rgb32;

   sdl_init_font(vid, g_settings.video.font_path, g_settings.video.font_size);


   if (fmt->Rshift == 10 && fmt->Gshift ==  5 && fmt->Bshift == 0) // XRGB1555
   {
      SSNES_LOG("SDL: 15-bit format matches. Fast blit.\n");
      vid->convert_15_func = convert_15bit_15bit_direct;
   }
   else
   {
      SSNES_LOG("SDL: 15-bit format does not match. Needs conversion.\n");
      vid->convert_15_func = convert_15bit_15bit_shift;
   }

   if (fmt->Rshift == 16 && fmt->Gshift == 8 && fmt->Bshift == 0) // ARGB8888
   {
      SSNES_LOG("SDL: 32-bit format matches. Fast blit.\n");
      vid->convert_32_func = convert_32bit_32bit_direct;
   }
   else
   {
      SSNES_LOG("SDL: 32-bit format does not match. Needs conversion.\n");
      vid->convert_32_func = convert_32bit_32bit_shift;
   }

   return vid;

error:
   sdl_gfx_free(vid);
   return NULL;
}

static inline uint16_t conv_pixel_32_15(uint32_t pix, const SDL_PixelFormat *fmt)
{
   uint16_t r = ((pix & 0x00f80000) >> 19) << fmt->Rshift;
   uint16_t g = ((pix & 0x0000f800) >> 11) << fmt->Gshift;
   uint16_t b = ((pix & 0x000000f8) >>  3) << fmt->Bshift;
   return r | g | b;
}

static inline uint32_t conv_pixel_15_32(uint16_t pix, const SDL_PixelFormat *fmt)
{
   uint32_t r = ((pix >> 10) & 0x1f) << (fmt->Rshift + 3);
   uint32_t g = ((pix >>  5) & 0x1f) << (fmt->Gshift + 3);
   uint32_t b = ((pix >>  0) & 0x1f) << (fmt->Bshift + 3);
   return r | g | b;
}

static void convert_32bit_15bit(uint16_t *out, unsigned outpitch, const uint32_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt)
{
   for (unsigned y = 0; y < height; y++)
   {
      for (unsigned x = 0; x < width; x++)
         out[x] = conv_pixel_32_15(input[x], fmt);

      out += outpitch >> 1;
      input += pitch >> 2;
   }
}

static void convert_15bit_32bit(uint32_t *out, unsigned outpitch, const uint16_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt)
{
   for (unsigned y = 0; y < height; y++)
   {
      for (unsigned x = 0; x < width; x++)
         out[x] = conv_pixel_15_32(input[x], fmt);

      out += outpitch >> 2;
      input += pitch >> 1;
   }
}

static void convert_15bit_15bit_direct(uint16_t *out, unsigned outpitch, const uint16_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt)
{
   for (unsigned y = 0; y < height; y++)
   {
      uint16_t *dest = out + ((y * outpitch) >> 1);
      const uint16_t *src = input + ((y * pitch) >> 1);
      memcpy(dest, src, width * sizeof(uint16_t));
   }
   (void)fmt;
}

static void convert_32bit_32bit_direct(uint32_t *out, unsigned outpitch, const uint32_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt)
{
   for (unsigned y = 0; y < height; y++)
   {
      uint32_t *dest = out + ((y * outpitch) >> 2);
      const uint32_t *src = input + ((y * pitch) >> 2);
      memcpy(dest, src, width * sizeof(uint32_t));
   }
   (void)fmt;
}

static void convert_15bit_15bit_shift(uint16_t *out, unsigned outpitch, const uint16_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt)
{
   for (unsigned y = 0; y < height; y++)
   {
      uint16_t *dest = out + ((y * outpitch) >> 1);
      const uint16_t *src = input + ((y * pitch) >> 1);
      for (unsigned x = 0; x < width; x++)
      {
         uint16_t color = src[x];
         uint16_t r = ((color >> 10) & 0x1f) << fmt->Rshift;
         uint16_t g = ((color >>  5) & 0x1f) << fmt->Gshift;
         uint16_t b = ((color >>  0) & 0x1f) << fmt->Bshift;
         dest[x] = r | g | b;
      }
   }
}

static void convert_32bit_32bit_shift(uint32_t *out, unsigned outpitch, const uint32_t *input, unsigned width, unsigned height, unsigned pitch, const SDL_PixelFormat *fmt)
{
   for (unsigned y = 0; y < height; y++)
   {
      uint32_t *dest = out + ((y * outpitch) >> 2);
      const uint32_t *src = input + ((y * pitch) >> 2);
      for (unsigned x = 0; x < width; x++)
      {
         uint32_t color = src[x];
         uint32_t r = ((color >> 16) & 0xff) << fmt->Rshift;
         uint32_t g = ((color >>  8) & 0xff) << fmt->Gshift;
         uint32_t b = ((color >>  0) & 0xff) << fmt->Bshift;
         dest[x] = r | g | b;
      }
   }
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

static bool sdl_gfx_frame(void *data, const void* frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   sdl_video_t *vid = data;

   if (SDL_MUSTLOCK(vid->buffer))
      SDL_LockSurface(vid->buffer);

   // :(
   // 15-bit -> 32-bit (Sometimes 15-bit won't work on "modern" OSes :\)
   if (vid->upsample)
   {
      convert_15bit_32bit(vid->buffer->pixels, vid->buffer->pitch, frame, width, height, pitch, vid->screen->format);
   }
   // 15-bit -> 15-bit
   else if (!vid->rgb32)
   {
      vid->convert_15_func(vid->buffer->pixels, vid->buffer->pitch, frame, width, height, pitch, vid->screen->format);
   }
   // 32-bit -> 15-bit
   else if (vid->rgb32 && g_settings.video.force_16bit)
   {
      convert_32bit_15bit(vid->buffer->pixels, vid->buffer->pitch, frame, width, height, pitch, vid->screen->format);
   }
   // 32-bit -> 32-bit
   else
   {
      vid->convert_32_func(vid->buffer->pixels, vid->buffer->pitch, frame, width, height, pitch, vid->screen->format);
   }
   
   if (SDL_MUSTLOCK(vid->buffer))
      SDL_UnlockSurface(vid->buffer);

   SDL_Rect src = {
      .x = 0,
      .y = 0,
      .w = width,
      .h = height
   };

   SDL_Rect dest = {
      .x = 0,
      .y = 0,
      .w = vid->screen->w,
      .h = vid->screen->h
   };

   SDL_SoftStretch(vid->buffer, &src, vid->screen, &dest);

   if (msg)
   {
      if ((!vid->rgb32 || g_settings.video.force_16bit) && !vid->upsample)
         sdl_render_msg_15(vid, vid->screen, msg, vid->screen->w, vid->screen->h, vid->screen->format);
      else
         sdl_render_msg_32(vid, vid->screen, msg, vid->screen->w, vid->screen->h, vid->screen->format);
   }

   char buf[128];
   if (gfx_window_title(buf, sizeof(buf)))
      SDL_WM_SetCaption(buf, NULL);

   SDL_Flip(vid->screen);

   return true;
}

static void sdl_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data; // Can SDL even do this?
   (void)state;
}

static bool sdl_gfx_alive(void *data)
{
   sdl_video_t *vid = data;
   check_window(vid);
   return !vid->quitting;
}

static bool sdl_gfx_focus(void *data)
{
   (void)data;
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
}


const video_driver_t video_sdl = {
   .init = sdl_gfx_init,
   .frame = sdl_gfx_frame,
   .alive = sdl_gfx_alive,
   .set_nonblock_state = sdl_gfx_set_nonblock_state,
   .focus = sdl_gfx_focus,
   .free = sdl_gfx_free,
   .ident = "sdl"
};

