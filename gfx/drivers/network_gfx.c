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
#include <retro_timers.h>
#include <stdlib.h>
#include <compat/strl.h>

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

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
#include "../common/network_common.h"

#define xstr(s) str(s)
#define str(s) #s

enum {
   NETWORK_VIDEO_PIXELFORMAT_RGBA8888 = 0,
   NETWORK_VIDEO_PIXELFORMAT_BGRA8888,
   NETWORK_VIDEO_PIXELFORMAT_RGB565
} network_video_pixelformat;

static unsigned char *network_menu_frame = NULL;
static unsigned network_menu_width       = 0;
static unsigned network_menu_height      = 0;
static unsigned network_menu_pitch       = 0;
static unsigned network_video_width      = 0;
static unsigned network_video_height     = 0;
static unsigned network_video_pitch      = 0;
static unsigned network_video_bits       = 0;
static unsigned network_menu_bits        = 0;
static bool network_rgb32                = false;
static bool network_menu_rgb32           = false;
static unsigned *network_video_temp_buf  = NULL;

static void *network_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   gfx_ctx_input_t inp;
   void *ctx_data                       = NULL;
   settings_t *settings                 = config_get_ptr();
   network_video_t *network             = (network_video_t*)calloc(1, sizeof(*network));
   const gfx_ctx_driver_t *ctx_driver   = NULL;
   struct addrinfo *addr = NULL, *next_addr = NULL;
   int fd;

   *input                               = NULL;
   *input_data                          = NULL;

   network_rgb32                        = video->rgb32;
   network_video_bits                   = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      network_video_pitch = video->width * 4;
   else
      network_video_pitch = video->width * 2;

   ctx_driver = video_context_driver_init_first(network,
         "network",
         GFX_CTX_NETWORK_VIDEO_API, 1, 0, false, &ctx_data);

   if (!ctx_driver)
      goto error;

   if (ctx_data)
      network->ctx_data = ctx_data;

   network->ctx_driver = ctx_driver;
   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("[network]: Found network video context: %s\n", ctx_driver->ident);

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (settings->bools.video_font_enable)
      font_driver_init_osd(network, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_NETWORK_VIDEO);

   strlcpy(network->address, xstr(NETWORK_VIDEO_HOST), sizeof(network->address));
   network->port = NETWORK_VIDEO_PORT;

   RARCH_LOG("[network] Connecting to host %s:%d\n", network->address, network->port);
try_connect:
   fd = socket_init((void**)&addr, network->port, network->address, SOCKET_TYPE_STREAM);

   next_addr = addr;

   while (fd >= 0)
   {
      {
         int ret = socket_connect(fd, (void*)next_addr, true);

         if (ret >= 0) /* && socket_nonblock(fd)) */
            break;

         socket_close(fd);
      }

      fd = socket_next((void**)&next_addr);
   }

   if (addr)
      freeaddrinfo_retro(addr);

   network->fd = fd;

#if 0
   socket_nonblock(network->fd);
#endif

   if (network->fd > 0)
      RARCH_LOG("[network]: Connected to host.\n");
   else
   {
      RARCH_LOG("[network]: Could not connect to host, retrying...\n");
      retro_sleep(1000);
      goto try_connect;
   }

   RARCH_LOG("[network]: Init complete.\n");

   return network;

error:
   video_context_driver_destroy();
   if (network)
      free(network);
   return NULL;
}

