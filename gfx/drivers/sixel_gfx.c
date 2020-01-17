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

#include <retro_miscellaneous.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../driver.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/sixel_common.h"

#ifndef _WIN32
#define HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#define HAVE_SYS_SELECT_H

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/select.h>
#endif

#ifndef SIXEL_PIXELFORMAT_BGRA8888
#error "Old version of libsixel detected, please upgrade to at least 1.6.0."
#endif

static unsigned char *sixel_menu_frame = NULL;
static unsigned sixel_menu_width       = 0;
static unsigned sixel_menu_height      = 0;
static unsigned sixel_menu_pitch       = 0;
static unsigned sixel_video_width      = 0;
static unsigned sixel_video_height     = 0;
static unsigned sixel_video_pitch      = 0;
static unsigned sixel_video_bits       = 0;
static unsigned sixel_menu_bits        = 0;
static double sixel_video_scale        = 1;
static bool sixel_rgb32                = false;
static bool sixel_menu_rgb32           = false;
static unsigned *sixel_temp_buf        = NULL;

static int sixel_write(char *data, int size, void *priv)
{
   return fwrite(data, 1, size, (FILE*)priv);
}

static SIXELSTATUS output_sixel(unsigned char *pixbuf, int width, int height,
      int ncolors, int pixelformat)
{
   sixel_output_t *context;
   sixel_dither_t *dither;
   SIXELSTATUS status;

   context = sixel_output_create(sixel_write, stdout);
   dither = sixel_dither_create(ncolors);
   status = sixel_dither_initialize(dither, pixbuf,
         width, height,
         pixelformat,
         SIXEL_LARGE_AUTO,
         SIXEL_REP_AUTO,
         SIXEL_QUALITY_AUTO);
   if (SIXEL_FAILED(status))
      return status;
   status = sixel_encode(pixbuf, width, height,
         pixelformat, dither, context);
   if (SIXEL_FAILED(status))
      return status;
   sixel_output_unref(context);
   sixel_dither_unref(dither);

   return status;
}

#ifdef HAVE_SYS_IOCTL_H
# ifdef HAVE_TERMIOS_H
static int wait_stdin(int usec)
{
#ifdef HAVE_SYS_SELECT_H
   fd_set rfds;
   struct timeval tv;
#endif  /* HAVE_SYS_SELECT_H */
   int ret = 0;

#ifdef HAVE_SYS_SELECT_H
   tv.tv_sec = usec / 1000000;
   tv.tv_usec = usec % 1000000;
   FD_ZERO(&rfds);
   FD_SET(STDIN_FILENO, &rfds);
   ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
#else
   (void) usec;
#endif  /* HAVE_SYS_SELECT_H */

   return ret;
}
# endif
#endif

static void scroll_on_demand(int pixelheight)
{
#ifdef HAVE_SYS_IOCTL_H
   struct winsize size = {0, 0, 0, 0};
#endif
#ifdef HAVE_TERMIOS_H
   struct termios old_termios;
   struct termios new_termios;
#endif
   int row = 0;
   int col = 0;
   int cellheight;
   int scroll;

#ifdef HAVE_SYS_IOCTL_H
   ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
   if (size.ws_ypixel <= 0)
   {
      printf("\033[H\0337");
      return;
   }
# ifdef HAVE_TERMIOS_H
   /* set the terminal to cbreak mode */
   tcgetattr(STDIN_FILENO, &old_termios);
   memcpy(&new_termios, &old_termios, sizeof(old_termios));
   new_termios.c_lflag &= ~(ECHO | ICANON);
   new_termios.c_cc[VMIN] = 1;
   new_termios.c_cc[VTIME] = 0;
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);

   /* request cursor position report */
   printf("\033[6n");

   if (wait_stdin(1000 * 1000) != (-1))
   {
      /* wait 1 sec */
      if (scanf("\033[%d;%dR", &row, &col) == 2)
      {
         cellheight = pixelheight * size.ws_row / size.ws_ypixel + 1;
         scroll = cellheight + row - size.ws_row + 1;
         printf("\033[%dS\033[%dA", scroll, scroll);
         printf("\0337");
      }
      else
      {
         printf("\033[H\0337");
      }
   }

   tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
# else
   printf("\033[H\0337");
# endif  /* HAVE_TERMIOS_H */
#else
   printf("\033[H\0337");
#endif  /* HAVE_SYS_IOCTL_H */
}

