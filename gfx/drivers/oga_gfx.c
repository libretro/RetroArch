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
#include <stdint.h>

#include "../../verbosity.h"
#include <fcntl.h>
#include <rga/RgaApi.h>
#include <rga/RockchipRgaMacro.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

#include "frontend/frontend_driver.h"

#include "../font_driver.h"
#include "libretro.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../configuration.h"
#include "../../retroarch.h"

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define ALIGN(val, align) (((val) + (align) - 1) & ~((align) - 1))

#define NUM_PAGES 3

typedef struct oga_rect
{
   int x;
   int y;
   int w;
   int h;
} oga_rect_t;

typedef struct oga_surface
{
   uint8_t* map;
   int width;
   int height;
   int pitch;
   int prime_fd;
   int rk_format;

   int display_fd;
   uint32_t handle;
} oga_surface_t;

typedef struct oga_framebuf
{
   oga_surface_t* surface;
   uint32_t fb_id;
} oga_framebuf_t;

typedef struct oga_video
{
   int fd;
   uint32_t connector_id;
   drmModeModeInfo mode;
   int drm_width;
   int drm_height;
   float display_ar;
   uint32_t crtc_id;

   oga_surface_t* frame_surface;
   oga_surface_t* menu_surface;

   oga_framebuf_t* pages[NUM_PAGES];
   int cur_page;
   int scale_mode;
   int rotation;
   bool threaded;

   oga_surface_t* msg_surface;
   const font_renderer_driver_t *font_driver;
   void *font;
   int msg_width;
   int msg_height;
   char last_msg[128];
} oga_video_t;

static bool oga_create_display(oga_video_t* vid)
{
   int i, ret;
   drmModeConnector *connector;
   drmModeModeInfo *mode;
   drmModeEncoder *encoder;
   drmModeRes *resources;

   vid->fd = open("/dev/dri/card0", O_RDWR);
   if (vid->fd < 0)
   {
      RARCH_ERR("open /dev/dri/card0 failed.\n");
      return false;
   }

   resources = drmModeGetResources(vid->fd);
   if (!resources)
   {
      RARCH_ERR("drmModeGetResources failed: %s\n", strerror(errno));
      goto err_01;
   }

   for (i = 0; i < resources->count_connectors; i++)
   {
      connector = drmModeGetConnector(vid->fd, resources->connectors[i]);
      if (connector->connection == DRM_MODE_CONNECTED)
         break;

      drmModeFreeConnector(connector);
      connector = NULL;
   }

   if (!connector)
   {
      RARCH_ERR("DRM_MODE_CONNECTED not found.\n");
      goto err_02;
   }

   vid->connector_id = connector->connector_id;

   /* Find prefered mode */
   for (i = 0; i < connector->count_modes; i++)
   {
      drmModeModeInfo *current_mode = &connector->modes[i];
      if (current_mode->type & DRM_MODE_TYPE_PREFERRED)
      {
         mode = current_mode;
         break;
      }

      mode = NULL;
   }

   if (!mode)
   {
      RARCH_ERR("DRM_MODE_TYPE_PREFERRED not found.\n");
      goto err_03;
   }

   vid->mode   = *mode;

   /* Find encoder */
   for (i = 0; i < resources->count_encoders; i++)
   {
      encoder = drmModeGetEncoder(vid->fd, resources->encoders[i]);
      if (encoder->encoder_id == connector->encoder_id)
         break;

      drmModeFreeEncoder(encoder);
      encoder = NULL;
   }

   if (!encoder)
   {
      RARCH_ERR("could not find encoder!\n");
      goto err_03;
   }

   vid->crtc_id = encoder->crtc_id;

   drmModeFreeEncoder(encoder);
   drmModeFreeConnector(connector);
   drmModeFreeResources(resources);

   return true;

err_03:
   drmModeFreeConnector(connector);

err_02:
   drmModeFreeResources(resources);

err_01:
   close(vid->fd);

   return false;
}

