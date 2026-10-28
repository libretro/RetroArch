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
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/mman.h>
#include <rga/RgaApi.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

#include <libretro.h>

#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "frontend/frontend_driver.h"

#include <encodings/utf.h>

#include "../font_driver.h"
#include "../video_driver.h"
#include "../video_views.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define ALIGN(val, align) (((val) + (align) - 1) & ~((align) - 1))

/* Scanout pages: one on screen, one queued to flip, one being drawn. */
#define NUM_PAGES 3
/* Bound on one wait for a flip, so a lost event cannot hang. */
#define OGA_FLIP_TIMEOUT_MS 100

/* What a page was last drawn with, for deciding whether its borders
 * need clearing. */
enum oga_page_mode
{
   OGA_PAGE_GAME = 0,
   OGA_PAGE_MENU,
   OGA_PAGE_VIEWS
};

typedef struct oga_rect
{
   int x;
   int y;
   int w;
   int h;
} oga_rect_t;

typedef struct oga_surface
{
   uint8_t* map;        /* NULL when only the RGA touches it */
   size_t size;
   int width;
   int height;
   int pitch;           /* bytes, as the kernel allocated it */
   int wstride;         /* the same in pixels, for the RGA */
   int prime_fd;
   int rk_format;
   int display_fd;
   uint32_t handle;
} oga_surface_t;

typedef struct oga_framebuf
{
   oga_surface_t* surface;
   uint32_t fb_id;
   unsigned clear_gen;  /* rect_gen its borders were last cleared for */
   bool dirty;          /* drawn over outside the picture (message) */
} oga_framebuf_t;

typedef struct oga_video
{
   drmModeModeInfo mode;
   drmModeCrtc *orig_crtc;
   oga_surface_t *frame_surface;
   oga_surface_t *menu_surface;
   oga_framebuf_t *pages[NUM_PAGES];
   oga_surface_t *msg_surface;
   /* A core's views laid out on the screen, upright, for the blit that
    * turns them onto the panel */
   oga_surface_t *views_surface;
   /* Where the last views frame put its views on views_surface; the
    * surface is cleared only when that changes. */
   video_views_rect_t views_cuts[VIDEO_VIEWS_MAX_PLACEMENTS];
   unsigned views_cut_count;
   const font_renderer_driver_t *font_driver;
   void *font;
   int msg_width;
   int msg_height;
   int fd;
   int drm_width;
   int drm_height;
   int cur_page;
   int scale_mode;
   int rotation;
   uint32_t crtc_id;
   uint32_t connector_id;
   /* Packed size of the picture in menu_surface */
   uint32_t menu_dims;
   /* Where the last frame went on the page, packed (x,w) and (y,h),
    * and with what; any change bumps rect_gen so each page clears its
    * borders once. */
   uint32_t last_xw;
   uint32_t last_yh;
   unsigned last_mode;
   unsigned rect_gen;
   float display_ar;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t views_fallback;
   bool threaded;
   bool views_seen;
   bool flip_pending;
   bool crtc_set;
   bool no_flip;        /* the driver refused page flips */
   char last_msg[128];
} oga_video_t;