static void *sixel_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   gfx_ctx_input_t inp;
   void *ctx_data                       = NULL;
   settings_t *settings                 = config_get_ptr();
   sixel_t *sixel                       = (sixel_t*)calloc(1, sizeof(*sixel));
   const gfx_ctx_driver_t *ctx_driver   = NULL;
   const char *scale_str                = NULL;

   *input                               = NULL;
   *input_data                          = NULL;

   sixel_rgb32                          = video->rgb32;
   sixel_video_bits                     = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      sixel_video_pitch = video->width * 4;
   else
      sixel_video_pitch = video->width * 2;

   scale_str = getenv("SIXEL_SCALE");

   if (scale_str)
   {
      sixel_video_scale = atof(scale_str);

      /* just in case the conversion fails, pick something sane */
      if (!sixel_video_scale)
         sixel_video_scale = 1.0;
   }

   ctx_driver = video_context_driver_init_first(sixel,
         settings->arrays.video_context_driver,
         GFX_CTX_SIXEL_API, 1, 0, false, &ctx_data);

   if (!ctx_driver)
      goto error;

   if (ctx_data)
      sixel->ctx_data = ctx_data;

   sixel->ctx_driver = ctx_driver;
   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("[SIXEL]: Found SIXEL context: %s\n", ctx_driver->ident);

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (settings->bools.video_font_enable)
      font_driver_init_osd(sixel, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_SIXEL);

   RARCH_LOG("[SIXEL]: Init complete.\n");

   return sixel;

error:
   video_context_driver_destroy();
   if (sixel)
      free(sixel);
   return NULL;
}

static bool sixel_gfx_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   gfx_ctx_mode_t mode;
   const void *frame_to_copy = frame;
   unsigned width            = 0;
   unsigned height           = 0;
   unsigned bits             = sixel_video_bits;
   unsigned pixfmt           = SIXEL_PIXELFORMAT_RGB565;
   bool draw                 = true;
   sixel_t *sixel            = (sixel_t*)data;

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   if (sixel_video_width != frame_width || sixel_video_height != frame_height || sixel_video_pitch != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         sixel_video_width = frame_width;
         sixel_video_height = frame_height;
         sixel_video_pitch = pitch;
         sixel->screen_width = sixel_video_width * sixel_video_scale;
         sixel->screen_height = sixel_video_height * sixel_video_scale;
      }
   }

   if (sixel_menu_frame && video_info->menu_is_alive)
   {
      frame_to_copy = sixel_menu_frame;
      width         = sixel_menu_width;
      height        = sixel_menu_height;
      pitch         = sixel_menu_pitch;
      bits          = sixel_menu_bits;
   }
   else
   {
      width         = sixel_video_width;
      height        = sixel_video_height;
      pitch         = sixel_video_pitch;

      if (frame_width == 4 && frame_height == 4 && (frame_width < width && frame_height < height))
         draw = false;

      if (video_info->menu_is_alive)
         draw = false;
   }

   if (sixel->video_width != width || sixel->video_height != height)
   {
      scroll_on_demand(sixel->screen_height);

      sixel->video_width = width;
      sixel->video_height = height;

      if (sixel_temp_buf)
      {
         free(sixel_temp_buf);
      }

      sixel_temp_buf = (unsigned*)malloc(sixel->screen_width * sixel->screen_height * sizeof(unsigned));
   }

   if (bits == 16)
   {
      if (sixel_temp_buf)
      {
         if (frame_to_copy == sixel_menu_frame)
         {
            /* Scale and convert 16-bit RGBX4444 image to 32-bit RGBX8888. */
            unsigned x, y;

            for (y = 0; y < sixel->screen_height; y++)
            {
               for (x = 0; x < sixel->screen_width; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned scaled_x = (width * x) / sixel->screen_width;
                  unsigned scaled_y = (height * y) / sixel->screen_height;
                  unsigned short pixel = ((unsigned short*)frame_to_copy)[width * scaled_y + scaled_x];

                  /* convert RGBX4444 to RGBX8888 */
                  unsigned r = ((pixel & 0xF000) << 8) | ((pixel & 0xF000) << 4);
                  unsigned g = ((pixel & 0x0F00) << 4) | ((pixel & 0x0F00) << 0);
                  unsigned b = ((pixel & 0x00F0) << 0) | ((pixel & 0x00F0) >> 4);

                  sixel_temp_buf[sixel->screen_width * y + x] = 0xFF000000 | b | g | r;
               }
            }

            pixfmt = SIXEL_PIXELFORMAT_RGBA8888;
            frame_to_copy = sixel_temp_buf;
         }
         else
         {
            /* Scale and convert 16-bit RGB565 image to 32-bit RGBX8888. */
            unsigned x, y;

            for (y = 0; y < sixel->screen_height; y++)
            {
               for (x = 0; x < sixel->screen_width; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned scaled_x = (width * x) / sixel->screen_width;
                  unsigned scaled_y = (height * y) / sixel->screen_height;
                  unsigned short pixel = ((unsigned short*)frame_to_copy)[(pitch / (bits / 8)) * scaled_y + scaled_x];

                  /* convert RGB565 to RGBX8888 */
                  unsigned r = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
                  unsigned g = ((pixel & 0x07E0) << 5) | ((pixel & 0x0600) >> 1);
                  unsigned b = ((pixel & 0xF800) << 8) | ((pixel & 0xE000) << 3);

                  sixel_temp_buf[sixel->screen_width * y + x] = 0xFF000000 | b | g | r;
               }
            }

            pixfmt = SIXEL_PIXELFORMAT_BGRA8888;
            frame_to_copy = sixel_temp_buf;
         }
      }
      else
      {
         /* no temp buffer available yet */
      }
   }
   else
   {
      /* Scale 32-bit RGBX8888 image to output geometry. */
      unsigned x, y;

      for (y = 0; y < sixel->screen_height; y++)
      {
         for (x = 0; x < sixel->screen_width; x++)
         {
            /* scale incoming frame to fit the screen */
            unsigned scaled_x = (width * x) / sixel->screen_width;
            unsigned scaled_y = (height * y) / sixel->screen_height;
            unsigned pixel = ((unsigned*)frame_to_copy)[(pitch / (bits / 8)) * scaled_y + scaled_x];

            sixel_temp_buf[sixel->screen_width * y + x] = pixel;
         }
      }

      pixfmt = SIXEL_PIXELFORMAT_BGRA8888;
      frame_to_copy = sixel_temp_buf;
   }

   if (draw && sixel->screen_width > 0 && sixel->screen_height > 0)
   {
      printf("\0338");

      sixel->sixel_status = output_sixel((unsigned char*)frame_to_copy, sixel->screen_width, sixel->screen_height,
            SIXEL_COLORS, pixfmt);

      if (SIXEL_FAILED(sixel->sixel_status))
      {
         fprintf(stderr, "%s\n%s\n",
               sixel_helper_format_error(sixel->sixel_status),
               sixel_helper_get_additional_message());
      }
   }

   if (msg)
      font_driver_render_msg(sixel, video_info, msg, NULL, NULL);

   return true;
}