static oga_surface_t* oga_create_surface(int display_fd,
      int width, int height, int rk_format)
{
   struct drm_mode_create_dumb args = {0};
   oga_surface_t* surface           = (oga_surface_t*)
      calloc(1, sizeof(oga_surface_t));
   if (!surface)
   {
      RARCH_ERR("Error allocating surface\n");
      return NULL;
   }

   args.width  = width;
   args.height = height;
   args.bpp    = rk_format == RK_FORMAT_BGRA_8888 ? 32 : 16;
   args.flags  = 0;

   if (drmIoctl(display_fd, DRM_IOCTL_MODE_CREATE_DUMB, &args) < 0)
   {
      RARCH_ERR("DRM_IOCTL_MODE_CREATE_DUMB failed.\n");
      goto out;
   }

   surface->display_fd = display_fd;
   surface->handle     = args.handle;
   surface->width      = width;
   surface->height     = height;
   surface->pitch      = width * args.bpp / 8;
   surface->rk_format  = rk_format;

   if (drmPrimeHandleToFD(display_fd, surface->handle, DRM_RDWR | DRM_CLOEXEC, &surface->prime_fd) < 0)
   {
      RARCH_ERR("drmPrimeHandleToFD failed.\n");
      goto out;
   }

   surface->map = mmap(NULL, args.size, PROT_READ | PROT_WRITE, MAP_SHARED, surface->prime_fd, 0);
   if (surface->map == MAP_FAILED)
   {
      RARCH_LOG("mmap failed.\n");
      return NULL;
   }

   return surface;

out:
   free(surface);
   return NULL;
}

static void oga_destroy_surface(oga_surface_t* surface)
{
   int io;
   struct drm_mode_destroy_dumb args = { 0 };

   args.handle = surface->handle;

   io          = drmIoctl(surface->display_fd,
         DRM_IOCTL_MODE_DESTROY_DUMB, &args);
   if (io < 0)
      RARCH_ERR("DRM_IOCTL_MODE_DESTROY_DUMB failed.\n");

   free(surface);
}

static oga_framebuf_t* oga_create_framebuf(oga_surface_t* surface)
{
   int ret;
   const uint32_t handles[4] = {surface->handle, 0, 0, 0};
   const uint32_t pitches[4] = {surface->pitch, 0, 0, 0};
   const uint32_t offsets[4] = {0, 0, 0, 0};
   oga_framebuf_t* framebuf  = calloc(1, sizeof(oga_framebuf_t));

   if (!framebuf)
   {
      RARCH_ERR("Error allocating framebuf\n");
      return NULL;
   }

   framebuf->surface = surface;
   ret               = drmModeAddFB2(surface->display_fd,
         surface->width,
         surface->height,
         surface->rk_format == RK_FORMAT_BGRA_8888
         ? DRM_FORMAT_ARGB8888
         : DRM_FORMAT_RGB565,
         handles,
         pitches,
         offsets,
         &framebuf->fb_id,
         0);

   if (ret)
   {
      RARCH_ERR("drmModeAddFB2 failed.\n");
      free(framebuf);
      return NULL;
   }

   return framebuf;
}

static void oga_destroy_framebuf(oga_framebuf_t* framebuf)
{
   if (drmModeRmFB(framebuf->surface->display_fd, framebuf->fb_id) != 0)
      RARCH_ERR("drmModeRmFB failed.\n");

   oga_destroy_surface(framebuf->surface);
   free(framebuf);
}

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
      oga_destroy_framebuf(vid->pages[i]);

   oga_destroy_surface(vid->frame_surface);
   oga_destroy_surface(vid->msg_surface);
   oga_destroy_surface(vid->menu_surface);

   close(vid->fd);

   free(vid);
   vid = NULL;
}

