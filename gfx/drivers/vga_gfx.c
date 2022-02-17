/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <dpmi.h>
#include <pc.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../common/vga_common.h"

#include "../font_driver.h"

#include "../../driver.h"
#include "../../verbosity.h"


static void vga_set_mode_13h(void)
{
  __dpmi_regs r = {0};

   r.x.ax = 0x13;
   __dpmi_int(0x10, &r);
}

static void vga_return_to_text_mode(void)
{
   __dpmi_regs r = {0};

   r.x.ax = 3;
   __dpmi_int(0x10, &r);
}

static void vga_upload_palette(void)
{
   unsigned i;
   unsigned char r = 0;
   unsigned char g = 0;
   unsigned char b = 0;

   outp(0x03c8, 0);

   /* RGB332 */
   for (i = 0; i < 256; i++)
   {
      if (i > 0 && i % 64 == 0)
      {
         r = 0;
         g = 0;
         b++;
      }
      else if (i > 0 && i % 8 == 0)
      {
         r = 0;
         g++;
      }

      outp(0x03c9, r * (63.0f / 7.0f));
      outp(0x03c9, g * (63.0f / 7.0f));
      outp(0x03c9, b * (63.0f / 3.0f));

      r++;
   }
}

static void vga_vsync(void)
{
   /* wait until any previous retrace has ended */
   do
   {
   }while (inportb(0x3da) & 8);

   /* wait until a new retrace has just begun */
   do
   {
   }while (!(inportb(0x3da) & 8));
}

static void vga_gfx_create(void)
{
   vga_set_mode_13h();
   vga_upload_palette();
}

static void *vga_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   vga_t *vga          = (vga_t*)calloc(1, sizeof(*vga));

   *input              = NULL;
   *input_data         = NULL;

   vga->vga_video_width    = video->width;
   vga->vga_video_height   = video->height;
   vga->vga_rgb32          = video->rgb32;

   if (video->rgb32)
   {
      vga->vga_video_pitch = video->width * 4;
      vga->vga_video_bits  = 32;
   }
   else
   {
      vga->vga_video_pitch = video->width * 2;
      vga->vga_video_bits  = 16;
   }

   vga->vga_frame          = (unsigned char*)malloc(VGA_WIDTH * VGA_HEIGHT);

   vga_gfx_create();

   if (video->font_enable)
      font_driver_init_osd(NULL,
            video,
            false,
            video->is_threaded, FONT_DRIVER_RENDER_VGA);

   return vga;
}

static bool vga_gfx_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned width, height, bits;
   size_t len                = 0;
   void *buffer              = NULL;
   const void *frame_to_copy = frame;
   bool draw                 = true;
   vga_t *vga                = (vga_t*)data;
#ifdef HAVE_MENU
   bool menu_is_alive        = video_info->menu_is_alive;
#endif

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (  vga->vga_video_width  != frame_width   ||
         vga->vga_video_height != frame_height  ||
         vga->vga_video_pitch  != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         vga->vga_video_width = frame_width;
         vga->vga_video_height = frame_height;
         vga->vga_video_pitch = pitch;
      }
   }

#ifdef HAVE_MENU
   if (vga->vga_menu_frame && menu_is_alive)
   {
      frame_to_copy = vga->vga_menu_frame;
      width         = vga->vga_menu_width;
      height        = vga->vga_menu_height;
      pitch         = vga->vga_menu_pitch;
      bits          = vga->vga_menu_bits;
   }
   else