static void sixel_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool sixel_gfx_alive(void *data)
{
   gfx_ctx_size_t size_data;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
   bool is_shutdown     = rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL);
   sixel_t *sixel       = (sixel_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   sixel->ctx_driver->check_window(sixel->ctx_data,
            &quit, &resize, &temp_width, &temp_height, is_shutdown);

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return true;
}

static bool sixel_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool sixel_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool sixel_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void sixel_gfx_free(void *data)
{
   sixel_t *sixel = (sixel_t*)data;

   printf("\033\\");

   if (sixel_menu_frame)
   {
      free(sixel_menu_frame);
      sixel_menu_frame = NULL;
   }

   if (sixel_temp_buf)
   {
      free(sixel_temp_buf);
      sixel_temp_buf = NULL;
   }

   font_driver_free_osd();

   if (sixel)
      free(sixel);
}

static bool sixel_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void sixel_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void sixel_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (sixel_menu_frame)
   {
      free(sixel_menu_frame);
      sixel_menu_frame = NULL;
   }

   if (!sixel_menu_frame || sixel_menu_width != width || sixel_menu_height != height || sixel_menu_pitch != pitch)
      if (pitch && height)
         sixel_menu_frame = (unsigned char*)malloc(pitch * height);

   if (sixel_menu_frame && frame && pitch && height)
   {
      memcpy(sixel_menu_frame, frame, pitch * height);
      sixel_menu_width  = width;
      sixel_menu_height = height;
      sixel_menu_pitch  = pitch;
      sixel_menu_bits   = rgb32 ? 32 : 16;
   }
}

static void sixel_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   video_context_driver_get_video_output_size(&size_data);
}

static void sixel_get_video_output_prev(void *data)
{
   video_context_driver_get_video_output_prev();
}

static void sixel_get_video_output_next(void *data)
{
   video_context_driver_get_video_output_next();
}

static void sixel_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}

static const video_poke_interface_t sixel_poke_interface = {
   NULL,
   NULL,
   NULL,
   sixel_set_video_mode,
   NULL,
   NULL,
   sixel_get_video_output_size,
   sixel_get_video_output_prev,
   sixel_get_video_output_next,
   NULL,
   NULL,
   NULL,
   NULL,
#if defined(HAVE_MENU)
   sixel_set_texture_frame,
   NULL,
   font_driver_render_msg,
   NULL,
#else
   NULL,
   NULL,
   NULL,
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   NULL,
};

static void sixel_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &sixel_poke_interface;
}

static void sixel_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
}

bool sixel_has_menu_frame(void)
{
   return (sixel_menu_frame != NULL);
}

video_driver_t video_sixel = {
   sixel_gfx_init,
   sixel_gfx_frame,
   sixel_gfx_set_nonblock_state,
   sixel_gfx_alive,
   sixel_gfx_focus,
   sixel_gfx_suppress_screensaver,
   sixel_gfx_has_windowed,
   sixel_gfx_set_shader,
   sixel_gfx_free,
   "sixel",
   sixel_gfx_set_viewport,
   sixel_gfx_set_rotation,
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   sixel_gfx_get_poke_interface,
   NULL /* wrap_type_to_enum */
};