static void *oga_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   int i;
   oga_video_t *vid                     = NULL;
   settings_t *settings                 = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   struct retro_game_geometry  *geom    = &av_info->geometry;
   int aw                               = ALIGN(geom->base_width, 32);
   int ah                               = ALIGN(geom->base_height, 32);

   frontend_driver_install_signal_handler();

   if (input && input_data)
   {
      void* udev = input_driver_init_wrap(
            &input_udev, settings->arrays.input_joypad_driver);
      if (udev)
      {
         *input       = &input_udev;
         *input_data  = udev;
      }
      else
         *input = NULL;
   }

   vid = (oga_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
   {
      RARCH_ERR("Error allocating vid\n");
      return NULL;
   }

   if (!oga_create_display(vid))
   {
      RARCH_ERR("Error initializing drm\n");
      return NULL;
   }

   vid->display_ar = (float)vid->mode.vdisplay / vid->mode.hdisplay;
   vid->drm_width = vid->mode.vdisplay;
   vid->drm_height = vid->mode.hdisplay;

   RARCH_LOG("oga_gfx_init video %dx%d rgb32 %d smooth %d ctx_scaling %d"
         " input_scale %u force_aspect %d fullscreen %d threaded %d base_width %d base_height %d"
         " max_width %d max_height %d aw %d ah %d\n",
         video->width, video->height, video->rgb32, video->smooth, video->ctx_scaling,
         video->input_scale, video->force_aspect, video->fullscreen, video->is_threaded, geom->base_width, geom->base_height,
         geom->max_width, geom->max_height, aw, ah);

   vid->menu_surface = oga_create_surface(vid->fd, vid->drm_width, vid->drm_height, RK_FORMAT_BGRA_8888);
   vid->threaded = video->is_threaded;

   /*
    * From RGA2 documentation:
    *
    *  0  CATROM
    *  1  MITCHELL
    *  2  HERMITE
    *  3  B-SPLINE
    */
   vid->scale_mode = video->ctx_scaling << 1 | video->smooth;
   vid->rotation = 0;

   vid->frame_surface = oga_create_surface(vid->fd, geom->max_width, geom->max_height, video->rgb32 ? RK_FORMAT_BGRA_8888 : RK_FORMAT_RGB_565);
   vid->msg_surface   = oga_create_surface(vid->fd, vid->drm_width, vid->drm_height, RK_FORMAT_BGRA_8888);
   vid->last_msg[0]   = 0;

   /* bitmap only for now */
   if (settings->bools.video_font_enable)
   {
      vid->font_driver = &bitmap_font_renderer;
      vid->font = vid->font_driver->init("", settings->floats.video_font_size);
   }

   for (i = 0; i < NUM_PAGES; ++i)
   {
      oga_surface_t* surface = oga_create_surface(vid->fd, vid->drm_height, vid->drm_width, RK_FORMAT_BGRA_8888);
      vid->pages[i] = oga_create_framebuf(surface);
      if (!vid->pages[i])
         return NULL;
   }

   return vid;
}

static void rga_clear_surface(oga_surface_t* surface, int color)
{
   rga_info_t dst   = { 0 };
   dst.fd           = surface->prime_fd;
   dst.mmuFlag      = 1;
   dst.rect.xoffset = 0;
   dst.rect.yoffset = 0;
   dst.rect.width   = surface->width;
   dst.rect.height  = surface->height;
   dst.rect.wstride = dst.rect.width;
   dst.rect.hstride = dst.rect.height;
   dst.rect.format  = surface->rk_format;
   dst.color        = color;

   c_RkRgaColorFill(&dst);
}

static bool render_msg(oga_video_t* vid, const char* msg)
{
   const struct font_atlas* atlas;
   uint32_t* fb;
   const char *c    = msg;
   int dest_x       = 0;
   int dest_y       = 0;
   int dest_stride;

   if (msg[0] == '\0')
      return false;

   if (strcmp(msg, vid->last_msg) == 0)
      return true;

   strlcpy(vid->last_msg, c, sizeof(vid->last_msg));
   rga_clear_surface(vid->msg_surface, 0);

   atlas          = vid->font_driver->get_atlas(vid->font);
   fb             = (uint32_t*)vid->msg_surface->map;
   dest_stride    = vid->msg_surface->pitch / 4;
   vid->msg_width = vid->msg_height = 0;

   while (*c)
   {
      int x, y;
      uint32_t* dest             = NULL;
      const uint8_t *source      = NULL;
      const struct font_glyph* g = vid->font_driver->get_glyph(vid->font, *c);

      if (!g)
         continue;

      if (vid->msg_height == 0)
         vid->msg_height = g->height;

      if (dest_x >= vid->drm_width)
      {
         dest_x = 0;
         dest_y += g->height;
         vid->msg_height += g->height;
      }

      source = atlas->buffer + g->atlas_offset_y *
         atlas->width  + g->atlas_offset_x;
      dest   = fb + dest_y * dest_stride + dest_x;

      for (y = 0; y < g->height; y++)
      {
         for (x = 0; x < g->advance_x; x++)
         {
            uint32_t px = (x < g->width) ? *(source++) : 0x00;
            *(dest++)   = (0xCD << 24) | (px << 16) | (px << 8) | px;
         }
         dest   += dest_stride - g->advance_x;
         source += atlas->width - g->width;
      }

      c++;
      dest_x += g->advance_x;

      if (vid->msg_width < dest_x)
         vid->msg_width = MIN(dest_x, vid->msg_surface->width);
   }


   return true;
}