static bool network_gfx_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   gfx_ctx_mode_t mode;
   const void *frame_to_copy = frame;
   unsigned width            = 0;
   unsigned height           = 0;
   unsigned bits             = network_video_bits;
   unsigned pixfmt           = NETWORK_VIDEO_PIXELFORMAT_RGB565;
   bool draw                 = true;
   network_video_t *network  = (network_video_t*)data;

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   if (network_video_width != frame_width || network_video_height != frame_height || network_video_pitch != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         network_video_width = frame_width;
         network_video_height = frame_height;
         network_video_pitch = pitch;
         network->screen_width = network_video_width;
         network->screen_height = network_video_height;
      }
   }

   if (network_menu_frame && video_info->menu_is_alive)
   {
      frame_to_copy = network_menu_frame;
      width         = network_menu_width;
      height        = network_menu_height;
      pitch         = network_menu_pitch;
      bits          = network_menu_bits;
   }
   else
   {
      width         = network_video_width;
      height        = network_video_height;
      pitch         = network_video_pitch;

      if (frame_width == 4 && frame_height == 4 && (frame_width < width && frame_height < height))
         draw = false;

      if (video_info->menu_is_alive)
         draw = false;
   }

   if (network->video_width != width || network->video_height != height)
   {
      network->video_width = width;
      network->video_height = height;

      if (network_video_temp_buf)
      {
         free(network_video_temp_buf);
      }

      network_video_temp_buf = (unsigned*)malloc(network->screen_width * network->screen_height * sizeof(unsigned));
   }

   if (bits == 16)
   {
      if (network_video_temp_buf)
      {
         if (frame_to_copy == network_menu_frame)
         {
            /* Scale and convert 16-bit RGBX4444 image to 32-bit RGBX8888. */
            unsigned x, y;

            for (y = 0; y < network->screen_height; y++)
            {
               for (x = 0; x < network->screen_width; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned scaled_x = (width * x) / network->screen_width;
                  unsigned scaled_y = (height * y) / network->screen_height;
                  unsigned short pixel = ((unsigned short*)frame_to_copy)[width * scaled_y + scaled_x];

                  /* convert RGBX4444 to RGBX8888 */
                  unsigned r = ((pixel & 0xF000) << 8) | ((pixel & 0xF000) << 4);
                  unsigned g = ((pixel & 0x0F00) << 4) | ((pixel & 0x0F00) << 0);
                  unsigned b = ((pixel & 0x00F0) << 0) | ((pixel & 0x00F0) >> 4);

                  network_video_temp_buf[network->screen_width * y + x] = 0xFF000000 | b | g | r;
               }
            }

            pixfmt = NETWORK_VIDEO_PIXELFORMAT_RGBA8888;
            frame_to_copy = network_video_temp_buf;
         }
         else
         {
            /* Scale and convert 16-bit RGB565 image to 32-bit RGBX8888. */
            unsigned x, y;

            for (y = 0; y < network->screen_height; y++)
            {
               for (x = 0; x < network->screen_width; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned scaled_x = (width * x) / network->screen_width;
                  unsigned scaled_y = (height * y) / network->screen_height;
                  unsigned short pixel = ((unsigned short*)frame_to_copy)[(pitch / (bits / 8)) * scaled_y + scaled_x];

                  /* convert RGB565 to RGBX8888 */
                  unsigned r = ((pixel & 0x001F) << 3) | ((pixel & 0x001C) >> 2);
                  unsigned g = ((pixel & 0x07E0) << 5) | ((pixel & 0x0600) >> 1);
                  unsigned b = ((pixel & 0xF800) << 8) | ((pixel & 0xE000) << 3);

                  network_video_temp_buf[network->screen_width * y + x] = 0xFF000000 | b | g | r;
               }
            }

            pixfmt = NETWORK_VIDEO_PIXELFORMAT_BGRA8888;
            frame_to_copy = network_video_temp_buf;
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

      for (y = 0; y < network->screen_height; y++)
      {
         for (x = 0; x < network->screen_width; x++)
         {
            /* scale incoming frame to fit the screen */
            unsigned scaled_x = (width * x) / network->screen_width;
            unsigned scaled_y = (height * y) / network->screen_height;
            unsigned pixel = ((unsigned*)frame_to_copy)[(pitch / (bits / 8)) * scaled_y + scaled_x];

            network_video_temp_buf[network->screen_width * y + x] = pixel;
         }
      }

      pixfmt = NETWORK_VIDEO_PIXELFORMAT_BGRA8888;
      frame_to_copy = network_video_temp_buf;
   }

   if (draw && network->screen_width > 0 && network->screen_height > 0)
   {
      if (network->fd > 0)
         socket_send_all_blocking(network->fd, frame_to_copy, network->screen_width * network->screen_height * 4, true);
   }

   if (msg)
      font_driver_render_msg(network, video_info, msg, NULL, NULL);

   return true;
}

static void network_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool network_gfx_alive(void *data)
{
   gfx_ctx_size_t size_data;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
   bool is_shutdown     = rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL);
   network_video_t *network       = (network_video_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   network->ctx_driver->check_window(network->ctx_data,
            &quit, &resize, &temp_width, &temp_height, is_shutdown);

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return true;
}

static bool network_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool network_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool network_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void network_gfx_free(void *data)
{
   network_video_t *network = (network_video_t*)data;

   if (network_menu_frame)
   {
      free(network_menu_frame);
      network_menu_frame = NULL;
   }

   if (network_video_temp_buf)
   {
      free(network_video_temp_buf);
      network_video_temp_buf = NULL;
   }

   font_driver_free_osd();

   if (network->fd >= 0)
      socket_close(network->fd);

   if (network)
      free(network);
}

static bool network_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void network_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void network_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (network_menu_frame)
   {
      free(network_menu_frame);
      network_menu_frame = NULL;
   }

   if (!network_menu_frame || network_menu_width != width || network_menu_height != height || network_menu_pitch != pitch)
      if (pitch && height)
         network_menu_frame = (unsigned char*)malloc(pitch * height);

   if (network_menu_frame && frame && pitch && height)
   {
      memcpy(network_menu_frame, frame, pitch * height);
      network_menu_width  = width;
      network_menu_height = height;
      network_menu_pitch  = pitch;
      network_menu_bits   = rgb32 ? 32 : 16;
   }
}

static void network_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   video_context_driver_get_video_output_size(&size_data);
}

static void network_get_video_output_prev(void *data)
{
   video_context_driver_get_video_output_prev();
}

static void network_get_video_output_next(void *data)
{
   video_context_driver_get_video_output_next();
}

static void network_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}

static const video_poke_interface_t network_poke_interface = {
   NULL,
   NULL,
   NULL,
   network_set_video_mode,
   NULL,
   NULL,
   network_get_video_output_size,
   network_get_video_output_prev,
   network_get_video_output_next,
   NULL,
   NULL,
   NULL,
   NULL,
#if defined(HAVE_MENU)
   network_set_texture_frame,
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

static void network_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &network_poke_interface;
}

static void network_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
}

bool network_has_menu_frame(void)
{
   return (network_menu_frame != NULL);
}

video_driver_t video_network = {
   network_gfx_init,
   network_gfx_frame,
   network_gfx_set_nonblock_state,
   network_gfx_alive,
   network_gfx_focus,
   network_gfx_suppress_screensaver,
   network_gfx_has_windowed,
   network_gfx_set_shader,
   network_gfx_free,
   "network",
   network_gfx_set_viewport,
   network_gfx_set_rotation,
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   network_gfx_get_poke_interface,
   NULL /* wrap_type_to_enum */
};
