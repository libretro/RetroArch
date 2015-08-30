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

#include "../../general.h"
#include "../../driver.h"
#include "../video_viewport.h"

#include <vita2d.h>

#define SCREEN_W 960
#define SCREEN_H 544

typedef struct vita_video
{
  vita2d_texture *texture;

   bool vsync;
   bool rgb32;

   video_viewport_t vp;

   unsigned rotation;
   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;
} vita_video_t;

static void *vita2d_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
   (void)video;

   vita_video_t *vita = (vita_video_t *)calloc(1, sizeof(vita_video_t));

   if (!vita)
      return NULL;

   vita2d_init();
   vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
   vita2d_set_vblank_wait(video->vsync);

   vita->texture = vita2d_create_empty_texture(SCREEN_W, SCREEN_H);

   vita->vsync = video->vsync;
   vita->rgb32 = video->rgb32;

   return vita;
}

static bool vita2d_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   vita_video_t *vita = (vita_video_t *)data;
   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;

   unsigned int *tex_p = vita2d_texture_get_datap(vita->texture);
   unsigned int tex_stride = vita2d_texture_get_stride(vita->texture);
   const unsigned int *frame_p = frame;

   vita2d_start_drawing();
   vita2d_clear_screen();

   int i, j;
   for (i = 0; i < height; i++)
   {
      for (j = 0; j < width; j++)
      {
         tex_p[i + j * tex_stride] = frame_p[i + j * pitch];
      }
   }

   vita2d_draw_texture(vita->texture, 0, 0);

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

   vita2d_free_texture(vita->texture);

   vita2d_fini();
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

static void vita2d_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   (void)iface;
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