static void oga_blit(oga_surface_t* src, int sx, int sy, int sw, int sh,
      oga_surface_t* dst, int dx, int dy, int dw, int dh,
      int rotation, int scale_mode, unsigned int blend)
{
   rga_info_t s = {
      .fd = src->prime_fd,
      .rect = { sx, sy, sw, sh, src->width, src->height, src->rk_format },
      .rotation = rotation,
      .mmuFlag = 1,
      .scale_mode = scale_mode,
      .blend = blend,
   };

   rga_info_t d = {
      .fd = dst->prime_fd,
      .rect = { dx, dy, dw, dh, dst->width, dst->height, dst->rk_format },
      .mmuFlag = 1,
   };

   c_RkRgaBlit(&s, &d, NULL);
}

static void oga_calc_bounds(oga_rect_t* r, int dw, int dh, int sw, int sh, float aspect, float dar)
{
   if (dar >= aspect)
   {
      r->h = dh;
      r->w = MIN(dw, (dh * aspect + 0.5));
      r->x = (int)((dw - r->w) / 2) + 0.5;
      r->y = 0;
   }
   else
   {
      r->w = dw;
      r->h = MIN(dh, (dw / aspect + 0.5));
      r->x = 0;
      r->y = (int)((dh - r->h) / 2) + 0.5;
   }
}

static bool oga_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   oga_video_t *vid  = (oga_video_t*)data;
   oga_framebuf_t* page = vid->pages[vid->cur_page];
   oga_surface_t *page_surface = page->surface;
   float aspect_ratio = video_driver_get_aspect_ratio();

   if (unlikely(!frame || width == 0 || height == 0))
      return true;

   if (unlikely(video_info->input_driver_nonblock_state) && !vid->threaded)
   {
      if (frame_count % 4 != 0)
         return true;
   }

   if (msg && vid->font)
   {
        if (!render_msg(vid, msg))
            msg = NULL;
   }

   rga_clear_surface(page_surface, 0);

   if (likely(!video_info->menu_is_alive))
   {
      uint8_t* src = (uint8_t*)frame;
      uint8_t* dst = (uint8_t*)vid->frame_surface->map;
      unsigned int blend = video_info->runloop_is_paused ? 0x800105 : 0;
      oga_rect_t r;

      if (src != dst)
      {
         int dst_pitch = vid->frame_surface->pitch;
         int yy = height;

         while (yy > 0) {
             memcpy(dst, src, pitch);
             src += pitch;
             dst += dst_pitch;
             --yy;
         }
      }

      oga_calc_bounds(&r, vid->drm_width, vid->drm_height, width, height, aspect_ratio, vid->display_ar);
      oga_blit(vid->frame_surface, 0, 0, width, height,
            page_surface, r.y, r.x, r.h, r.w, vid->rotation, vid->scale_mode, blend);
   }
#ifdef HAVE_MENU
   else
   {
      menu_driver_frame(true, video_info);

      width = vid->menu_surface->width;
      height = vid->menu_surface->height;

      aspect_ratio = (float)width / height;

      oga_rect_t r;
      oga_calc_bounds(&r, vid->drm_width, vid->drm_height, width, height, aspect_ratio, vid->display_ar);
      oga_blit(vid->menu_surface, 0, 0, width, height,
            page_surface, r.y, r.x, r.h, r.w, HAL_TRANSFORM_ROT_270, vid->scale_mode, 0);
   }
