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

#include <gfx/video_frame.h>
#include <retro_assert.h>
#include "../../verbosity.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

#include "frontend/frontend_driver.h"

#include "../font_driver.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../configuration.h"
#include "../../retroarch.h"

#include <go2/display.h>

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define ALIGN(val, align)   (((val) + (align) - 1) & ~((align) - 1))

#define NATIVE_WIDTH 480
#define NATIVE_HEIGHT 320

#define NUM_PAGES 2

typedef struct oga_video
{
    go2_presenter_t* presenter;
    go2_display_t* display;
    go2_surface_t* frame;

    go2_frame_buffer_t* frameBuffer[NUM_PAGES];
    int cur_page;
    bool threaded;

    const font_renderer_driver_t *font_driver;
    void *font;

    go2_surface_t* menu_surface;
    const void* menu_frame;
    unsigned menu_width;
    unsigned menu_height;
    unsigned menu_pitch;

    char menu_buf[NATIVE_WIDTH*NATIVE_HEIGHT*4];
} oga_video_t;

go2_rotation_t oga_rotation = GO2_ROTATION_DEGREES_0;

static void oga_gfx_free(void *data)
{
   unsigned i;
   oga_video_t *vid = (oga_video_t*)data;

   if (!vid)
      return;

   if (vid->font)
   {
      vid->font_driver->free(vid->font);
      vid->font_driver = NULL;
   }

   for (i = 0; i < NUM_PAGES; ++i)
   {
      go2_frame_buffer_t* frameBuffer = vid->frameBuffer[i];
      go2_surface_t* surface = go2_frame_buffer_surface_get(frameBuffer);

      go2_frame_buffer_destroy(frameBuffer);
      go2_surface_destroy(surface);
   }

   go2_surface_destroy(vid->frame);
   go2_surface_destroy(vid->menu_surface);
   go2_presenter_destroy(vid->presenter);
   go2_display_destroy(vid->display);

   free(vid);
   vid = NULL;
}

static void *oga_gfx_init(const video_info_t *video,
        input_driver_t **input, void **input_data)
{
   oga_video_t     *vid = NULL;
   settings_t *settings = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   frontend_driver_install_signal_handler();

   if (input && input_data)
   {
      void* udev = input_udev.init(settings->arrays.input_joypad_driver);
      if (udev)
      {
         *input       = &input_udev;
         *input_data  = udev;
      }
      else
         *input = NULL;
   }

   vid = (oga_video_t*)calloc(1, sizeof(*vid));

   vid->menu_frame = NULL;
   vid->menu_width = 0;
   vid->menu_height = 0;
   vid->menu_pitch = 0;
   vid->display = go2_display_create();
   vid->presenter = go2_presenter_create(vid->display, DRM_FORMAT_RGB565, 0xff000000, false);
   vid->menu_surface = go2_surface_create(vid->display, NATIVE_WIDTH, NATIVE_HEIGHT, DRM_FORMAT_XRGB8888);
   vid->font = NULL;
   vid->font_driver = NULL;

   vid->threaded = video->is_threaded;

   int aw = MAX(ALIGN(av_info->geometry.max_width, 32), NATIVE_WIDTH);
   int ah = MAX(ALIGN(av_info->geometry.max_height, 32), NATIVE_HEIGHT);

   RARCH_LOG("oga_gfx_init video %dx%d rgb32 %d smooth %d input_scale %u force_aspect %d"
           " fullscreen %d aw %d ah %d rgb %d threaded %d\n",
         video->width, video->height, video->rgb32, video->smooth, video->input_scale,
         video->force_aspect, video->fullscreen, aw, ah, video->rgb32, video->is_threaded);

   vid->frame = go2_surface_create(vid->display, aw, ah, video->rgb32 ? DRM_FORMAT_XRGB8888 : DRM_FORMAT_RGB565);

   /* bitmap only for now */
   if (settings->bools.video_font_enable)
   {
      vid->font_driver = &bitmap_font_renderer;
      vid->font = vid->font_driver->init("", settings->floats.video_font_size);
   }

   for (int i = 0; i < NUM_PAGES; ++i)
   {
      go2_surface_t* surface = go2_surface_create(vid->display, NATIVE_HEIGHT, NATIVE_WIDTH,
            video->rgb32 ? DRM_FORMAT_XRGB8888 : DRM_FORMAT_XRGB8888);
      vid->frameBuffer[i] = go2_frame_buffer_create(surface);
   }
   vid->cur_page = 0;

   return vid;
}