#endif
   {
      width         = vga->vga_video_width;
      height        = vga->vga_video_height;
      pitch         = vga->vga_video_pitch;
      bits          = vga->vga_video_bits;

      if (frame_width == 4 && frame_height == 4 && (frame_width < width && frame_height < height))
         draw = false;

#ifdef HAVE_MENU
      if (menu_is_alive)
         draw = false;
#endif
   }

   if (draw)
   {
      vga_vsync();

      if (frame_to_copy == vga->vga_menu_frame)
         dosmemput(frame_to_copy,
               MIN(VGA_WIDTH,width)*MIN(VGA_HEIGHT,height), 0xA0000);
      else
      {
         if (bits == 32)
         {
            unsigned x, y;
            for (y = 0; y < VGA_HEIGHT; y++)
            {
               for (x = 0; x < VGA_WIDTH; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned    scaled_x = (width * x) / VGA_WIDTH;
                  unsigned    scaled_y = (height * y) / VGA_HEIGHT;
                  uint32_t pixel = ((uint32_t*)frame_to_copy)[width * scaled_y + scaled_x];

                  /* convert RGB888 to BGR332 */
                  unsigned r = ((pixel & 0xFF0000) >> 21);
                  unsigned g = ((pixel & 0x00FF00) >> 13);
                  unsigned b = ((pixel & 0x0000FF) >> 6);

                  vga->vga_frame[VGA_WIDTH * y + x] = (b << 6) | (g << 3) | r;
               }
            }

            dosmemput(vga->vga_frame, VGA_WIDTH*VGA_HEIGHT, 0xA0000);
         }
         else if (bits == 16)
         {
            unsigned x, y;

            for (y = 0; y < VGA_HEIGHT; y++)
            {
               for (x = 0; x < VGA_WIDTH; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned    scaled_x = (width * x) / VGA_WIDTH;
                  unsigned    scaled_y = (height * y) / VGA_HEIGHT;
                  unsigned short pixel = ((unsigned short*)frame_to_copy)[width * scaled_y + scaled_x];

                  /* convert RGB565 to BGR332 */
                  unsigned r = ((pixel & 0xF800) >> 13);
                  unsigned g = ((pixel & 0x07E0) >> 8);
                  unsigned b = ((pixel & 0x001F) >> 3);

                  vga->vga_frame[VGA_WIDTH * y + x] = (b << 6) | (g << 3) | r;
               }
            }

            dosmemput(vga->vga_frame, VGA_WIDTH*VGA_HEIGHT, 0xA0000);
         }
      }
   }

   if (msg)
      font_driver_render_msg(data, msg, NULL, NULL);

   return true;
}

static void vga_gfx_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }


static bool vga_gfx_alive(void *data)
{
   vga_t *vga = (vga_t*)data;
   /* TODO/FIXME - check if this is valid */
   video_driver_set_size(vga->vga_video_width, vga->vga_video_height);
   return true;
}

static bool vga_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool vga_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void vga_gfx_free(void *data)
{
   vga_t *vga = (vga_t*)data;

   if (!vga)
      return;

   if (vga->vga_frame)
      free(vga->vga_frame);
   vga->vga_frame = NULL;

   if (vga->vga_menu_frame)
      free(vga->vga_menu_frame);
   vga->vga_menu_frame = NULL;

   vga_return_to_text_mode();
}

static bool vga_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void vga_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   vga_t     *vga = (vga_t*)data;
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (vga->vga_menu_frame)
      free(vga->vga_menu_frame);
   vga->vga_menu_frame = NULL;

   if ( !vga->vga_menu_frame ||
         vga->vga_menu_width  != width  ||
         vga->vga_menu_height != height ||
         vga->vga_menu_pitch  != pitch)
      if (pitch && height)
         vga->vga_menu_frame = (unsigned char*)malloc(VGA_WIDTH * VGA_HEIGHT);

   if (vga->vga_menu_frame && frame && pitch && height)
   {
      unsigned x, y;

      if (!rgb32)
      {
         unsigned short *video_frame = (unsigned short*)frame;

         for (y = 0; y < VGA_HEIGHT; y++)
         {
            for (x = 0; x < VGA_WIDTH; x++)
            {
               /* scale incoming frame to fit the screen */
               unsigned scaled_x    = (width * x) / VGA_WIDTH;
               unsigned scaled_y    = (height * y) / VGA_HEIGHT;
               unsigned short pixel = video_frame[width * scaled_y + scaled_x];
               unsigned r           = ((pixel & 0xF000) >> 13);
               unsigned g           = ((pixel & 0xF00) >> 9);
               unsigned b           = ((pixel & 0xF0) >> 6);
               vga->vga_menu_frame[VGA_WIDTH * y + x] = (b << 6) | (g << 3) | r;
            }
         }
      }

      vga->vga_menu_width  = width;
      vga->vga_menu_height = height;
      vga->vga_menu_pitch  = pitch;
      vga->vga_menu_bits   = rgb32 ? 32 : 16;
   }
}

static uint32_t vga_get_flags(void *data) { return 0; }

static const video_poke_interface_t vga_poke_interface = {
   vga_get_flags,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   vga_set_texture_frame,
   NULL,
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

static void vga_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { *iface = &vga_poke_interface; }
void vga_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate) { }

video_driver_t video_vga = {
   vga_gfx_init,
   vga_gfx_frame,
   vga_gfx_set_nonblock_state,
   vga_gfx_alive,
   vga_gfx_focus,
   vga_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   vga_gfx_set_shader,
   vga_gfx_free,
   "vga",
   vga_gfx_set_viewport,
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
  vga_gfx_get_poke_interface,
};