static bool oga_create_display(oga_video_t* vid)
{
   int i;
   drmModeConnector *connector;
   drmModeModeInfo *mode;
   drmModeEncoder *encoder;
   drmModeRes *resources;

   vid->fd = open("/dev/dri/card0", O_RDWR);
   if (vid->fd < 0)
   {
      RARCH_ERR("Open /dev/dri/card0 failed.\n");
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

   /* Find preferred mode */
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
      RARCH_ERR("Could not find encoder.\n");
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

static void oga_destroy_surface(oga_surface_t* surface)
{
   struct drm_mode_destroy_dumb args;

   if (!surface)
      return;

   if (surface->map)
      munmap(surface->map, surface->size);
   if (surface->prime_fd >= 0)
      close(surface->prime_fd);
   if (surface->handle)
   {
      args.handle = surface->handle;
      if (drmIoctl(surface->display_fd,
               DRM_IOCTL_MODE_DESTROY_DUMB, &args) < 0)
         RARCH_ERR("DRM_IOCTL_MODE_DESTROY_DUMB failed.\n");
   }

   free(surface);
}

/* A dumb buffer the RGA reads or writes through its prime fd; mapped
 * for the CPU only when @map is set. */
static oga_surface_t* oga_create_surface(int display_fd,
      int width, int height, int rk_format, bool map)
{
   struct drm_mode_create_dumb args;
   int bytes                        = (rk_format == RK_FORMAT_BGRA_8888)
      ? 4 : 2;
   oga_surface_t* surface           = (oga_surface_t*)
      calloc(1, sizeof(oga_surface_t));
   if (!surface)
   {
      RARCH_ERR("Error allocating surface.\n");
      return NULL;
   }
   surface->prime_fd   = -1;
   surface->display_fd = display_fd;

   memset(&args, 0, sizeof(args));
   args.width  = width;
   args.height = height;
   args.bpp    = bytes * 8;

   if (drmIoctl(display_fd, DRM_IOCTL_MODE_CREATE_DUMB, &args) < 0)
   {
      RARCH_ERR("DRM_IOCTL_MODE_CREATE_DUMB failed.\n");
      goto error;
   }

   surface->handle     = args.handle;
   surface->size       = (size_t)args.size;
   surface->width      = width;
   surface->height     = height;
   /* The kernel pads rows (to 64 bytes on Rockchip): its pitch, not
    * width * bpp, is where each row starts. */
   surface->pitch      = (int)args.pitch;
   surface->wstride    = (int)args.pitch / bytes;
   surface->rk_format  = rk_format;

   if (drmPrimeHandleToFD(display_fd, surface->handle,
            DRM_RDWR | DRM_CLOEXEC, &surface->prime_fd) < 0)
   {
      RARCH_ERR("drmPrimeHandleToFD failed.\n");
      surface->prime_fd = -1;
      goto error;
   }

   if (map)
   {
      void *p = mmap(NULL, surface->size, PROT_READ | PROT_WRITE,
            MAP_SHARED, surface->prime_fd, 0);
      if (p == MAP_FAILED)
      {
         RARCH_ERR("mmap failed.\n");
         goto error;
      }
      surface->map = (uint8_t*)p;
   }

   return surface;

error:
   oga_destroy_surface(surface);
   return NULL;
}

static oga_framebuf_t* oga_create_framebuf(oga_surface_t* surface)
{
   int ret;
   uint32_t handles[4], pitches[4], offsets[4];
   oga_framebuf_t* framebuf;

   if (!surface)
      return NULL;
   if (!(framebuf = (oga_framebuf_t*)calloc(1, sizeof(oga_framebuf_t))))
   {
      RARCH_ERR("Error allocating framebuf.\n");
      oga_destroy_surface(surface);
      return NULL;
   }

   handles[0] = surface->handle;
   pitches[0] = (uint32_t)surface->pitch;
   handles[1] = handles[2] = handles[3] = 0;
   pitches[1] = pitches[2] = pitches[3] = 0;
   offsets[0] = offsets[1] = offsets[2] = offsets[3] = 0;

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
      oga_destroy_surface(surface);
      free(framebuf);
      return NULL;
   }

   return framebuf;
}

static void oga_destroy_framebuf(oga_framebuf_t* framebuf)
{
   if (!framebuf)
      return;
   if (drmModeRmFB(framebuf->surface->display_fd, framebuf->fb_id) != 0)
      RARCH_ERR("drmModeRmFB failed.\n");

   oga_destroy_surface(framebuf->surface);
   free(framebuf);
}

static void oga_page_flip_handler(int fd, unsigned frame,
      unsigned sec, unsigned usec, void *data)
{
   ((oga_video_t*)data)->flip_pending = false;
}

static drmEventContext oga_evctx = {
   2,                      /* DRM_EVENT_CONTEXT_VERSION for this handler */
   NULL,                   /* vblank_handler */
   oga_page_flip_handler
};

/* Waits up to timeout_ms (0 polls) for the queued flip to land.
 * Returns true once nothing is pending. */
static bool oga_wait_flip(oga_video_t *vid, int timeout_ms)
{
   while (vid->flip_pending)
   {
      int ret;
      struct pollfd p;
      p.fd      = vid->fd;
      p.events  = POLLIN;
      p.revents = 0;
      ret       = poll(&p, 1, timeout_ms);
      if (ret < 0 && errno == EINTR)
         continue;
      if (ret <= 0)
      {
         /* A timed-out wait gives the flip up for lost; a poll leaves
          * it pending. */
         if (timeout_ms > 0)
            vid->flip_pending = false;
         break;
      }
      drmHandleEvent(vid->fd, &oga_evctx);
   }
   return !vid->flip_pending;
}

static void oga_set_crtc(oga_video_t *vid, uint32_t fb_id)
{
   if (drmModeSetCrtc(vid->fd, vid->crtc_id, fb_id, 0, 0,
            &vid->connector_id, 1, &vid->mode))
      RARCH_ERR("drmModeSetCrtc failed.\n");
   vid->crtc_set = true;
}

/* The first frame sets the mode; every frame after is a page flip,
 * which the kernel carries out at the next vblank on its own. */
static void oga_present(oga_video_t *vid, uint32_t fb_id)
{
   if (!vid->crtc_set || vid->no_flip)
   {
      oga_set_crtc(vid, fb_id);
      return;
   }

   if (!drmModePageFlip(vid->fd, vid->crtc_id, fb_id,
            DRM_MODE_PAGE_FLIP_EVENT, vid))
      vid->flip_pending = true;
   else if (errno != EBUSY)
   {
      RARCH_WARN("[OGA] Page flip refused, presenting with drmModeSetCrtc.\n");
      vid->no_flip = true;
      oga_set_crtc(vid, fb_id);
   }
}

static void oga_free(void *data)
{
   unsigned i;
   oga_video_t *vid = (oga_video_t*)data;

   if (!vid)
      return;

   oga_wait_flip(vid, OGA_FLIP_TIMEOUT_MS);
   if (vid->orig_crtc)
   {
      drmModeSetCrtc(vid->fd, vid->orig_crtc->crtc_id,
            vid->orig_crtc->buffer_id,
            vid->orig_crtc->x, vid->orig_crtc->y,
            &vid->connector_id, 1, &vid->orig_crtc->mode);
      drmModeFreeCrtc(vid->orig_crtc);
      vid->orig_crtc = NULL;
   }

   if (vid->font_driver && vid->font)
      vid->font_driver->free(vid->font);
   vid->font_driver = NULL;
   vid->font        = NULL;

   for (i = 0; i < NUM_PAGES; ++i)
      oga_destroy_framebuf(vid->pages[i]);

   /* frame_surface->map is handed to the core through
    * oga_get_current_software_framebuffer, so the cached frame can
    * point straight into it. Retire before tearing it down. */
   video_driver_cached_frame_retire();

   oga_destroy_surface(vid->frame_surface);
   oga_destroy_surface(vid->msg_surface);
   oga_destroy_surface(vid->menu_surface);
   oga_destroy_surface(vid->views_surface);

   close(vid->fd);

   free(vid);
   vid = NULL;
}

static void *oga_init(const video_info_t *video)
{
   int i;
   oga_video_t *vid                     = NULL;
   settings_t *settings                 = config_get_ptr();
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   struct retro_game_geometry  *geom    = &av_info->geometry;

   frontend_driver_install_signal_handler();

   /* no input driver of this driver's own: there is no window system
    * here, and the frontend starts the one that goes with that */
   input_driver_video_window(INPUT_WINDOW_KMS, NULL);

   vid = (oga_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
   {
      RARCH_ERR("Error allocating vid.\n");
      return NULL;
   }

   if (!oga_create_display(vid))
   {
      RARCH_ERR("Error initializing drm.\n");
      free(vid);
      return NULL;
   }

   vid->display_ar = (float)vid->mode.vdisplay / vid->mode.hdisplay;
   vid->drm_width = vid->mode.vdisplay;
   vid->drm_height = vid->mode.hdisplay;
   vid->orig_crtc  = drmModeGetCrtc(vid->fd, vid->crtc_id);
   vid->rect_gen   = 1;

   /* RGUI's frame is 16-bit, which the RGA reads as it is. Made here
    * at the screen's size so opening the menu allocates nothing. */
   vid->menu_surface = oga_create_surface(vid->fd, vid->drm_width,
         vid->drm_height, RK_FORMAT_RGB_565, true);
   vid->menu_dims    = VIDEO_SCALE_PACK(vid->drm_width, vid->drm_height);
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

   vid->frame_surface = oga_create_surface(vid->fd, geom->max_width,
         geom->max_height, video->rgb32 ? RK_FORMAT_BGRA_8888
         : RK_FORMAT_RGB_565, true);
   vid->msg_surface   = oga_create_surface(vid->fd, vid->drm_width,
         vid->drm_height, RK_FORMAT_BGRA_8888, true);
   if (!vid->menu_surface || !vid->frame_surface || !vid->msg_surface)
   {
      oga_free(vid);
      return NULL;
   }
   vid->last_msg[0]   = 0;

   if (settings->bools.video_font_enable)
   {
      /* Through font_renderer_create_default(), as every other driver
       * does: it resolves the path, reads the file and picks a
       * backend.  Reaching for &stb_font_renderer and calling its
       * init() by hand meant this driver had to track that function's
       * signature, and it stopped doing so - the call passed three
       * arguments to a five-argument prototype and had not compiled
       * for some time.  A NULL path still ends at stb's built-in
       * glyphs when no font file is configured or found, which is the
       * behaviour that was wanted here. */
      if (!font_renderer_create_default(&vid->font_driver, &vid->font,
               *settings->paths.path_font ? settings->paths.path_font : NULL,
               (unsigned)settings->floats.video_font_size,
               FONT_ATLAS_FORMAT_A8))
      {
         vid->font_driver = NULL;
         vid->font        = NULL;
      }
      else
      {
         /* The atlas may grow when a message needs more glyphs than it
          * holds; the glyphs are blitted from it in memory, so there is
          * no texture to make again */
         struct font_atlas *grow = vid->font_driver->get_atlas(vid->font);
         grow->max_dims = VIDEO_SCALE_PACK(2048, 2048);
      }
   }

   /* The pages take the core's depth: a 16-bit core's frames go to
    * the panel at 16 bits, which halves what the RGA writes and what
    * the display controller reads each frame. Menu, messages and views
    * are converted by the RGA on the way. */
   for (i = 0; i < NUM_PAGES; ++i)
   {
      vid->pages[i] = oga_create_framebuf(oga_create_surface(vid->fd,
               vid->drm_height, vid->drm_width,
               video->rgb32 ? RK_FORMAT_BGRA_8888 : RK_FORMAT_RGB_565,
               false));
      if (!vid->pages[i])
      {
         oga_free(vid);
         return NULL;
      }
   }

   /* For a core's views: made now, at the screen's size, so the first
    * views frame allocates nothing. Without it views are drawn whole. */
   vid->views_surface = oga_create_surface(vid->fd, vid->drm_width,
         vid->drm_height, RK_FORMAT_BGRA_8888, false);

   /* What views are laid out on: the screen as it is seen */
   video_driver_set_output_dims(VIDEO_SCALE_PACK(vid->drm_width,
            vid->drm_height));

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
   dst.rect.wstride = surface->wstride;
   dst.rect.hstride = surface->height;
   dst.rect.format  = surface->rk_format;
   dst.color        = color;

   c_RkRgaColorFill(&dst);
}

static bool render_msg(oga_video_t* vid, const char* msg)
{
   const struct font_atlas* atlas;
   uint32_t* fb;
   int dest_x       = 0;
   int dest_y       = 0;
   int dest_stride;

   if (msg[0] == '\0')
      return false;

   if (strcmp(msg, vid->last_msg) == 0)
      return true;

   strlcpy(vid->last_msg, msg, sizeof(vid->last_msg));
   rga_clear_surface(vid->msg_surface, 0);

   atlas          = vid->font_driver->get_atlas(vid->font);
   fb             = (uint32_t*)vid->msg_surface->map;
   dest_stride    = vid->msg_surface->pitch / 4;
   vid->msg_width = vid->msg_height = 0;

   {
      const struct font_glyph* (*get_glyph)(void*, uint32_t)
                                             = vid->font_driver->get_glyph;
      void *font_data                        = vid->font;
      const struct font_glyph *glyph_q       = get_glyph(font_data, '?');
      struct font_line_metrics *line_metrics = NULL;
      size_t msg_len                         = strlen(msg);
      int line_h                             = 0;
      int surf_w                             = vid->msg_surface->width;
      int surf_h                             = vid->msg_surface->height;
      bool full                              = false;

      vid->font_driver->get_line_metrics(font_data, &line_metrics);
      if (line_metrics)
         line_h = (int)line_metrics->height;

      /* UTF-8; a line break starts a new row, as running out of width
       * does. A glyph is laid in the surface whole or not at all: it
       * wraps before it would cross the right edge, and nothing is
       * drawn once a row would cross the bottom. */
#define FONT_LAYOUT_ALIGNED 0
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
      do \
      { \
         (void)(line_width); \
         (void)(count); \
         (void)(bytes); \
         if (line) \
         { \
            dest_x           = 0; \
            dest_y          += line_h; \
            vid->msg_height += line_h; \
         } \
      } while (0)
#define FONT_LAYOUT_GLYPH(g, pen_x, pen_y) \
      do \
      { \
         int x, y, drawn; \
         uint32_t* dest        = NULL; \
         const uint8_t *source = NULL; \
         (void)(pen_x); \
         (void)(pen_y); \
         if (full) \
            break; \
         if (vid->msg_height == 0) \
            vid->msg_height = VIDEO_SCALE_H((g)->dims); \
         if (dest_x + (g)->advance_x > surf_w) \
         { \
            dest_x           = 0; \
            dest_y          += VIDEO_SCALE_H((g)->dims); \
            vid->msg_height += VIDEO_SCALE_H((g)->dims); \
         } \
         if (     (g)->advance_x > surf_w \
               || dest_y + (int)VIDEO_SCALE_H((g)->dims) > surf_h) \
         { \
            full = true; \
            break; \
         } \
         source = atlas->buffer + VIDEO_SCALE_H((g)->atlas_pos) * \
            atlas->width  + VIDEO_SCALE_W((g)->atlas_pos); \
         dest   = fb + dest_y * dest_stride + dest_x; \
         /* The glyph's texels up to its advance, then clear ones */ \
         drawn  = MIN((int)VIDEO_SCALE_W((g)->dims), (g)->advance_x); \
         for (y = 0; y < (int)VIDEO_SCALE_H((g)->dims); y++) \
         { \
            for (x = 0; x < drawn; x++) \
            { \
               uint32_t px = source[x]; \
               dest[x]     = (0xCDu << 24) | (px * 0x010101u); \
            } \
            for (; x < (g)->advance_x; x++) \
               dest[x]     = 0xCDu << 24; \
            dest   += dest_stride; \
            source += atlas->width; \
         } \
         dest_x += (g)->advance_x; \
         if (vid->msg_width < dest_x) \
            vid->msg_width = MIN(dest_x, surf_w); \
      } while (0)
#include "../font_layout.h"

      /* A row that did not fit counted towards the height before it was
       * found not to; the blit takes no more than the surface holds */
      if (vid->msg_height > surf_h)
         vid->msg_height = surf_h;
   }


   return true;
}

static void oga_blit(oga_surface_t* src, int sx, int sy, int sw, int sh,
      oga_surface_t* dst, int dx, int dy, int dw, int dh,
      int rotation, int scale_mode, unsigned int blend)
{
   rga_info_t s, d;

   memset(&s, 0, sizeof(s));
   s.fd           = src->prime_fd;
   s.mmuFlag      = 1;
   s.rotation     = rotation;
   s.scale_mode   = scale_mode;
   s.blend        = blend;
   s.rect.xoffset = sx;
   s.rect.yoffset = sy;
   s.rect.width   = sw;
   s.rect.height  = sh;
   s.rect.wstride = src->wstride;
   s.rect.hstride = src->height;
   s.rect.format  = src->rk_format;

   memset(&d, 0, sizeof(d));
   d.fd           = dst->prime_fd;
   d.mmuFlag      = 1;
   d.rect.xoffset = dx;
   d.rect.yoffset = dy;
   d.rect.width   = dw;
   d.rect.height  = dh;
   d.rect.wstride = dst->wstride;
   d.rect.hstride = dst->height;
   d.rect.format  = dst->rk_format;

   c_RkRgaBlit(&s, &d, NULL);
}

/* Where a pushed frame lies in the lent surface.
 *
 * A core that renders into the buffer
 * GET_CURRENT_SOFTWARE_FRAMEBUFFER lends it may push back a pointer
 * partway into that buffer, at the buffer's pitch, with the size of
 * the window it wants shown: that is how an overscan crop is done by
 * offset (beetle-psx renders its whole 700x480 scanout surface into
 * the loan and pushes the visible part). The threaded wrapper accepts
 * that as a lend since 9ed0db58c3 and the vulkan and d3d12 drivers
 * since 7510039d5d; this is the same test for this driver's loan.
 *
 * True, with the window's pixel origin, when @frame lies inside
 * @surface's mapping on that mapping's pitch and starts on a pixel.
 * Anything else is a frame of the core's own and is copied in. */
static bool oga_frame_window(const oga_surface_t *surface,
      const void *frame, size_t pitch, int *x, int *y)
{
   uintptr_t base = (uintptr_t)surface->map;
   uintptr_t p    = (uintptr_t)frame;
   size_t    bpp  = (surface->rk_format == RK_FORMAT_BGRA_8888) ? 4 : 2;
   size_t    off;
   size_t    row;

   if (!surface->map || surface->pitch <= 0 || surface->height <= 0)
      return false;
   if (p < base || (p - base) >= (uintptr_t)surface->pitch *
         (uintptr_t)surface->height)
      return false;

   off = (size_t)(p - base);
   row = off % (size_t)surface->pitch;

   /* Inside the mapping but not addressable as a window: copying it
    * would memcpy the surface onto itself and shred it, so show the
    * loan from its origin instead. */
   if (pitch != (size_t)surface->pitch || (row % bpp))
   {
      *x = 0;
      *y = 0;
      return true;
   }

   *y = (int)(off / (size_t)surface->pitch);
   *x = (int)(row / bpp);
   return true;
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

/* Makes @page ready for a picture at @r drawn as @mode. The blit
 * writes every pixel inside @r, so only the borders around it can
 * hold anything stale, and only once the picture moved, a message was
 * drawn over the page, or a blend has to land on black. Each page is
 * cleared once per change instead of every frame. */
static void oga_prepare_page(oga_video_t *vid, oga_framebuf_t *page,
      const oga_rect_t *r, unsigned mode, unsigned int blend)
{
   uint32_t xw  = ((uint32_t)r->x << 16) | ((uint32_t)r->w & 0xFFFF);
   uint32_t yh  = ((uint32_t)r->y << 16) | ((uint32_t)r->h & 0xFFFF);
   unsigned key = mode | ((unsigned)vid->rotation << 4);
   bool full    =  r->x == 0 && r->y == 0
               && r->w >= vid->drm_width && r->h >= vid->drm_height;

   if (     xw  != vid->last_xw
         || yh  != vid->last_yh
         || key != vid->last_mode)
   {
      vid->last_xw   = xw;
      vid->last_yh   = yh;
      vid->last_mode = key;
      vid->rect_gen++;
   }

   if (     blend
         || page->dirty
         || (page->clear_gen != vid->rect_gen && !full))
      rga_clear_surface(page->surface, 0);
   page->clear_gen = vid->rect_gen;
   page->dirty     = false;
}

/* A core's views: each placement scaled by the RGA straight from the
 * frame's surface into an upright surface the size of the screen, cut
 * to its own eye's area and the screen, which one more blit turns onto
 * the panel. Only while the panel's own turn is the one applied:
 * anaglyph and interlaced, which blend the eyes, and a turned core draw
 * the frame whole. False when the frame is to be drawn whole. */
static bool oga_views_frame(oga_video_t *vid, const void *frame,
      unsigned dims, unsigned width, unsigned height, int sx, int sy,
      const video_frame_info_t *info, oga_framebuf_t *page,
      unsigned int blend)
{
   unsigned i, n;
   unsigned screen;
   oga_rect_t whole;
   video_views_rect_t cuts[VIDEO_VIEWS_MAX_PLACEMENTS];
   unsigned char which[VIDEO_VIEWS_MAX_PLACEMENTS];
   const video_views_layout_t *layout;
   bool views = info && frame && info->views.num_views
      && !info->views_layout.offscreen
      && vid->rotation == HAL_TRANSFORM_ROT_270
      /* All of the frame in its surface */
      && width == VIDEO_SCALE_W(dims) && height == VIDEO_SCALE_H(dims)
      && video_views_fit_frame(&info->views, dims);

   /* Made at init; without it the frame is drawn whole */
   if (!vid->views_surface)
      views = false;
   retro_atomic_store_release_int(&vid->views_fallback,
         (info && info->views.num_views && !views) ? 1 : 0);
   if (!views)
      return false;
   if (!vid->views_seen)
   {
      vid->views_seen = true;
      RARCH_LOG("[OGA] Views drawn straight from the frame.\n");
   }

   layout = &info->views_layout;
   screen = VIDEO_SCALE_PACK(vid->drm_width, vid->drm_height);
   /* Where each visible view lands this frame */
   for (i = 0, n = 0; i < layout->num_placements
         && i < VIDEO_VIEWS_MAX_PLACEMENTS; i++)
   {
      if (!video_views_visible(layout, &layout->placements[i], screen,
               &cuts[n]))
         continue;
      which[n++] = (unsigned char)i;
   }

   /* The views overwrite the same rectangles every frame, so what lies
    * outside them only needs black again when they move. */
   if (     n != vid->views_cut_count
         || memcmp(cuts, vid->views_cuts, n * sizeof(cuts[0])))
   {
      rga_clear_surface(vid->views_surface, 0);
      memcpy(vid->views_cuts, cuts, n * sizeof(cuts[0]));
      vid->views_cut_count = n;
   }

   for (i = 0; i < n; i++)
   {
      video_views_rect_t from;
      const video_views_rect_t *cut    = &cuts[i];
      const video_views_placement_t *p = &layout->placements[which[i]];

      video_views_cut_source(&info->views.views[p->view], &p->dst, cut,
            &from);
      oga_blit(vid->frame_surface,
            sx + VIDEO_POS_X(from.pos), sy + VIDEO_POS_Y(from.pos),
            (int)VIDEO_SCALE_W(from.dims), (int)VIDEO_SCALE_H(from.dims),
            vid->views_surface, VIDEO_POS_X(cut->pos), VIDEO_POS_Y(cut->pos),
            (int)VIDEO_SCALE_W(cut->dims), (int)VIDEO_SCALE_H(cut->dims),
            0, vid->scale_mode, 0);
   }
   whole.x = whole.y = 0;
   whole.w = vid->drm_width;
   whole.h = vid->drm_height;
   oga_prepare_page(vid, page, &whole, OGA_PAGE_VIEWS, blend);
   oga_blit(vid->views_surface, 0, 0, vid->drm_width, vid->drm_height,
         page->surface, 0, 0, vid->drm_height, vid->drm_width,
         vid->rotation, vid->scale_mode, blend);
   return true;
}

static bool oga_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   oga_rect_t r;
   unsigned width              = VIDEO_SCALE_W(dims);
   unsigned height             = VIDEO_SCALE_H(dims);
   oga_video_t *vid            = (oga_video_t*)data;
   oga_framebuf_t* page;
   oga_surface_t *page_surface;
   float aspect_ratio;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (unlikely(!frame || width == 0 || height == 0))
      return true;

   /* Fast-forward or vsync off: with the last flip still queued, this
    * frame is not shown rather than waited for - and since nothing is
    * drawn for it, it costs no RGA work either. With the flip landed
    * the frame goes out, so the panel shows one frame per refresh and
    * the core runs as fast as it can. */
   if (     (video_info->input_driver_nonblock_state || !video_info->vsync)
         && vid->flip_pending
         && !oga_wait_flip(vid, 0))
      return true;

   page         = vid->pages[vid->cur_page];
   page_surface = page->surface;
   aspect_ratio = video_driver_get_aspect_ratio();

   if (msg && vid->font)
   {
        if (!render_msg(vid, msg))
            msg = NULL;
   }

#ifdef HAVE_MENU
   if (menu_is_alive)
   {
      menu_driver_frame(true, video_info);
      if (!vid->menu_surface || !vid->menu_dims)
         return true;

      width        = VIDEO_SCALE_W(vid->menu_dims);
      height       = VIDEO_SCALE_H(vid->menu_dims);
      aspect_ratio = (float)width / height;

      oga_calc_bounds(&r, vid->drm_width, vid->drm_height, width, height, aspect_ratio, vid->display_ar);
      oga_prepare_page(vid, page, &r, OGA_PAGE_MENU, 0);
      oga_blit(vid->menu_surface, 0, 0, width, height,
            page_surface, r.y, r.x, r.h, r.w, HAL_TRANSFORM_ROT_270, vid->scale_mode, 0);
   }
   else
#endif
   {
      uint8_t* src = (uint8_t*)frame;
      uint8_t* dst = (uint8_t*)vid->frame_surface->map;
      unsigned int blend = video_info->runloop_is_paused ? 0x800105 : 0;
      int sx       = 0;
      int sy       = 0;
      bool lent    = oga_frame_window(vid->frame_surface, frame,
            (size_t)pitch, &sx, &sy);

      /* The surface holds the geometry declared at init. A core is
       * free to hand over more than it declared, so take what fits:
       * the rows the surface has from the window's origin, and the
       * bytes one of its rows holds. Both the copy below and the blit
       * that follows read and write this allocation. */
      if (width  > (unsigned)(vid->frame_surface->width  - sx))
         width  = (unsigned)(vid->frame_surface->width  - sx);
      if (height > (unsigned)(vid->frame_surface->height - sy))
         height = (unsigned)(vid->frame_surface->height - sy);

      if (!lent)
      {
         int    dst_pitch = vid->frame_surface->pitch;
         size_t row       = (pitch < (unsigned)dst_pitch)
            ? (size_t)pitch : (size_t)dst_pitch;
         int    yy        = (int)height;

         /* Rows that are packed on both sides are one copy */
         if (pitch == (unsigned)dst_pitch)
            memcpy(dst, src, (size_t)dst_pitch * height);
         else
         {
            while (yy > 0)
            {
                memcpy(dst, src, row);
                src += pitch;
                dst += dst_pitch;
                --yy;
            }
         }
      }

      if (!oga_views_frame(vid, frame, dims, width, height, sx, sy,
               video_info, page, blend))
      {
         oga_calc_bounds(&r, vid->drm_width, vid->drm_height, width, height, aspect_ratio, vid->display_ar);
         oga_prepare_page(vid, page, &r, OGA_PAGE_GAME, blend);
         oga_blit(vid->frame_surface, sx, sy, width, height,
               page_surface, r.y, r.x, r.h, r.w, vid->rotation, vid->scale_mode, blend);
      }
   }

   if (msg)
   {
      oga_blit(vid->msg_surface, 0, 0, vid->msg_width, vid->msg_height,
            page_surface, 0, 0, vid->msg_height, vid->msg_width,
            HAL_TRANSFORM_ROT_270, vid->scale_mode, 0xff0105);
      /* It may lie over the borders, which the next frame on this page
       * does not redraw */
      page->dirty = true;
   }

   /* The RGA work above ran while the last flip was landing. */
   oga_wait_flip(vid, OGA_FLIP_TIMEOUT_MS);
   oga_present(vid, page->fb_id);

   vid->cur_page = (vid->cur_page + 1) % NUM_PAGES;

   return true;
}

/* RGUI's frame, kept at its own depth: the RGA reads 16 and 32 bits
 * alike, so this is a row copy, not a pixel conversion. The surface
 * is made again only when the frame outgrows it or changes depth. */
static void oga_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   unsigned i;
   const uint8_t *src;
   uint8_t *dst;
   oga_video_t *vid = (oga_video_t*)data;
   unsigned w       = VIDEO_SCALE_W(dims);
   unsigned h       = VIDEO_SCALE_H(dims);
   int format       = rgb32 ? RK_FORMAT_BGRA_8888 : RK_FORMAT_RGB_565;
   size_t row       = (size_t)w * (rgb32 ? 4 : 2);

   if (!vid || !frame || !w || !h)
      return;

   if (     !vid->menu_surface
         ||  vid->menu_surface->rk_format != format
         ||  vid->menu_surface->width     <  (int)w
         ||  vid->menu_surface->height    <  (int)h)
   {
      oga_destroy_surface(vid->menu_surface);
      vid->menu_surface = oga_create_surface(vid->fd, w, h, format, true);
      if (!vid->menu_surface)
      {
         vid->menu_dims = 0;
         return;
      }
   }

   src = (const uint8_t*)frame;
   dst = vid->menu_surface->map;
   if (row == (size_t)vid->menu_surface->pitch)
      memcpy(dst, src, row * h);
   else
   {
      for (i = 0; i < h; i++)
      {
         memcpy(dst, src, row);
         src += row;
         dst += vid->menu_surface->pitch;
      }
   }
   vid->menu_dims = VIDEO_SCALE_PACK(w, h);
}

/* TODO/FIXME - implement */
static void oga_texture_enable(void *data, bool state, bool full_screen) { }
static void oga_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }

static bool oga_alive(void *data)
{
   return !frontend_driver_get_signal_handler_state();
}

static bool oga_focus(void *data) { return true; }
static bool oga_suppress_screensaver(void *a, bool b) { return false; }
static bool oga_has_windowed(void *data) { return false; }

static void oga_viewport_info(void *data, struct video_viewport *vp)
{
   oga_video_t *vid = (oga_video_t*)data;
   if (unlikely(!vid))
      return;

   vp->pos = VIDEO_POS_PACK(0, 0);
   vp->dims  = vp->full_dims  = VIDEO_SCALE_PACK(vid->mode.vdisplay,
         vid->mode.hdisplay);
}

static bool oga_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
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
   if (!vid || !vid->frame_surface)
      return false;

   /* The surface is allocated once, to the geometry the core declared
    * at init. A core that asks for more than that -- after raising its
    * geometry, say -- gets nothing rather than a buffer it would
    * render past the end of. */
   if (     (int)framebuffer->width  > vid->frame_surface->width
         || (int)framebuffer->height > vid->frame_surface->height)
      return false;

   framebuffer->format = vid->frame_surface->rk_format == RK_FORMAT_BGRA_8888 ?
      RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   framebuffer->data = (uint8_t*)vid->frame_surface->map;
   framebuffer->pitch = vid->frame_surface->pitch;

   return true;
}

static uint32_t oga_get_flags(void *data)
{
   uint32_t flags   = 0;
   oga_video_t *vid = (oga_video_t*)data;
   if (vid)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&vid->views_fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

video_poke_interface_t oga_poke_interface = {
   oga_get_flags,
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
   oga_set_texture_frame,
   oga_texture_enable,
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   oga_get_current_software_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void oga_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   *iface = &oga_poke_interface;
}

video_driver_t video_oga = {
   oga_init,
   oga_frame,
   oga_set_nonblock_state,
   oga_alive,
   oga_focus,
   oga_suppress_screensaver,
   oga_has_windowed,
   oga_set_shader,
   oga_free,
   "oga",
   NULL, /* set_viewport */
   oga_set_rotation,
   oga_viewport_info,
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   oga_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