#endif

   if (msg)
   {
      oga_blit(vid->msg_surface, 0, 0, vid->msg_width, vid->msg_height,
            page_surface, 0, 0, vid->msg_height, vid->msg_width,
            HAL_TRANSFORM_ROT_270, vid->scale_mode, 0xff0105);
   }

   if (unlikely(drmModeSetCrtc(vid->fd, vid->crtc_id, page->fb_id, 0, 0, &vid->connector_id, 1, &vid->mode) != 0))
      RARCH_ERR("drmModeSetCrtc failed.\n");

   vid->cur_page = (vid->cur_page + 1) % NUM_PAGES;

   return true;
}

static void oga_gfx_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   oga_video_t *vid             = (oga_video_t*)data;
   unsigned i, j;
   /* Borrowed from drm_gfx
    *
    * We have to go on a pixel format conversion adventure
    * for now, until we can convince RGUI to output
    * in an 8888 format. */
   unsigned int src_pitch        = width * 2;
   unsigned int dst_pitch        = width * 4;
   unsigned int dst_width        = width;
   uint32_t line[dst_width];
   char *frame_output;

   if (vid->menu_surface->width != width || vid->menu_surface->height != height)
   {
      oga_destroy_surface(vid->menu_surface);
      vid->menu_surface = oga_create_surface(vid->fd, width, height,
            RK_FORMAT_BGRA_8888);
   }

   /* The output pixel array with the converted pixels. */
   frame_output = (char*)vid->menu_surface->map;

   for (i = 0; i < height; i++)
   {
      for (j = 0; j < src_pitch / 2; j++)
      {
         uint16_t src_pix = *((uint16_t*)frame + (src_pitch / 2 * i) + j);
         /* The hex AND is for keeping only the part
          * we need for each component. */
         uint32_t R       = (src_pix << 8) & 0x00FF0000;
         uint32_t G       = (src_pix << 4) & 0x0000FF00;
         uint32_t B       = (src_pix << 0) & 0x000000FF;
         line[j]          = (0x00 | R | G | B);
      }
      memcpy(frame_output + (dst_pitch * i), (char*)line, dst_pitch);
   }
}

static void oga_gfx_texture_enable(void *data, bool state, bool full_screen)
{
   (void)data;
   (void)state;
   (void)full_screen;
}

static void oga_gfx_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }

static bool oga_gfx_alive(void *data)
{
   return !frontend_driver_get_signal_handler_state();
}

static bool oga_gfx_focus(void *data) { return true; }
static bool oga_gfx_suppress_screensaver(void *data, bool enable) { return false; }
static bool oga_gfx_has_windowed(void *data) { return false; }

static void oga_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   oga_video_t *vid = (oga_video_t*)data;
   if (unlikely(!vid))
      return;

   vp->x = vp->y = 0;
   vp->width = vp->full_width = vid->mode.vdisplay;
   vp->height = vp->full_height = vid->mode.hdisplay;
}

static bool oga_gfx_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void oga_set_rotation(void *data, unsigned rotation)
{
   oga_video_t *vid = (oga_video_t*)data;
   if (!vid)
      return;

   switch (rotation)
   {
   case 0:
      vid->rotation = HAL_TRANSFORM_ROT_270;
      break;
   case 1:
      vid->rotation = HAL_TRANSFORM_ROT_180;
      break;
   case 2:
      vid->rotation = HAL_TRANSFORM_ROT_90;
      break;
   case 3:
      vid->rotation = 0;
      break;
   default:
      RARCH_ERR("Unhandled rotation %hu\n", rotation);
      break;
   }
}

static bool oga_get_current_software_framebuffer(void *data, struct retro_framebuffer *framebuffer)
{
   oga_video_t *vid = (oga_video_t*)data;
   if (!vid)
      return false;

   framebuffer->format = vid->frame_surface->rk_format == RK_FORMAT_BGRA_8888 ?
      RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   framebuffer->data = (uint8_t*)vid->frame_surface->map;
   framebuffer->pitch = vid->frame_surface->pitch;

   return true;
}

video_poke_interface_t oga_poke_interface = {
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
   oga_gfx_set_texture_frame,
   oga_gfx_texture_enable,
   NULL,
   NULL,
   NULL,
   NULL,
   oga_get_current_software_framebuffer,
   NULL
};

static void oga_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
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
   oga_gfx_set_shader,
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