int get_message_width(oga_video_t* vid, const char* msg)
{
    int width = 0;
    for (const char* c = msg; *c; c++)
    {
        const struct font_glyph* glyph = vid->font_driver->get_glyph(vid->font, *c);
        if (unlikely(!glyph))
            continue;

        width += glyph->advance_x;
    }
    return width;
}

static void render_msg(oga_video_t* vid,
      go2_surface_t* surface, const char* msg, int width, int bpp)
{
   const struct font_atlas* atlas = vid->font_driver->get_atlas(vid->font);
   int                  msg_width = get_message_width(vid, msg);
   int                     dest_x = MAX(0, width - get_message_width(vid, msg));
   int                dest_stride = go2_surface_stride_get(surface);
   const char                 *c  = msg;

   while (*c)
   {
      const struct font_glyph* g = vid->font_driver->get_glyph(vid->font, *c);
      if (!g)
         continue;
      if (dest_x + g->advance_x >= width)
         break;

      const uint8_t* source = atlas->buffer + g->atlas_offset_y * atlas->width + g->atlas_offset_x;
      uint8_t* dest = (uint8_t*)go2_surface_map(surface) + dest_x * bpp;

      for (int y = 0; y < g->height; y++)
      {
         for (int x = 0; x < g->advance_x; x++)
         {
            uint8_t px = (x < g->width) ? *(source++) : 0x00;
            if (bpp == 4)
            {
               *(dest++) = px;
               *(dest++) = px;
               *(dest++) = px;
               *(dest++) = px;
            }
            else
            {
               *(dest++) = px;
               *(dest++) = px;
            }
         }
         dest += dest_stride - g->advance_x * bpp;
         source += atlas->width - g->width;
      }

      c++;
      dest_x += g->advance_x;
   }
}

static bool oga_gfx_frame(void *data, const void *frame, unsigned width,
        unsigned height, uint64_t frame_count,
        unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   int out_w, out_h;
   int yy, stride, dst_stride;
   int out_x                  = 0;
   int out_y                  = 0;
   go2_display_t             *display = NULL;
   go2_frame_buffer_t *dstFrameBuffer = NULL;
   go2_surface_t          *dstSurface = NULL;
   uint8_t                      *dst  = NULL;
   oga_video_t                  *vid  = (oga_video_t*)data;
   go2_surface_t         *dst_surface = vid->frame;
   uint8_t                      *src  = (uint8_t*)frame;
   int                            bpp = go2_drm_format_get_bpp(
         go2_surface_format_get(dst_surface)) / 8;
   bool                 menu_is_alive = video_info->menu_is_alive;

   if (unlikely(!frame || width == 0 || height == 0))
      return true;

   if (unlikely(video_info->input_driver_nonblock_state) && !vid->threaded)
   {
      if (frame_count % 4 != 0)
          return true;
   }

#ifdef HAVE_MENU
   if (unlikely(menu_is_alive))
   {
      if (unlikely(vid->menu_width == 0))
         return true;
      menu_driver_frame(menu_is_alive, video_info);
      dst_surface = vid->menu_surface;
      src         = (uint8_t*)vid->menu_frame;
      width       = vid->menu_width;
      height      = vid->menu_height;
      pitch       = vid->menu_pitch;
      bpp         = vid->menu_pitch / vid->menu_width;
   }
#endif

   /* copy buffer to surface */
   dst        = (uint8_t*)go2_surface_map(dst_surface);
   yy         = height;
   stride     = width * bpp;
   dst_stride = go2_surface_stride_get(dst_surface);

   while (yy > 0)
   {
      memcpy(dst, src, stride);
      src += pitch;
      dst += dst_stride;
      --yy;
   }

   out_w = NATIVE_WIDTH;
   out_h = NATIVE_HEIGHT;

   if ((out_w != width || out_h != height))
   {
      out_w = MIN(out_h * video_driver_get_aspect_ratio(), NATIVE_WIDTH);
      out_x = MAX((NATIVE_WIDTH - out_w) / 2, 0);
   }

   if (msg && vid->font)
      render_msg(vid, dst_surface, msg, width, bpp);

   dstFrameBuffer = vid->frameBuffer[vid->cur_page];
   dstSurface     = go2_frame_buffer_surface_get(dstFrameBuffer);

   go2_surface_blit(dst_surface, 0, 0, width, height,
         dstSurface, out_y, out_x, out_h, out_w,
         !menu_is_alive ? oga_rotation : GO2_ROTATION_DEGREES_270, 2);

   display = go2_presenter_display_get(vid->presenter);
   go2_display_present(display, dstFrameBuffer);
   vid->cur_page = !vid->cur_page;

   return true;
}

static void oga_set_texture_frame(void *data, const void *frame, bool rgb32,
        unsigned width, unsigned height, float alpha)
{
    oga_video_t *vid = (oga_video_t*)data;

    vid->menu_width = width;
    vid->menu_height = height;
    vid->menu_pitch = width * 4;


   /* Borrowed from drm_gfx
    *
    * We have to go on a pixel format conversion adventure
    * for now, until we can convince RGUI to output
    * in an 8888 format. */
   unsigned int src_pitch        = width * 2;
   unsigned int dst_pitch        = width * 4;
   unsigned int dst_width        = width;
   uint32_t line[dst_width];

   /* The output pixel array with the converted pixels. */
   char *frame_output = vid->menu_buf;

   /* Remember, memcpy() works with 8bits pointers for increments. */
   char *dst_base_addr           = frame_output;

   for (int i = 0; i < height; i++)
   {
      for (int j = 0; j < src_pitch / 2; j++)
      {
         uint16_t src_pix = *((uint16_t*)frame + (src_pitch / 2 * i) + j);
         /* The hex AND is for keeping only the part we need for each component. */
         uint32_t R = (src_pix << 8) & 0x00FF0000;
         uint32_t G = (src_pix << 4) & 0x0000FF00;
         uint32_t B = (src_pix << 0) & 0x000000FF;
         line[j] = (0 | R | G | B);
      }
      memcpy(dst_base_addr + (dst_pitch * i), (char*)line, dst_pitch);
   }

    if (unlikely(!vid->menu_frame))
        vid->menu_frame = frame_output;
}

static void oga_gfx_set_nonblock_state(void *a, bool b, bool c, unsigned d)
{
}

static bool oga_gfx_alive(void *data)
{
    return !frontend_driver_get_signal_handler_state();
}

static bool oga_gfx_focus(void *data)
{
    (void)data;
    return true;
}

static bool oga_gfx_suppress_screensaver(void *data, bool enable)
{
    (void)data;
    (void)enable;
    return false;
}

static bool oga_gfx_has_windowed(void *data)
{
    (void)data;
    return false;
}

static void oga_gfx_viewport_info(void *data, struct video_viewport *vp)
{
    oga_video_t *vid = (oga_video_t*)data;
    vp->x = 0;
    vp->y = 0;
    vp->width = vp->full_width = NATIVE_WIDTH;
    vp->height = vp->full_height = NATIVE_HEIGHT;
}

static const video_poke_interface_t oga_poke_interface = {
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
    NULL,
    oga_set_texture_frame,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

void oga_set_rotation(void *data, unsigned rotation)
{
   /* called before init? */
   (void)data;
   switch (rotation)
   {
      case 0:
         oga_rotation = GO2_ROTATION_DEGREES_270;
         break;
      case 1:
         oga_rotation = GO2_ROTATION_DEGREES_180;
         break;
      case 2:
         oga_rotation = GO2_ROTATION_DEGREES_90;
         break;
      case 3:
         oga_rotation = GO2_ROTATION_DEGREES_0;
         break;
      default:
         RARCH_ERR("Unhandled rotation %hu\n", rotation);
         break;
   }
}

static void oga_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
    (void)data;
    *iface = &oga_poke_interface;
}

video_driver_t video_oga = {
    oga_gfx_init,
    oga_gfx_frame,
    oga_gfx_set_nonblock_state,
    oga_gfx_alive,
    oga_gfx_focus,
    oga_gfx_suppress_screensaver,
    oga_gfx_has_windowed,
    NULL,
    oga_gfx_free,
    "oga",
    NULL,
    oga_set_rotation,
    oga_gfx_viewport_info,
    NULL,
    NULL,
#ifdef HAVE_OVERLAY
    NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
    NULL,
#endif
    oga_get_poke_interface
};
