/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2015 - Tobias Jakobi
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <dirent.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <drm_fourcc.h>
#include <libdrm/exynos_drmif.h>
#include <exynos/exynos_fimg2d.h>

#include <retro_inline.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../common/drm_common.h"
#include <encodings/utf.h>

#include "../font_driver.h"
#include "../video_views.h"
#ifdef HAVE_OVERLAY
#include <formats/image.h>
#endif
#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../retroarch.h"

/* The G2D calls libdrm gives cannot rotate or mirror: a core's rotation
 * is applied as the frame is copied, a mirrored overlay image is written
 * mirrored into its buffer. */

/* Set to '1' to enable debug logging code. */
#define EXYNOS_GFX_DEBUG_LOG 0

/* Set to '1' to enable debug perf code. */
#define EXYNOS_GFX_DEBUG_PERF 0

/* GEM buffers for the 'data' from the frontend: the frame (main), the
 * menu (aux) and the messages (font), each its own, as the font buffer
 * keeps what it last drew */
enum exynos_buffer_type
{
  EXYNOS_BUFFER_MAIN = 0,
  EXYNOS_BUFFER_AUX,
  EXYNOS_BUFFER_FONT,
  EXYNOS_BUFFER_COUNT
};

/* We have to handle three types of 'data' from the frontend, each abstracted by a *
 * G2D image object. The image objects are then backed by some storage buffer.     *
 * (1) the core framebuffer (backed by main buffer)                                *
 * (2) the menu buffer (backed by aux buffer)                                      *
 * (3) the font rendering buffer (backed by font buffer)                           */
enum exynos_image_type
{
  EXYNOS_IMAGE_FRAME = 0,
  EXYNOS_IMAGE_FONT,
  EXYNOS_IMAGE_MENU,
  EXYNOS_IMAGE_COUNT
};

static const struct exynos_config_default
{
   unsigned dims;
   enum exynos_buffer_type buf_type;
   unsigned g2d_color_mode;
   unsigned bpp; /* bytes per pixel */
} defaults[EXYNOS_IMAGE_COUNT] = {
   {VIDEO_SCALE_PACK(1024, 640), EXYNOS_BUFFER_MAIN, G2D_COLOR_FMT_RGB565   | G2D_ORDER_AXRGB, 2}, /* frame */
   {VIDEO_SCALE_PACK(720, 368), EXYNOS_BUFFER_FONT, G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_AXRGB, 2}, /* font */
   {VIDEO_SCALE_PACK(400, 240), EXYNOS_BUFFER_AUX,  G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_RGBAX, 2}  /* menu */
};

struct exynos_data;

/* Frame addresses the G2D reads from: a core's buffers, a few at most */
#define EXYNOS_USERPTR_SLOTS 8

/* The size a frame address was last given to the G2D with. The kernel
 * keeps a mapping per address, made and synced when it is new, and
 * reuses it - unsynced - for the same address and size: so each frame
 * gives the address another size, which replaces it with a fresh one. */
struct exynos_userptr
{
   uintptr_t addr;
   unsigned long size;
   bool refused; /* the kernel would not map it: copied */
};

/* Where the exynos-drm-g2d devices are listed; one with an IOMMU group
 * reads userptr memory as one range */
static const char *exynos_g2d_driver_dir =
   "/sys/bus/platform/drivers/exynos-drm-g2d";

#if (EXYNOS_GFX_DEBUG_PERF == 1)
struct exynos_perf
{
  unsigned memcpy_calls;
  unsigned g2d_calls;

  unsigned long long memcpy_time;
  unsigned long long g2d_time;

  struct timespec tspec;
};
#endif

struct exynos_page
{
  struct exynos_bo *bo;
  uint32_t buf_id;

  struct exynos_data *base;

  bool used;      /* Set if page is currently used. */
  bool clear;     /* Set if page has to be cleared. */
};

struct exynos_data
{
  char drmname[32];
  struct exynos_device *device;

  /* G2D is used for scaling to framebuffer dimensions. */
  struct g2d_context *g2d;
  struct g2d_image *dst;
  struct g2d_image *src[EXYNOS_IMAGE_COUNT];

  struct exynos_bo *buf[EXYNOS_BUFFER_COUNT];

  struct exynos_page *pages;
  unsigned num_pages;

  /* currently displayed page */
  struct exynos_page *cur_page;

  unsigned pageflip_pending;

  /* The mode the CRTC runs, which g_drm_mode points at */
  drmModeModeInfo mode;

  /* framebuffer dimensions, VIDEO_SCALE_PACK's layout */
  unsigned dims;

  /* framebuffer aspect ratio */
  float aspect;
  /* The frame's aspect ratio on the screen */
  float frame_aspect;

  /* parameters for blitting core fb to screen */
  unsigned blit_params[6];

  /* bytes per pixel */
  unsigned bpp;

  /* framebuffer parameters */
  unsigned pitch, size;

  bool sync;
  /* A core's views are drawn, the menu over the whole screen */
  bool views_on;
  /* The main buffer went to the core to render its frame into */
  bool main_lent;
  /* The G2D reads a core's own frame memory, an IOMMU mapping it */
  bool userptr;

  /* The frame turned by the CPU, while a rotation is set */
  struct exynos_bo *turn;

  struct exynos_userptr up[EXYNOS_USERPTR_SLOTS];
  unsigned up_count;
  unsigned long page_size;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  struct exynos_perf perf;
#endif
};

static INLINE unsigned align_common(unsigned i, unsigned j)
{
   return (i + j - 1) & ~(j - 1);
}

/* Find the index of a compatible DRM device. */
static int exynos_get_device_index(void)
{
   drmVersionPtr ver;
   char buf[32]       = {0};
   int index          = 0;
   bool found         = false;

   while (!found)
   {
      int fd;

      snprintf(buf, sizeof(buf), "/dev/dri/card%d", index);

      fd = open(buf, O_RDWR);
      if (fd < 0) break;

      ver = drmGetVersion(fd);

      if (ver && string_is_equal(ver->name, "exynos"))
         found = true;
      else
         ++index;

      drmFreeVersion(ver);
      close(fd);
   }

   if (!found)
      return -1;
   return index;
}

/* The main pageflip handler, which the DRM executes
 * when it flips to the page.
 *
 * Decreases the pending pageflip count and
 * updates the current page.
 */
static void exynos_page_flip_handler(int fd, unsigned frame, unsigned sec,
      unsigned usec, void *data)
{
   struct exynos_page *page = data;

   if (page->base->cur_page)
      page->base->cur_page->used = false;

   page->base->pageflip_pending--;
   page->base->cur_page = page;
}

static struct exynos_page *exynos_get_free_page(
      struct exynos_page *p, unsigned cnt)
{
   unsigned i;

   for (i = 0; i < cnt; ++i)
   {
      if (!p[i].used)
         return &p[i];
   }

   return NULL;
}

/* Count the number of used pages. */
static unsigned exynos_pages_used(struct exynos_page *p, unsigned cnt)
{
   unsigned i;
   unsigned count = 0;

   for (i = 0; i < cnt; ++i)
   {
      if (p[i].used)
         ++count;
   }

   return count;
}

static void exynos_clean_up_pages(struct exynos_page *p, unsigned cnt)
{
   unsigned i;

   for (i = 0; i < cnt; ++i)
   {
      if (p[i].bo)
      {
         if (p[i].buf_id != 0)
            drmModeRmFB(g_drm_fd, p[i].buf_id);

         exynos_bo_destroy(p[i].bo);
      }
   }
}

#if (EXYNOS_GFX_DEBUG_LOG == 1)
static const char *exynos_buffer_name(enum exynos_buffer_type type)
{
   switch (type)
   {
      case EXYNOS_BUFFER_MAIN:
         return "main";
      case EXYNOS_BUFFER_AUX:
         return "aux";
      case EXYNOS_BUFFER_FONT:
         return "font";
      default:
         break;
   }

   return NULL;
}
#endif

/* Create a GEM buffer with userspace mapping.
 * Buffer is cleared after creation. */
static struct exynos_bo *exynos_create_mapped_buffer(
      struct exynos_device *dev, unsigned size)
{
   const unsigned flags = 0;
   struct exynos_bo *buf = exynos_bo_create(dev, size, flags);

   if (!buf)
   {
      RARCH_ERR("[Exynos] Failed to create temp buffer object.\n");
      return NULL;
   }

   if (!exynos_bo_map(buf))
   {
      RARCH_ERR("[Exynos] Failed to map temp buffer object.\n");
      exynos_bo_destroy(buf);
      return NULL;
   }

   memset(buf->vaddr, 0, size);

   return buf;
}

static int exynos_realloc_buffer(struct exynos_data *pdata,
      enum exynos_buffer_type type, unsigned size)
{
   struct exynos_bo *buf = pdata->buf[type];

   if (!buf)
      return -1;

   if (size > buf->size)
   {
      unsigned i;

#if (EXYNOS_GFX_DEBUG_LOG == 1)
      RARCH_LOG("[Exynos] Reallocating %s buffer (%u -> %u bytes).\n",
            exynos_buffer_name(type), buf->size, size);
#endif

      /* The cached frame can point into the buffer the core was lent */
      if (type == EXYNOS_BUFFER_MAIN && pdata->main_lent)
      {
         video_driver_cached_frame_retire();
         pdata->main_lent = false;
      }
      exynos_bo_destroy(buf);
      buf = exynos_create_mapped_buffer(pdata->device, size);

      if (!buf)
      {
         RARCH_ERR("[Exynos] Reallocation failed.\n");
         return -1;
      }

      pdata->buf[type] = buf;

      /* Map new GEM buffer to the G2D images backed by it. */
      for (i = 0; i < EXYNOS_IMAGE_COUNT; ++i)
      {
         if (defaults[i].buf_type == type)
            pdata->src[i]->bo[0] = buf->handle;
      }
   }

   return 0;
}

/* Clear a buffer associated to a G2D image by doing a (fast) solid fill. */
static int exynos_clear_buffer(struct g2d_context *g2d, struct g2d_image *img)
{
   int ret = g2d_solid_fill(g2d, img, 0, 0, img->width, img->height);

   if (ret == 0)
      ret = g2d_exec(g2d);

   if (ret != 0)
      RARCH_ERR("[Exynos] Failed to clear buffer using G2D.\n");

   return ret;
}

/* Put a font glyph at a position in the buffer that is backing the G2D font image object. */
static void exynos_put_glyph_rgba4444(struct exynos_data *pdata,
      const uint8_t *__restrict__ src,
      uint16_t color, unsigned g_width, unsigned g_height,
      unsigned g_pitch, unsigned dst_x, unsigned dst_y, unsigned *rect)
{
   unsigned x, y;
   const enum exynos_buffer_type buf_type = defaults[EXYNOS_IMAGE_FONT].buf_type;
   const              unsigned buf_width = pdata->src[EXYNOS_IMAGE_FONT]->width;
   uint16_t            *__restrict__ dst = (uint16_t*)pdata->buf[buf_type]->vaddr +
      dst_y * buf_width + dst_x;

   for (y = 0; y < g_height; ++y, src += g_pitch, dst += buf_width)
   {
      for (x = 0; x < g_width; ++x)
      {
         const uint16_t blend = src[x];
         dst[x] = color | ((blend << 8) & 0xf000);
      }
   }

   /* What the glyphs cover: cleared and blended, and no more */
   if (rect[2] <= rect[0])
   {
      rect[0] = dst_x;
      rect[1] = dst_y;
      rect[2] = dst_x + g_width;
      rect[3] = dst_y + g_height;
   }
   else
   {
      rect[0] = MIN(rect[0], dst_x);
      rect[1] = MIN(rect[1], dst_y);
      rect[2] = MAX(rect[2], dst_x + g_width);
      rect[3] = MAX(rect[3], dst_y + g_height);
   }
}

#if (EXYNOS_GFX_DEBUG_PERF == 1)
static void exynos_perf_init(struct exynos_perf *p)
{
   p->memcpy_calls = 0;
   p->g2d_calls = 0;

   p->memcpy_time = 0;
   p->g2d_time = 0;

   memset(&p->tspec, 0, sizeof(struct timespec));
}

static void exynos_perf_finish(struct exynos_perf *p)
{
   RARCH_DBG("[Exynos] total memcpy calls: %u\n", p->memcpy_calls);
   RARCH_DBG("[Exynos] total g2d calls: %u\n", p->g2d_calls);

   RARCH_DBG("[Exynos] total memcpy time: %f seconds\n",
         (double)p->memcpy_time / 1000000.0);
   RARCH_DBG("[Exynos] total g2d time: %f seconds\n",
         (double)p->g2d_time / 1000000.0);

   RARCH_DBG("[Exynos] average time per memcpy call: %f microseconds\n",
         (double)p->memcpy_time / (double)p->memcpy_calls);
   RARCH_DBG("[Exynos] average time per g2d call: %f microseconds\n",
         (double)p->g2d_time / (double)p->g2d_calls);
}

static void exynos_perf_memcpy(struct exynos_perf *p, bool start)
{
   if (start)
      clock_gettime(CLOCK_MONOTONIC, &p->tspec);
   else
   {
      struct timespec new;
      clock_gettime(CLOCK_MONOTONIC, &new);

      p->memcpy_time += (new.tv_sec - p->tspec.tv_sec) * 1000000;
      p->memcpy_time += (new.tv_nsec - p->tspec.tv_nsec) / 1000;
      ++p->memcpy_calls;
   }
}

static void exynos_perf_g2d(struct exynos_perf *p, bool start)
{
   if (start)
      clock_gettime(CLOCK_MONOTONIC, &p->tspec);
   else
   {
      struct timespec new;
      clock_gettime(CLOCK_MONOTONIC, &new);

      p->g2d_time += (new.tv_sec - p->tspec.tv_sec) * 1000000;
      p->g2d_time += (new.tv_nsec - p->tspec.tv_nsec) / 1000;
      ++p->g2d_calls;
   }
}
#endif

static int exynos_g2d_init(struct exynos_data *pdata)
{
   unsigned i;
   struct g2d_image *dst = NULL;
   struct g2d_context *g2d = g2d_init(g_drm_fd);
   if (!g2d)
      return -1;

   dst = calloc(1, sizeof(struct g2d_image));
   if (!dst)
      goto fail;

   dst->buf_type   = G2D_IMGBUF_GEM;
   dst->color_mode = (pdata->bpp == 2) ? G2D_COLOR_FMT_RGB565 | G2D_ORDER_AXRGB :
      G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_AXRGB;
   dst->width      = VIDEO_SCALE_W(pdata->dims);
   dst->height     = VIDEO_SCALE_H(pdata->dims);
   dst->stride     = pdata->pitch;
   dst->color      = 0xff000000; /* Clear color for solid fill operation. */

   for (i = 0; i < EXYNOS_IMAGE_COUNT; ++i)
   {
      const enum exynos_buffer_type buf_type = defaults[i].buf_type;
      const unsigned buf_size = VIDEO_SCALE_AREA(defaults[i].dims)
            * defaults[i].bpp;
      struct g2d_image *src   = (struct g2d_image*)calloc(1, sizeof(struct g2d_image));
      if (!src)
         break;

      src->width       = VIDEO_SCALE_W(defaults[i].dims);
      src->height      = VIDEO_SCALE_H(defaults[i].dims);
      src->stride      = src->width * defaults[i].bpp;

      src->color_mode  = defaults[i].g2d_color_mode;

      /* Associate GEM buffer storage with G2D image. */
      src->buf_type    = G2D_IMGBUF_GEM;
      src->bo[0]       = pdata->buf[buf_type]->handle;

      src->repeat_mode = G2D_REPEAT_MODE_PAD; /* Pad creates no border artifacts. */

      /* Make sure that the storage buffer is large enough. If the code is working *
       * properly, then this is just a NOP. Still put it here as an insurance.     */
      exynos_realloc_buffer(pdata, buf_type, buf_size);

      pdata->src[i]    = src;
   }

   if (i != EXYNOS_IMAGE_COUNT)
   {
      while (i-- > 0)
      {
         free(pdata->src[i]);
         pdata->src[i] = NULL;
      }
      goto fail_src;
   }

   pdata->dst = dst;
   pdata->g2d = g2d;

   return 0;

fail_src:
   free(dst);

fail:
   g2d_fini(g2d);

   return -1;
}

static void exynos_g2d_free(struct exynos_data *pdata)
{
   unsigned i;

   free(pdata->dst);

   for (i = 0; i < EXYNOS_IMAGE_COUNT; ++i)
   {
      free(pdata->src[i]);
      pdata->src[i] = NULL;
   }

   g2d_fini(pdata->g2d);
}

static int exynos_open(struct exynos_data *pdata)
{
   int fd                                 = -1;
   char buf[32]                           = {0};
   int devidx                             = exynos_get_device_index();
   settings_t *settings                   = config_get_ptr();

   if (pdata)
      g_drm_fd                            = -1;

   if (devidx != -1)
      snprintf(buf, sizeof(buf), "/dev/dri/card%d", devidx);
   else
   {
      RARCH_ERR("[Exynos] No compatible DRM device found.\n");
      return -1;
   }

   fd = open(buf, O_RDWR);

   if (fd < 0)
   {
      RARCH_ERR("[Exynos] Can't open DRM device.\n");
      return -1;
   }

   if (!drm_get_resources(fd))
      goto fail;

   if (!drm_get_connector(fd, settings->uints.video_monitor_index))
      goto fail;

   if (!drm_get_encoder(fd))
      goto fail;

   /* Setup the flip handler. */
   g_drm_fds.fd                         = fd;
   g_drm_fds.events                     = POLLIN;
   g_drm_evctx.version                  = DRM_EVENT_CONTEXT_VERSION;
   g_drm_evctx.page_flip_handler        = exynos_page_flip_handler;

   strlcpy(pdata->drmname, buf, sizeof(pdata->drmname));
   g_drm_fd = fd;

   RARCH_LOG("[Exynos] using DRM device \"%s\" with connector id %u.\n",
         pdata->drmname, g_drm_connector->connector_id);

   return 0;

fail:
   drm_free();
   close(fd);

   return -1;
}

/* Counterpart to exynos_open. */
static void exynos_close(struct exynos_data *pdata)
{
   memset(pdata->drmname, 0, sizeof(char) * 32);

   drm_free();
   close(g_drm_fd);
   g_drm_fd   = -1;
}

/* The page size, pitch and aspect of the mode the CRTC runs */
static void exynos_take_mode(struct exynos_data *pdata,
      const drmModeModeInfo *mode)
{
   pdata->mode   = *mode;
   g_drm_mode    = &pdata->mode;
   pdata->dims   = VIDEO_SCALE_PACK(mode->hdisplay, mode->vdisplay);
   pdata->aspect = (float)mode->hdisplay / (float)mode->vdisplay;
   pdata->pitch  = pdata->bpp * VIDEO_SCALE_W(pdata->dims);
   pdata->size   = pdata->pitch * VIDEO_SCALE_H(pdata->dims);
}

static int exynos_data_init(struct exynos_data *pdata, unsigned bpp)
{
   drmModeModeInfo custom;
   const drmModeModeInfo *m    = NULL;
   settings_t *settings        = config_get_ptr();
   unsigned video_fullscreen_x = settings->uints.video_fullscreen_x;
   unsigned video_fullscreen_y = settings->uints.video_fullscreen_y;

   /* The CRT switch's modeline, HDMI timings, the fullscreen size the
    * connector has closest at the refresh rate set, else its native
    * mode */
   if (     drm_mode_from_crt_switch(&custom)
         || drm_mode_from_timings(settings->arrays.crt_switch_timings,
               &custom))
      m = &custom;
   else if (video_fullscreen_x != 0 && video_fullscreen_y != 0)
   {
      if (!(m = drm_find_mode(g_drm_connector, video_fullscreen_x,
                  video_fullscreen_y, 1.0f,
                  settings->floats.video_refresh_rate)))
      {
         RARCH_ERR(
               "[Exynos] Requested resolution (%ux%u) not available.\n",
               video_fullscreen_x,
               video_fullscreen_y);
         goto fail;
      }
   }
   else
      m = &g_drm_connector->modes[0];

   if (m->hdisplay == 0 || m->vdisplay == 0)
   {
      RARCH_ERR("[Exynos] Failed to select sane resolution.\n");
      goto fail;
   }

   drm_setup(g_drm_fd);

   pdata->bpp        = bpp;
   exynos_take_mode(pdata, m);

   /* Always use triple buffering to reduce chance of tearing. */
   pdata->num_pages  = 3;

   RARCH_LOG("[Exynos] Selected %ux%u resolution with %u bpp.\n",
         VIDEO_SCALE_W(pdata->dims), VIDEO_SCALE_H(pdata->dims),
         pdata->bpp);

   return 0;

fail:
   drm_restore_crtc();

   g_drm_mode         = NULL;

   return -1;
}

/* Counterpart to exynos_data_init. */
static void exynos_deinit(struct exynos_data *pdata)
{
   drm_restore_crtc();

   g_drm_mode       = NULL;
   pdata->dims      = 0;
   pdata->num_pages = 0;
   pdata->bpp       = 0;
   pdata->pitch     = 0;
   pdata->size      = 0;
}

/* The pages at the mode's size, each a framebuffer, the CRTC set on
 * the last of them at the mode */
static int exynos_alloc_pages(struct exynos_data *pdata,
      struct exynos_device *device)
{
   unsigned i;
   uint32_t pixel_format;
   uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
   struct exynos_page *pages = (struct exynos_page*)calloc(pdata->num_pages,
         sizeof(struct exynos_page));

   if (!pages)
   {
      RARCH_ERR("[Exynos] Failed to allocate pages.\n");
      return -1;
   }

   for (i = 0; i < pdata->num_pages; ++i)
   {
      struct exynos_bo *bo = exynos_bo_create(device, pdata->size, 0);
      if (!bo)
      {
         RARCH_ERR("[Exynos] Failed to create buffer object.\n");
         goto fail;
      }

      /* Mapped only when a screenshot reads one back */
      pages[i].bo    = bo;
      pages[i].base  = pdata;

      pages[i].used  = false;
      pages[i].clear = true;
   }

   pixel_format = (pdata->bpp == 2) ? DRM_FORMAT_RGB565 : DRM_FORMAT_XRGB8888;
   pitches[0]   = pdata->pitch;
   offsets[0]   = 0;

   for (i = 0; i < pdata->num_pages; ++i)
   {
      handles[0] = pages[i].bo->handle;

      if (drmModeAddFB2(g_drm_fd, VIDEO_SCALE_W(pdata->dims),
               VIDEO_SCALE_H(pdata->dims),
               pixel_format, handles, pitches, offsets,
               &pages[i].buf_id, 0))
      {
         RARCH_ERR("[Exynos] Failed to add bo %u to fb.\n", i);
         goto fail;
      }
   }

   /* Setup CRTC: display the last allocated page. */
   if (drmModeSetCrtc(g_drm_fd, g_crtc_id,
            pages[pdata->num_pages - 1].buf_id,
            0, 0, &g_connector_id, 1, g_drm_mode))
   {
      RARCH_ERR("[Exynos] Initial CRTC setup failed.\n");
      goto fail;
   }

   pdata->pages    = pages;
   pdata->cur_page = NULL;
   return 0;

fail:
   exynos_clean_up_pages(pages, pdata->num_pages);
   free(pages);
   return -1;
}

static int exynos_alloc(struct exynos_data *pdata)
{
   unsigned i;
   struct exynos_device *device = exynos_device_create(g_drm_fd);

   if (!device)
   {
      RARCH_ERR("[Exynos] Failed to create device.\n");
      return -1;
   }

   for (i = 0; i < EXYNOS_BUFFER_COUNT; ++i)
   {
      unsigned j, buffer_size = 0;
      struct exynos_bo *bo;
      /* As large as the largest image it backs */
      for (j = 0; j < EXYNOS_IMAGE_COUNT; ++j)
         if (     defaults[j].buf_type == (enum exynos_buffer_type)i
               && VIDEO_SCALE_AREA(defaults[j].dims) * defaults[j].bpp
                  > buffer_size)
            buffer_size = VIDEO_SCALE_AREA(defaults[j].dims) * defaults[j].bpp;

      if (!(bo = exynos_create_mapped_buffer(device, buffer_size)))
         break;
      pdata->buf[i] = bo;
   }

   if (i != EXYNOS_BUFFER_COUNT || exynos_alloc_pages(pdata, device) != 0)
   {
      for (i = 0; i < EXYNOS_BUFFER_COUNT; ++i)
      {
         if (pdata->buf[i])
            exynos_bo_destroy(pdata->buf[i]);
         pdata->buf[i] = NULL;
      }
      exynos_device_destroy(device);
      return -1;
   }

   pdata->device = device;
   return 0;
}

/* Counterpart to exynos_alloc. */
static void exynos_data_free(struct exynos_data *pdata)
{
   unsigned i;

   /* Disable the CRTC. */
   if (drmModeSetCrtc(g_drm_fd, g_crtc_id, 0,
            0, 0, NULL, 0, NULL))
      RARCH_WARN("[Exynos] Failed to disable the CRTC.\n");

   exynos_clean_up_pages(pdata->pages, pdata->num_pages);

   free(pdata->pages);
   pdata->pages = NULL;

   for (i = 0; i < EXYNOS_BUFFER_COUNT; ++i)
   {
      exynos_bo_destroy(pdata->buf[i]);
      pdata->buf[i] = NULL;
   }
   if (pdata->turn)
      exynos_bo_destroy(pdata->turn);
   pdata->turn = NULL;

   exynos_device_destroy(pdata->device);
   pdata->device = NULL;
}

#if (EXYNOS_GFX_DEBUG_LOG == 1)
static void exynos_alloc_status(struct exynos_data *pdata)
{
   unsigned i;
   struct exynos_page *pages = pdata->pages;

   RARCH_LOG("[Exynos] Allocated %u pages with %u bytes each (pitch = %u bytes).\n",
         pdata->num_pages, pdata->size, pdata->pitch);

   for (i = 0; i < pdata->num_pages; ++i)
   {
      RARCH_LOG("[Exynos] Page %u: BO at %p, buffer id = %u.\n",
            i, pages[i].bo, pages[i].buf_id);
   }
}
#endif

/* Find a free page, clear it if necessary, and return the page. If  *
 * no free page is available when called, wait for a page flip.      */
static struct exynos_page *exynos_free_page(struct exynos_data *pdata)
{
   struct exynos_page *page = NULL;
   struct g2d_image *dst = pdata->dst;

   /* Wait until a free page is available. */
   while (!page)
   {
      page = exynos_get_free_page(pdata->pages, pdata->num_pages);

      if (!page)
         drm_wait_flip(-1);
   }

   dst->bo[0] = page->bo->handle;

   if (page->clear)
   {
      if (exynos_clear_buffer(pdata->g2d, dst) == 0)
         page->clear = false;
   }

   page->used = true;
   return page;
}

static void exynos_setup_scale(struct exynos_data *pdata,
      unsigned width, unsigned height, unsigned src_bpp)
{
   unsigned i;
   unsigned w, h;
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_FRAME];
   const unsigned pw     = VIDEO_SCALE_W(pdata->dims);
   const unsigned ph     = VIDEO_SCALE_H(pdata->dims);
   /* The aspect the frame is shown at - the core's, the user's - else
    * its pixels square */
   const float aspect    = pdata->frame_aspect > 0.0f
      ? pdata->frame_aspect : (float)width / (float)height;

   src->width      = width;
   src->height     = height;
   src->color_mode = (src_bpp == 2) ?
      G2D_COLOR_FMT_RGB565 | G2D_ORDER_AXRGB:
      G2D_COLOR_FMT_XRGB8888 | G2D_ORDER_AXRGB;

   if (fabsf(pdata->aspect - aspect) < 0.0001f)
   {
      w = pw;
      h = ph;
   }
   else
   {
      if (pdata->aspect > aspect)
      {
         w = (float)pw * aspect / pdata->aspect;
         h = ph;
      }
      else
      {
         w = pw;
         h = (float)ph * pdata->aspect / aspect;
      }
   }

   pdata->blit_params[0] = (pw - w) / 2;
   pdata->blit_params[1] = (ph - h) / 2;
   pdata->blit_params[2] = w;
   pdata->blit_params[3] = h;
   pdata->blit_params[4] = width;
   pdata->blit_params[5] = height;

   for (i = 0; i < pdata->num_pages; ++i)
      pdata->pages[i].clear = true;
}

static void exynos_set_fake_blit(struct exynos_data *pdata)
{
   unsigned i;

   pdata->blit_params[0] = 0;
   pdata->blit_params[1] = 0;
   pdata->blit_params[2] = VIDEO_SCALE_W(pdata->dims);
   pdata->blit_params[3] = VIDEO_SCALE_H(pdata->dims);

   for (i = 0; i < pdata->num_pages; ++i)
      pdata->pages[i].clear = true;
}

/* Whether the G2D can be given a core's own memory: an IOMMU maps it,
 * else the kernel hands the G2D the first page's address for the whole
 * range and nothing tells */
static bool exynos_userptr_probe(void)
{
   struct dirent *e;
   bool found = false;
   DIR *d     = opendir(exynos_g2d_driver_dir);

   if (!d)
      return false;
   while (!found && (e = readdir(d)))
   {
      int fd;
      if (e->d_name[0] == '.')
         continue;
      if ((fd = openat(dirfd(d), e->d_name, O_RDONLY | O_DIRECTORY)) < 0)
         continue;
      found = faccessat(fd, "iommu_group", F_OK, 0) == 0;
      close(fd);
   }
   closedir(d);
   return found;
}

static struct exynos_userptr *exynos_userptr_find(
      struct exynos_data *pdata, uintptr_t addr)
{
   unsigned i;
   for (i = 0; i < pdata->up_count; i++)
      if (pdata->up[i].addr == addr)
         return &pdata->up[i];
   return NULL;
}

/* The frame read by the G2D where the core wrote it, when the kernel
 * can map it: false when it is to be copied */
static bool exynos_userptr_frame(struct exynos_data *pdata,
      const void *frame, unsigned src_pitch)
{
   struct g2d_image *src  = pdata->src[EXYNOS_IMAGE_FRAME];
   const uintptr_t addr   = (uintptr_t)frame;
   const unsigned bpp     = (src->color_mode & G2D_COLOR_FMT_MASK)
      == G2D_COLOR_FMT_RGB565 ? 2 : 4;
   const unsigned long need = (unsigned long)src_pitch
      * (pdata->blit_params[5] - 1) + pdata->blit_params[4] * bpp;
   /* Bytes past the frame on its last page: room for a second size */
   const unsigned long rem  = (addr + need) & (pdata->page_size - 1);
   const unsigned long room = rem ? pdata->page_size - rem : 0;
   struct exynos_userptr *up;
   unsigned long size       = need;

   if (     !pdata->userptr || (addr & 3) || (src_pitch % bpp)
         || !pdata->blit_params[5])
      return false;
   if (!(up = exynos_userptr_find(pdata, addr)))
   {
      /* Every address given stays known, or its size could repeat */
      if (pdata->up_count == EXYNOS_USERPTR_SLOTS)
         return false;
      up          = &pdata->up[pdata->up_count++];
      up->addr    = addr;
      up->size    = 0;
      up->refused = false;
   }
   if (up->refused)
      return false;
   if (up->size == need)
   {
      if (!room)
         return false;
      size = need + 1;
   }
   up->size                 = size;
   src->buf_type            = G2D_IMGBUF_USERPTR;
   src->user_ptr[0].userptr = (unsigned long)addr;
   src->user_ptr[0].size    = size;
   src->stride              = src_pitch;
   return true;
}

/* The frame into the GEM buffer the G2D reads it from */
static int exynos_copy_frame(
      struct exynos_data *pdata,
      const void *frame,
      unsigned src_pitch)
{
   const enum exynos_buffer_type
      buf_type           = defaults[EXYNOS_IMAGE_FRAME].buf_type;
   const unsigned size   = src_pitch * pdata->blit_params[5];
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_FRAME];

   src->buf_type = G2D_IMGBUF_GEM;
   if (exynos_realloc_buffer(pdata, buf_type, size) != 0)
      return -1;
   src->bo[0]    = pdata->buf[buf_type]->handle;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_memcpy(&pdata->perf, true);
#endif

   memcpy(pdata->buf[buf_type]->vaddr, frame, size);
   src->stride = src_pitch;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_memcpy(&pdata->perf, false);
#endif

   return 0;
}

/* @w x @h pixels of @src turned @rot quarter turns counter-clockwise
 * into @dst, written in order */
static void exynos_turn_copy(uint8_t *dst, size_t stride,
      const uint8_t *src, size_t pitch, unsigned w, unsigned h,
      unsigned bpp, unsigned rot)
{
   unsigned ox, oy;
   const unsigned ow = (rot & 1) ? h : w;
   const unsigned oh = (rot & 1) ? w : h;

   for (oy = 0; oy < oh; oy++, dst += stride)
   {
      ptrdiff_t step;
      const uint8_t *in;
      /* Where output row oy starts in the source, and its next pixel */
      switch (rot & 3)
      {
         case 1:
            step = (ptrdiff_t)pitch;
            in   = src + (size_t)(w - 1 - oy) * bpp;
            break;
         case 2:
            step = -(ptrdiff_t)bpp;
            in   = src + (size_t)(h - 1 - oy) * pitch + (size_t)(w - 1) * bpp;
            break;
         default:
            step = -(ptrdiff_t)pitch;
            in   = src + (size_t)(h - 1) * pitch + (size_t)oy * bpp;
            break;
      }
      if (bpp == 4)
      {
         uint32_t *out = (uint32_t*)dst;
         for (ox = 0; ox < ow; ox++, in += step)
            out[ox] = *(const uint32_t*)in;
      }
      else
      {
         uint16_t *out = (uint16_t*)dst;
         for (ox = 0; ox < ow; ox++, in += step)
            out[ox] = *(const uint16_t*)in;
      }
   }
}

/* A turned frame, @w x @h before the turn, into the buffer for it */
static int exynos_turn_frame(struct exynos_data *pdata, const void *frame,
      unsigned src_pitch, unsigned w, unsigned h, unsigned rot)
{
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_FRAME];
   const unsigned bpp    = (src->color_mode & G2D_COLOR_FMT_MASK)
      == G2D_COLOR_FMT_RGB565 ? 2 : 4;
   const unsigned stride = ((rot & 1) ? h : w) * bpp;
   const unsigned size   = stride * ((rot & 1) ? w : h);

   if (!pdata->turn || pdata->turn->size < size)
   {
      if (pdata->turn)
         exynos_bo_destroy(pdata->turn);
      if (!(pdata->turn = exynos_create_mapped_buffer(pdata->device, size)))
         return -1;
   }
   exynos_turn_copy((uint8_t*)pdata->turn->vaddr, stride,
         (const uint8_t*)frame, src_pitch, w, h, bpp, rot);
   src->buf_type = G2D_IMGBUF_GEM;
   src->bo[0]    = pdata->turn->handle;
   src->stride   = stride;
   return 0;
}

/* The frame where the G2D reads it: in the buffer the core was lent, in
 * the core's own memory, or copied into the GEM buffer */
static int exynos_upload_frame(
      struct exynos_data *pdata,
      const void *frame,
      unsigned src_pitch)
{
   const enum exynos_buffer_type
      buf_type           = defaults[EXYNOS_IMAGE_FRAME].buf_type;
   const unsigned size   = src_pitch * pdata->blit_params[5];
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_FRAME];

   /* A frame the core rendered into the buffer it was lent is there */
   if (frame == pdata->buf[buf_type]->vaddr && size <= pdata->buf[buf_type]->size)
   {
      src->buf_type = G2D_IMGBUF_GEM;
      src->bo[0]    = pdata->buf[buf_type]->handle;
      src->stride   = src_pitch;
      return 0;
   }
   if (exynos_userptr_frame(pdata, frame, src_pitch))
      return 0;
   return exynos_copy_frame(pdata, frame, src_pitch);
}

/* One scaled copy of the frame onto the page. Memory the kernel will not
 * map for the G2D (read-only, say) is copied, from now on. */
static int exynos_scale_frame(struct exynos_data *pdata,
      const void *frame, unsigned src_pitch,
      unsigned sx, unsigned sy, unsigned sw, unsigned sh,
      unsigned dx, unsigned dy, unsigned dw, unsigned dh)
{
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_FRAME];

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, true);
#endif

   if (g2d_copy_with_scale(pdata->g2d, src, pdata->dst,
            sx, sy, sw, sh, dx, dy, dw, dh, 0) != 0)
   {
      struct exynos_userptr *up;
      if (src->buf_type != G2D_IMGBUF_USERPTR)
         return -1;
      if ((up = exynos_userptr_find(pdata, (uintptr_t)frame)))
         up->refused = true;
      RARCH_WARN("[Exynos] The G2D cannot read the frame at %p: copied.\n",
            frame);
      if (     exynos_copy_frame(pdata, frame, src_pitch) != 0
            || g2d_copy_with_scale(pdata->g2d, src, pdata->dst,
               sx, sy, sw, sh, dx, dy, dw, dh, 0) != 0)
         return -1;
   }
   if (g2d_exec(pdata->g2d) != 0)
      return -1;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, false);
#endif

   return 0;
}

static int exynos_blit_frame(
      struct exynos_data *pdata,
      const void *frame,
      unsigned src_pitch)
{
   if (     exynos_upload_frame(pdata, frame, src_pitch) != 0
         || exynos_scale_frame(pdata, frame, src_pitch, 0, 0,
            pdata->blit_params[4], pdata->blit_params[5],
            pdata->blit_params[0], pdata->blit_params[1],
            pdata->blit_params[2], pdata->blit_params[3]) != 0)
   {
      RARCH_ERR("[Exynos] Failed to blit frame.\n");
      return -1;
   }
   return 0;
}

/* A core's views, laid out on the screen: the page cleared, then each
 * placement scaled by the G2D straight from the frame, cut to its own
 * eye's area and the screen. */
static int exynos_blit_views(
      struct exynos_data *pdata,
      const void *frame,
      unsigned src_pitch,
      const video_views_map_t *map,
      const video_views_layout_t *layout)
{
   unsigned i;

   if (exynos_upload_frame(pdata, frame, src_pitch) != 0)
      return -1;
   if (exynos_clear_buffer(pdata->g2d, pdata->dst) != 0)
      return -1;

   for (i = 0; i < layout->num_placements; i++)
   {
      video_views_rect_t cut, from;
      const video_views_placement_t *p = &layout->placements[i];

      if (!video_views_visible(layout, p, pdata->dims, &cut))
         continue;
      video_views_cut_source(&map->views[p->view], &p->dst, &cut, &from);

      if (exynos_scale_frame(pdata, frame, src_pitch,
               VIDEO_POS_X(from.pos), VIDEO_POS_Y(from.pos),
               VIDEO_SCALE_W(from.dims), VIDEO_SCALE_H(from.dims),
               VIDEO_POS_X(cut.pos), VIDEO_POS_Y(cut.pos),
               VIDEO_SCALE_W(cut.dims), VIDEO_SCALE_H(cut.dims)) != 0)
      {
         RARCH_ERR("[Exynos] Failed to blit view.\n");
         return -1;
      }
   }

   return 0;
}

#ifdef HAVE_MENU
static int exynos_blend_menu(struct exynos_data *pdata)
{
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_MENU];
   /* Over the frame, or the whole screen the views are laid out on */
   unsigned x = pdata->views_on ? 0 : pdata->blit_params[0];
   unsigned y = pdata->views_on ? 0 : pdata->blit_params[1];
   unsigned w = pdata->views_on
      ? VIDEO_SCALE_W(pdata->dims) : pdata->blit_params[2];
   unsigned h = pdata->views_on
      ? VIDEO_SCALE_H(pdata->dims) : pdata->blit_params[3];

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, true);
#endif

   if (g2d_scale_and_blend(pdata->g2d, src, pdata->dst, 0, 0,
            src->width, src->height, x, y, w, h, G2D_OP_INTERPOLATE) ||
         g2d_exec(pdata->g2d))
   {
      RARCH_ERR("[Exynos] Failed to blend menu.\n");
      return -1;
   }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, false);
#endif

   return 0;
}
#endif

static int exynos_flip(struct exynos_data *pdata, struct exynos_page *page)
{
   /* We don't queue multiple page flips. Without vsync, a frame that
    * comes while the last is still on its way is not shown: its page is
    * written over by the next. */
   if (pdata->pageflip_pending > 0)
   {
      if (!pdata->sync)
      {
         drm_wait_flip(0);
         if (pdata->pageflip_pending > 0)
         {
            page->used = false;
            return 0;
         }
      }
      else
         drm_wait_flip(-1);
   }

   /* Issue a page flip at the next vblank interval. */
   if (drmModePageFlip(g_drm_fd, g_crtc_id, page->buf_id,
            DRM_MODE_PAGE_FLIP_EVENT, page) != 0)
   {
      RARCH_ERR("[Exynos] Failed to issue page flip.\n");
      return -1;
   }
   else
   {
      pdata->pageflip_pending++;
   }

   /* On startup no frame is displayed. We therefore wait for the initial flip to finish. */
   if (!pdata->cur_page)
      drm_wait_flip(-1);

   return 0;
}

#ifdef HAVE_OVERLAY
/* An input overlay image, in a GEM buffer of its own that the G2D scales
 * and blends onto the page */
struct exynos_overlay_image
{
   struct exynos_bo *bo; /* px at the image's alpha */
   uint32_t *px;         /* premultiplied ARGB8888 */
   struct g2d_image img;
   float tex[4];
   float vert[4];
   float alpha;
   unsigned w, h;
   unsigned mirror;      /* bo holds px mirrored: 1 across, 2 up and down */
   bool stale;           /* bo does not hold px at alpha */
};

struct exynos_overlay
{
   struct exynos_overlay_image *img;
   unsigned count;
   bool enabled;
   bool fullscreen;
};
#endif

struct exynos_video
{
   struct exynos_data *data;
#ifdef HAVE_OVERLAY
   struct exynos_overlay ov;
#endif

   void *font;
   const font_renderer_driver_t *font_driver;
   uint16_t font_color; /* ARGB4444 */
   /* What the font buffer holds glyphs in: x0, y0, x1, y1 */
   unsigned font_rect[4];

   unsigned bytes_per_pixel;

   /* current dimensions of the core fb, VIDEO_SCALE_PACK's layout */
   unsigned dims;

   /* menu data */
   /* The frame's quarter turns, counter-clockwise */
   unsigned rotation;
   bool menu_active;

   bool aspect_changed;
   bool views_seen;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t views_fallback;
};

static int exynos_init_font(struct exynos_video *vid,
      const video_info_t *video)
{
   struct exynos_data *pdata = vid->data;
   struct g2d_image *src     = pdata->src[EXYNOS_IMAGE_FONT];
   const unsigned buf_height = VIDEO_SCALE_H(defaults[EXYNOS_IMAGE_FONT].dims);
   const unsigned buf_width  = align_common(pdata->aspect * (float)buf_height, 16);
   const unsigned buf_bpp    = defaults[EXYNOS_IMAGE_FONT].bpp;

   if (!video->font_enable)
      return 0;

   if (font_renderer_create_default(&vid->font_driver, &vid->font,
            (video->path_font && *video->path_font) ? video->path_font : NULL,
            video->font_size, FONT_ATLAS_FORMAT_A8))
   {
      /* The atlas may grow when a message needs more glyphs than it holds;
       * the glyphs are blitted from it in memory, so there is no texture
       * to make again */
      struct font_atlas *grow = vid->font_driver->get_atlas(vid->font);
      grow->max_dims = VIDEO_SCALE_PACK(2048, 2048);
   }
   else
   {
      RARCH_ERR("[Exynos] Creating font renderer failed.\n");
      return -1;
   }

   /* The font buffer color type is ARGB4444. */
   if (exynos_realloc_buffer(pdata, defaults[EXYNOS_IMAGE_FONT].buf_type,
            buf_width * buf_height * buf_bpp) != 0)
   {
      vid->font_driver->free(vid->font);
      vid->font = NULL;
      return -1;
   }

   src->width  = buf_width;
   src->height = buf_height;
   src->stride = buf_width * buf_bpp;

   return 0;
}

/* @msg into the font buffer from (@base_x, @base_y), each line one line
 * height below the last, what it covers added to font_rect */
static void exynos_draw_text(struct exynos_video *vid, const char *msg,
      size_t msg_len, int msg_base_x, int msg_base_y)
{
   struct exynos_data *pdata = vid->data;
   struct g2d_image *dst     = pdata->src[EXYNOS_IMAGE_FONT];
   const struct font_atlas *atlas = vid->font_driver->get_atlas(vid->font);
   const struct font_glyph* (*get_glyph)(void*, uint32_t)
                                          = vid->font_driver->get_glyph;
   void *font_data                        = vid->font;
   const struct font_glyph *glyph_q       = get_glyph(font_data, '?');
   struct font_line_metrics *line_metrics = NULL;
   int line_h                             = 0;
   int line_x                             = msg_base_x;
   int line_y                             = msg_base_y;

   vid->font_driver->get_line_metrics(font_data, &line_metrics);
   if (line_metrics)
      line_h = (int)line_metrics->height;

#define FONT_LAYOUT_ALIGNED 0
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(line_width); \
      (void)(count); \
      (void)(bytes); \
      line_x = msg_base_x; \
      line_y = msg_base_y + (line) * line_h; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int base_x, base_y; \
      int glyph_width, glyph_height; \
      int max_width, max_height; \
      const uint8_t *src = NULL; \
      base_x       = (line_x + (pen_x)) + glyph->draw_offset_x; \
      base_y       = (line_y + (pen_y)) + glyph->draw_offset_y; \
      glyph_width  = VIDEO_SCALE_W(glyph->dims); \
      glyph_height = VIDEO_SCALE_H(glyph->dims); \
      src = atlas->buffer + VIDEO_SCALE_W(glyph->atlas_pos) \
         + VIDEO_SCALE_H(glyph->atlas_pos) * atlas->width; \
      if (base_x < 0) \
      { \
         src -= base_x; \
         glyph_width += base_x; \
         base_x = 0; \
      } \
      if (base_y < 0) \
      { \
         src -= base_y * (int)atlas->width; \
         glyph_height += base_y; \
         base_y = 0; \
      } \
      max_width    = (int)dst->width - base_x; \
      max_height   = (int)dst->height - base_y; \
      if (max_width <= 0 || max_height <= 0 \
            || glyph_width <= 0 || glyph_height <= 0) \
         break; \
      if (glyph_width > max_width) \
         glyph_width = max_width; \
      if (glyph_height > max_height) \
         glyph_height = max_height; \
      exynos_put_glyph_rgba4444(pdata, src, vid->font_color, \
            glyph_width, glyph_height, \
            atlas->width, base_x, base_y, vid->font_rect); \
   } while (0)
#include "../font_layout.h"
}

/* The message and the statistics over the page: what the last ones
 * drew cleared, these drawn, and only the area they cover blended */
static int exynos_render_text(struct exynos_video *vid, const char *msg,
      const video_frame_info_t *info, bool stats)
{
   struct exynos_data *pdata = vid->data;
   struct g2d_image *dst     = pdata->src[EXYNOS_IMAGE_FONT];
   unsigned *r               = vid->font_rect;
   const char *stat          = stats && info && info->statistics_show
      && info->stat_text_len ? info->stat_text : NULL;
   unsigned fw, fh, sw, sh;

   if (!vid->font || !vid->font_driver || !info)
      return 0;

   if (r[2] > r[0] && r[3] > r[1])
   {
      if (     g2d_solid_fill(pdata->g2d, dst, r[0], r[1], r[2] - r[0],
                  r[3] - r[1])
            || g2d_exec(pdata->g2d))
      {
         RARCH_ERR("[Exynos] Failed to clear the messages.\n");
         return -1;
      }
   }
   r[0] = r[1] = r[2] = r[3] = 0;

   {
      int cr = (int)(info->font_msg_color_r * 15.0f);
      int cg = (int)(info->font_msg_color_g * 15.0f);
      int cb = (int)(info->font_msg_color_b * 15.0f);
      vid->font_color = (uint16_t)(((cb < 0 ? 0 : (cb > 15 ? 15 : cb)) << 0)
         | ((cg < 0 ? 0 : (cg > 15 ? 15 : cg)) << 4)
         | ((cr < 0 ? 0 : (cr > 15 ? 15 : cr)) << 8));
   }
   if (msg && *msg)
      exynos_draw_text(vid, msg, strlen(msg),
            (int)(info->font_msg_pos_x * dst->width),
            (int)((1.0f - info->font_msg_pos_y) * dst->height));
   if (stat)
      exynos_draw_text(vid, stat, info->stat_text_len,
            (int)(info->osd_stat_params.x * dst->width),
            (int)((1.0f - info->osd_stat_params.y) * dst->height));

   if (r[2] <= r[0] || r[3] <= r[1])
      return 0;

   /* The font buffer covers the screen */
   fw = dst->width;
   fh = dst->height;
   sw = VIDEO_SCALE_W(pdata->dims);
   sh = VIDEO_SCALE_H(pdata->dims);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, true);
#endif

   if (g2d_scale_and_blend(pdata->g2d, dst, pdata->dst,
            r[0], r[1], r[2] - r[0], r[3] - r[1],
            r[0] * sw / fw, r[1] * sh / fh,
            MAX(1u, (r[2] - r[0]) * sw / fw), MAX(1u, (r[3] - r[1]) * sh / fh),
            G2D_OP_INTERPOLATE) || g2d_exec(pdata->g2d))
   {
      RARCH_ERR("[Exynos] Failed to blend font.\n");
      return -1;
   }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, false);
#endif

   return 1;
}

static void exynos_clear_pages(struct exynos_data *pdata)
{
   unsigned i;
   for (i = 0; i < pdata->num_pages; ++i)
      pdata->pages[i].clear = true;
}

#ifdef HAVE_OVERLAY
#define EXYNOS_DIV255(x) ((((x) + 1) + ((x) >> 8)) >> 8)

static void exynos_overlay_free_images(struct exynos_overlay *ov)
{
   unsigned i;
   for (i = 0; i < ov->count; i++)
   {
      if (ov->img[i].bo)
         exynos_bo_destroy(ov->img[i].bo);
      free(ov->img[i].px);
   }
   free(ov->img);
   ov->img   = NULL;
   ov->count = 0;
}

static bool exynos_overlay_load(void *data, const void *image_data,
      unsigned num_images)
{
   unsigned i;
   size_t k, n;
   struct exynos_video *vid         = (struct exynos_video*)data;
   const struct texture_image *imgs = (const struct texture_image*)image_data;
   struct exynos_overlay *ov;

   if (!vid)
      return false;
   ov = &vid->ov;
   exynos_overlay_free_images(ov);
   exynos_clear_pages(vid->data);
   if (!num_images || !imgs)
      return true;
   if (!(ov->img = (struct exynos_overlay_image*)calloc(num_images,
               sizeof(*ov->img))))
      return false;
   ov->count = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct exynos_overlay_image *o = &ov->img[i];
      o->tex[2]  = o->tex[3]  = 1.0f;
      o->vert[2] = o->vert[3] = 1.0f;
      o->alpha   = 1.0f;
      /* The G2D's limit is 8000 a side */
      if (     !imgs[i].pixels || !imgs[i].width || !imgs[i].height
            || imgs[i].width > 8000 || imgs[i].height > 8000
            || imgs[i].pix10 || imgs[i].fp16)
         continue;
      n = (size_t)imgs[i].width * imgs[i].height;
      if (!(o->px = (uint32_t*)malloc(n * sizeof(uint32_t))))
         continue;
      if (!(o->bo = exynos_create_mapped_buffer(vid->data->device,
                  (unsigned)(n * sizeof(uint32_t)))))
      {
         free(o->px);
         o->px = NULL;
         continue;
      }
      o->w = imgs[i].width;
      o->h = imgs[i].height;
      /* Premultiplied, as the G2D's OVER blends */
      for (k = 0; k < n; k++)
      {
         uint32_t p = imgs[i].pixels[k];
         uint32_t a = p >> 24;
         uint32_t r = imgs[i].supports_rgba ? (p & 0xff) : ((p >> 16) & 0xff);
         uint32_t g = (p >> 8) & 0xff;
         uint32_t b = imgs[i].supports_rgba ? ((p >> 16) & 0xff) : (p & 0xff);
         o->px[k]   = (a << 24) | (EXYNOS_DIV255(r * a) << 16)
            | (EXYNOS_DIV255(g * a) << 8) | EXYNOS_DIV255(b * a);
      }
      o->img.buf_type    = G2D_IMGBUF_GEM;
      o->img.bo[0]       = o->bo->handle;
      o->img.width       = o->w;
      o->img.height      = o->h;
      o->img.stride      = o->w * 4;
      o->img.color_mode  = G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_AXRGB;
      o->img.repeat_mode = G2D_REPEAT_MODE_PAD;
      o->stale           = true;
   }
   return true;
}

static void exynos_overlay_tex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   float *t;
   if (!vid || index >= vid->ov.count)
      return;
   t    = vid->ov.img[index].tex;
   t[0] = x;
   t[1] = y;
   t[2] = w;
   t[3] = h;
}

static void exynos_overlay_vertex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   float *t;
   if (!vid || index >= vid->ov.count)
      return;
   t = vid->ov.img[index].vert;
   if (t[0] != x || t[1] != y || t[2] != w || t[3] != h)
   {
      t[0] = x;
      t[1] = y;
      t[2] = w;
      t[3] = h;
      /* Where it was is drawn over again */
      exynos_clear_pages(vid->data);
   }
}

static void exynos_overlay_full_screen(void *data, bool enable)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   if (vid && vid->ov.fullscreen != enable)
   {
      vid->ov.fullscreen = enable;
      exynos_clear_pages(vid->data);
   }
}

static void exynos_overlay_set_alpha(void *data, unsigned index, float mod)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   if (!vid || index >= vid->ov.count)
      return;
   if (vid->ov.img[index].alpha != mod)
   {
      vid->ov.img[index].alpha = mod;
      vid->ov.img[index].stale = true;
   }
}

static void exynos_overlay_enable(void *data, bool state)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   if (vid && vid->ov.enabled != state)
   {
      vid->ov.enabled = state;
      exynos_clear_pages(vid->data);
   }
}

/* Image @o's part on the screen, @base its overlay's area (x, y, w, h):
 * src and dst as x, y, w, h, src in the image as @mirror has it
 * mirrored. False when none of it is. */
static bool exynos_overlay_rect(const struct exynos_overlay_image *o,
      const float *base, unsigned sw, unsigned sh,
      unsigned *src, unsigned *dst, unsigned *mirror)
{
   float fx = base[0] + o->vert[0] * base[2];
   float fy = base[1] + o->vert[1] * base[3];
   float fw = o->vert[2] * base[2];
   float fh = o->vert[3] * base[3];
   float tx = o->tex[0] * o->w, ty = o->tex[1] * o->h;
   float tw = o->tex[2] * o->w, th = o->tex[3] * o->h;
   float x0, y0, x1, y1;
   int dx0, dy0, dx1, dy1, sx0, sy0, sx1, sy1;

   /* A quad and its texels running opposite ways: the image mirrored,
    * which the G2D cannot do as it scales; its buffer holds it so */
   *mirror = ((fw < 0.0f) != (tw < 0.0f) ? 1u : 0u)
      | ((fh < 0.0f) != (th < 0.0f) ? 2u : 0u);
   if (fw < 0.0f)
   {
      fx += fw;
      fw  = -fw;
   }
   if (fh < 0.0f)
   {
      fy += fh;
      fh  = -fh;
   }
   if (tw < 0.0f)
   {
      tx += tw;
      tw  = -tw;
   }
   if (th < 0.0f)
   {
      ty += th;
      th  = -th;
   }
   if (fw < 0.5f || fh < 0.5f || tw <= 0.0f || th <= 0.0f)
      return false;
   /* The same texels, where the mirrored image has them */
   if (*mirror & 1)
      tx = (float)o->w - tx - tw;
   if (*mirror & 2)
      ty = (float)o->h - ty - th;
   x0 = MAX(fx, 0.0f);
   y0 = MAX(fy, 0.0f);
   x1 = MIN(fx + fw, (float)sw);
   y1 = MIN(fy + fh, (float)sh);
   dx0 = (int)(x0 + 0.5f);
   dy0 = (int)(y0 + 0.5f);
   dx1 = (int)(x1 + 0.5f);
   dy1 = (int)(y1 + 0.5f);
   if (dx1 <= dx0 || dy1 <= dy0)
      return false;
   /* The texels under that part */
   sx0 = (int)(tx + (x0 - fx) * tw / fw + 0.5f);
   sy0 = (int)(ty + (y0 - fy) * th / fh + 0.5f);
   sx1 = (int)(tx + (x1 - fx) * tw / fw + 0.5f);
   sy1 = (int)(ty + (y1 - fy) * th / fh + 0.5f);
   sx0 = MAX(0, MIN(sx0, (int)o->w - 1));
   sy0 = MAX(0, MIN(sy0, (int)o->h - 1));
   sx1 = MAX(sx0 + 1, MIN(sx1, (int)o->w));
   sy1 = MAX(sy0 + 1, MIN(sy1, (int)o->h));
   src[0] = (unsigned)sx0;
   src[1] = (unsigned)sy0;
   src[2] = (unsigned)(sx1 - sx0);
   src[3] = (unsigned)(sy1 - sy0);
   dst[0] = (unsigned)dx0;
   dst[1] = (unsigned)dy0;
   dst[2] = (unsigned)(dx1 - dx0);
   dst[3] = (unsigned)(dy1 - dy0);
   return true;
}

static void exynos_overlay_base(struct exynos_video *vid, float *base)
{
   struct exynos_data *pdata = vid->data;
   if (vid->ov.fullscreen || pdata->views_on)
   {
      base[0] = base[1] = 0.0f;
      base[2] = (float)VIDEO_SCALE_W(pdata->dims);
      base[3] = (float)VIDEO_SCALE_H(pdata->dims);
   }
   else
   {
      base[0] = (float)pdata->blit_params[0];
      base[1] = (float)pdata->blit_params[1];
      base[2] = (float)pdata->blit_params[2];
      base[3] = (float)pdata->blit_params[3];
   }
}

static int exynos_fill(struct exynos_data *pdata,
      int x0, int y0, int x1, int y1)
{
   if (x1 <= x0 || y1 <= y0)
      return 0;
   return g2d_solid_fill(pdata->g2d, pdata->dst, (unsigned)x0, (unsigned)y0,
         (unsigned)(x1 - x0), (unsigned)(y1 - y0));
}

/* Under the overlay, where this frame did not draw: black again, or it
 * is blended over what it left there on this page before */
static int exynos_overlay_clear(struct exynos_video *vid, bool frame_drawn)
{
   unsigned i;
   float base[4];
   struct exynos_data *pdata = vid->data;
   struct exynos_overlay *ov = &vid->ov;
   /* The frame's rectangle; the whole page when views cleared it */
   int fx0 = frame_drawn ? (int)pdata->blit_params[0] : 0;
   int fy0 = frame_drawn ? (int)pdata->blit_params[1] : 0;
   int fx1 = frame_drawn ? fx0 + (int)pdata->blit_params[2] : 0;
   int fy1 = frame_drawn ? fy0 + (int)pdata->blit_params[3] : 0;
   bool queued = false;

   if (!ov->enabled || !ov->count || (frame_drawn && pdata->views_on))
      return 0;
   exynos_overlay_base(vid, base);
   for (i = 0; i < ov->count; i++)
   {
      unsigned src[4], dst[4], mirror;
      int x0, y0, x1, y1, my0, my1;
      if (     !ov->img[i].bo
            || !exynos_overlay_rect(&ov->img[i], base,
               VIDEO_SCALE_W(pdata->dims), VIDEO_SCALE_H(pdata->dims),
               src, dst, &mirror))
         continue;
      x0  = (int)dst[0];
      y0  = (int)dst[1];
      x1  = x0 + (int)dst[2];
      y1  = y0 + (int)dst[3];
      my0 = MAX(y0, MIN(fy0, y1));
      my1 = MIN(y1, MAX(fy1, my0));
      /* Above, below, and beside the frame */
      if (     exynos_fill(pdata, x0, y0, x1, my0) != 0
            || exynos_fill(pdata, x0, my1, x1, y1) != 0
            || exynos_fill(pdata, x0, my0, MIN(x1, fx0), my1) != 0
            || exynos_fill(pdata, MAX(x0, fx1), my0, x1, my1) != 0)
         return -1;
      queued = true;
   }
   return queued ? g2d_exec(pdata->g2d) : 0;
}

/* Each image scaled and blended over the page, premultiplied, at its
 * alpha: the G2D's blend takes none, so the alpha is in its buffer */
static int exynos_overlay_blend(struct exynos_video *vid)
{
   unsigned i, queued = 0;
   float base[4];
   struct exynos_data *pdata = vid->data;
   struct exynos_overlay *ov = &vid->ov;

   if (!ov->enabled || !ov->count)
      return 0;
   exynos_overlay_base(vid, base);
   for (i = 0; i < ov->count; i++)
   {
      unsigned src[4], dst[4], mirror;
      struct exynos_overlay_image *o = &ov->img[i];
      unsigned m = VIDEO_ALPHA_BYTE(o->alpha);

      if (     !o->bo || !m
            || !exynos_overlay_rect(o, base, VIDEO_SCALE_W(pdata->dims),
               VIDEO_SCALE_H(pdata->dims), src, dst, &mirror))
         continue;
      if (o->stale || o->mirror != mirror)
      {
         unsigned x, y;
         uint32_t *to = (uint32_t*)o->bo->vaddr;
         if (m == 0xFF && !mirror)
            memcpy(to, o->px, (size_t)o->w * o->h * sizeof(uint32_t));
         else
            for (y = 0; y < o->h; y++)
            {
               const uint32_t *row = o->px
                  + (size_t)((mirror & 2) ? o->h - 1 - y : y) * o->w;
               for (x = 0; x < o->w; x++, to++)
               {
                  uint32_t p = row[(mirror & 1) ? o->w - 1 - x : x];
                  if (m < 0xFF)
                     p = (EXYNOS_DIV255((p >> 24) * m) << 24)
                        | (EXYNOS_DIV255(((p >> 16) & 0xff) * m) << 16)
                        | (EXYNOS_DIV255(((p >> 8) & 0xff) * m) << 8)
                        | EXYNOS_DIV255((p & 0xff) * m);
                  *to = p;
               }
            }
         o->mirror = mirror;
         o->stale  = false;
      }
      if (g2d_scale_and_blend(pdata->g2d, &o->img, pdata->dst,
               src[0], src[1], src[2], src[3],
               dst[0], dst[1], dst[2], dst[3], G2D_OP_OVER) != 0)
         return -1;
      /* A command list each: run them before libdrm's limit */
      if (++queued == 16)
      {
         if (g2d_exec(pdata->g2d) != 0)
            return -1;
         queued = 0;
      }
   }
   return queued ? g2d_exec(pdata->g2d) : 0;
}

static const video_overlay_interface_t exynos_overlay_interface = {
   exynos_overlay_enable,
   exynos_overlay_load,
   NULL, /* load_textures */
   exynos_overlay_tex_geom,
   exynos_overlay_vertex_geom,
   exynos_overlay_full_screen,
   exynos_overlay_set_alpha
};

static void exynos_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &exynos_overlay_interface;
}
#endif

static void *exynos_init(const video_info_t *video)
{
   struct exynos_video *vid;
   const unsigned fb_bpp = 4; /* Use XRGB8888 framebuffer. */

   vid = calloc(1, sizeof(struct exynos_video));
   if (!vid)
      return NULL;

   vid->data = calloc(1, sizeof(struct exynos_data));
   if (!vid->data)
      goto fail_data;

   vid->bytes_per_pixel = video->rgb32 ? 4 : 2;

   if (exynos_open(vid->data) != 0)
   {
      RARCH_ERR("[Exynos] Open device failed.\n");
      goto fail;
   }

   if (exynos_data_init(vid->data, fb_bpp) != 0)
   {
      RARCH_ERR("[Exynos] Initialization failed.\n");
      goto fail_init;
   }

   if (exynos_alloc(vid->data) != 0)
   {
      RARCH_ERR("[Exynos] Allocation failed.\n");
      goto fail_alloc;
   }

   if (exynos_g2d_init(vid->data) != 0)
   {
      RARCH_ERR("[Exynos] G2D initialization failed.\n");
      goto fail_g2d;
   }

   {
      long page = sysconf(_SC_PAGESIZE);
      vid->data->page_size = page > 0 ? (unsigned long)page : 4096;
   }
   /* A core's frame read where it is, not copied, where an IOMMU maps it */
   if ((vid->data->userptr = exynos_userptr_probe()))
      RARCH_LOG("[Exynos] The G2D reads frames from the core's memory.\n");

#if (EXYNOS_GFX_DEBUG_LOG == 1)
   exynos_alloc_status(vid->data);
#endif

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_init(&vid->data->perf);
#endif


   if (exynos_init_font(vid, video) != 0)
   {
      RARCH_ERR("[Exynos] Font initialization failed.\n");
      goto fail_font;
   }

   vid->data->sync         = video->vsync;
   vid->data->frame_aspect = video_driver_get_aspect_ratio();

   /* What views are laid out on; refresh rate, resolutions and
    * switching through the KMS display server */
   video_driver_set_output_dims(vid->data->dims);
   video_driver_display_type_set(RARCH_DISPLAY_KMS);

   return vid;

fail_font:
   exynos_g2d_free(vid->data);

fail_g2d:
   exynos_data_free(vid->data);

fail_alloc:
   exynos_deinit(vid->data);

fail_init:
   exynos_close(vid->data);

fail:
   free(vid->data);

fail_data:
   free(vid);
   return NULL;
}

static void exynos_free(void *data)
{
   struct exynos_video *vid = data;
   struct exynos_data *pdata;

   if (!vid)
      return;

   pdata = vid->data;

#ifdef HAVE_OVERLAY
   exynos_overlay_free_images(&vid->ov);
#endif
   exynos_g2d_free(pdata);

   /* Flush pages: One page remains, the one being displayed at this moment. */
   while (exynos_pages_used(pdata->pages, pdata->num_pages) > 1)
      drm_wait_flip(-1);

   exynos_data_free(pdata);
   exynos_deinit(pdata);
   exynos_close(pdata);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_finish(&pdata->perf);
#endif

   free(pdata);

   if (vid->font && vid->font_driver)
      vid->font_driver->free(vid->font);

   free(vid);
}

/* Whether a core's views draw this frame: each placement blitted by the
 * G2D. Anaglyph and interlaced, which blend the eyes, draw the frame
 * whole. */
static bool exynos_views_frame(struct exynos_video *vid,
      const void *frame, unsigned dims, unsigned pitch,
      const video_frame_info_t *info, struct exynos_page *page)
{
   unsigned i;
   struct exynos_data *pdata = vid->data;
   bool views = info && frame && info->views.num_views
      && !info->views_layout.offscreen
      && video_views_fit_frame(&info->views, dims);

   retro_atomic_store_release_int(&vid->views_fallback,
         (info && info->views.num_views && !views) ? 1 : 0);
   if (views != pdata->views_on)
   {
      /* What the last one drew outside the other's rectangle goes */
      for (i = 0; i < pdata->num_pages; ++i)
         pdata->pages[i].clear = true;
      if (!views && exynos_clear_buffer(pdata->g2d, pdata->dst) == 0)
         page->clear = false;
      pdata->views_on = views;
   }
   if (views && !vid->views_seen)
   {
      vid->views_seen = true;
      RARCH_LOG("[Exynos] Views drawn straight from the frame.\n");
   }
   return views;
}

static bool exynos_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   unsigned width = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   struct exynos_video *vid = data;
   struct exynos_page *page = NULL;
#ifdef HAVE_MENU
   bool menu_is_alive       = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
   /* Check if neither menu nor core framebuffer is to be displayed. */
   if (!vid->menu_active && !frame)
      return true;
#endif

   if (frame)
   {
      if (vid->dims != dims || vid->aspect_changed)
      {
         /* Sanity check on new dimension parameters. */
         if (width == 0 || height == 0)
            return true;

         if (vid->dims != dims)
            RARCH_LOG("[Exynos] Resolution changed by core: %ux%u -> %ux%u.\n",
                  VIDEO_SCALE_W(vid->dims), VIDEO_SCALE_H(vid->dims),
                  width, height);
         /* Placed at the size it is shown, turned */
         if (vid->rotation & 1)
            exynos_setup_scale(vid->data, height, width,
                  vid->bytes_per_pixel);
         else
            exynos_setup_scale(vid->data, width, height,
                  vid->bytes_per_pixel);

         vid->dims           = dims;
         vid->aspect_changed = false;
      }

      page = exynos_free_page(vid->data);

      if (vid->rotation)
      {
         struct exynos_data *pd = vid->data;
         /* The views are laid out for the frame unturned: drawn whole */
         exynos_views_frame(vid, NULL, dims, pitch, video_info, page);
         if (     exynos_turn_frame(pd, frame, pitch, width, height,
                     vid->rotation) != 0
               || exynos_scale_frame(pd, NULL, pitch, 0, 0,
                     pd->blit_params[4], pd->blit_params[5],
                     pd->blit_params[0], pd->blit_params[1],
                     pd->blit_params[2], pd->blit_params[3]) != 0)
            goto fail;
      }
      else if (exynos_views_frame(vid, frame, dims, pitch, video_info, page))
      {
         if (exynos_blit_views(vid->data, frame, pitch,
                  &video_info->views, &video_info->views_layout) != 0)
            goto fail;
      }
      else if (exynos_blit_frame(vid->data, frame, pitch) != 0)
         goto fail;
   }

   /* If at this point the dimension parameters are still zero, setup some  *
    * fake blit parameters so that menu and font rendering work properly.   */
   if (     VIDEO_SCALE_W(vid->dims) == 0
         || VIDEO_SCALE_H(vid->dims) == 0)
      exynos_set_fake_blit(vid->data);

   if (!page)
      page = exynos_free_page(vid->data);

#ifdef HAVE_OVERLAY
   if (exynos_overlay_clear(vid, frame != NULL) != 0)
      goto fail;
   if (     video_info->overlay_behind_menu
         && exynos_overlay_blend(vid) != 0)
      goto fail;
#endif

#ifdef HAVE_MENU
   if (vid->menu_active)
   {
      if (exynos_blend_menu(vid->data) != 0)
         goto fail;
      menu_driver_frame(menu_is_alive, video_info);
   }
#endif

#ifdef HAVE_OVERLAY
   if (     !video_info->overlay_behind_menu
         && exynos_overlay_blend(vid) != 0)
      goto fail;
#endif

   /* The message, and the statistics where the menu is not over them */
   {
      int drawn = exynos_render_text(vid, msg, video_info,
#ifdef HAVE_MENU
            !vid->menu_active
#else
            true
#endif
            );
      if (drawn < 0)
         goto fail;
      /* Text may lie outside the frame: the page is cleared next time */
      if (drawn > 0)
         page->clear = true;
   }

   if (exynos_flip(vid->data, page) != 0)
      goto fail;

   return true;

fail:
   /* Since we didn't manage to issue a pageflip to this page, set *
    * it to 'unused' again, and hope that it works next time.      */
   page->used = false;

   return false;
}

static void exynos_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   struct exynos_video *vid = data;
   if (vid && vid->data)
      vid->data->sync = !state;
}

static bool exynos_alive(void *data) { return true; }
/* DRM device always has focus */
static bool exynos_focus(void *data) { return true; }
static bool exynos_suppress_screensaver(void *data, bool enable) { return false; }

static void exynos_set_rotation(void *data, unsigned rotation)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   if (vid && (rotation & 3) != vid->rotation)
   {
      vid->rotation       = rotation & 3;
      /* Placed again, at its turned size, with the next frame */
      vid->aspect_changed = true;
   }
}

/* Where the frame is on the screen; the whole screen while views draw */
static void exynos_viewport_info(void *data, struct video_viewport *vp)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   struct exynos_data *pdata;

   if (!vid)
      return;
   pdata         = vid->data;
   vp->full_dims = pdata->dims;
   if (pdata->views_on || !vid->dims)
   {
      vp->pos  = VIDEO_POS_PACK(0, 0);
      vp->dims = pdata->dims;
   }
   else
   {
      vp->pos  = VIDEO_POS_PACK(pdata->blit_params[0], pdata->blit_params[1]);
      vp->dims = VIDEO_SCALE_PACK(pdata->blit_params[2],
            pdata->blit_params[3]);
   }
}

/* The page on screen, where viewport_info says: BGR24, bottom row first.
 * The page is mapped the first time it is read. */
static bool exynos_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned x, y;
   struct video_viewport vp;
   const uint8_t *map;
   struct exynos_video *vid = (struct exynos_video*)data;
   struct exynos_data *pdata;
   unsigned px, py, w, h;

   if (!vid || !buffer || !(pdata = vid->data)->cur_page
         || pdata->bpp != 4)
      return false;
   if (     !pdata->cur_page->bo->vaddr
         && !exynos_bo_map(pdata->cur_page->bo))
      return false;
   map = (const uint8_t*)pdata->cur_page->bo->vaddr;
   exynos_viewport_info(vid, &vp);
   px = VIDEO_POS_X(vp.pos);
   py = VIDEO_POS_Y(vp.pos);
   w  = VIDEO_SCALE_W(vp.dims);
   h  = VIDEO_SCALE_H(vp.dims);
   for (y = 0; y < h; y++)
   {
      const uint32_t *in = (const uint32_t*)(map
            + (size_t)(py + y) * pdata->pitch) + px;
      uint8_t *out       = buffer + (size_t)(h - 1 - y) * w * 3;
      for (x = 0; x < w; x++, out += 3)
      {
         out[0] = (uint8_t)in[x];
         out[1] = (uint8_t)(in[x] >> 8);
         out[2] = (uint8_t)(in[x] >> 16);
      }
   }
   return true;
}

static void exynos_take_aspect(struct exynos_video *vid, float aspect)
{
   if (!vid || vid->data->frame_aspect == aspect)
      return;
   vid->data->frame_aspect = aspect;
   vid->aspect_changed     = true;
}

static void exynos_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   exynos_take_aspect((struct exynos_video*)data,
         aspectratio_lut[aspect_ratio_idx].value);
}

/* The CRT switch puts its aspect and asks for it to be taken up */
static void exynos_apply_state_changes(void *data)
{
   exynos_take_aspect((struct exynos_video*)data,
         video_driver_get_aspect_ratio());
}

/* The buffer the G2D reads the frame from, for the core to render into:
 * the frame then goes to the screen without a copy. Not to a core that
 * reads it back, as the mapping may be uncached. */
static bool exynos_get_current_software_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   struct exynos_data *pdata;
   unsigned bpp;

   if (     !vid || !fb || !fb->width || !fb->height
         || (fb->access_flags & RETRO_MEMORY_ACCESS_READ))
      return false;
   pdata = vid->data;
   bpp   = vid->bytes_per_pixel;
   if (exynos_realloc_buffer(pdata, EXYNOS_BUFFER_MAIN,
            fb->width * fb->height * bpp) != 0)
      return false;
   pdata->main_lent = true;
   fb->data         = pdata->buf[EXYNOS_BUFFER_MAIN]->vaddr;
   fb->pitch        = fb->width * bpp;
   fb->format       = bpp == 4
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   fb->memory_flags = 0;
   return true;
}

/* The connector's mode closest to @dims at the refresh rate set, or the
 * CRT switch's modeline: new pages at its size, the CRTC moved onto
 * them before the old ones go */
static void exynos_set_video_mode(void *data, unsigned dims, bool fullscreen)
{
   drmModeModeInfo custom, old_mode;
   const drmModeModeInfo *m;
   struct exynos_page *old_pages;
   struct exynos_video *vid = (struct exynos_video*)data;
   settings_t *settings     = config_get_ptr();
   struct exynos_data *pdata;

   if (!vid || !g_drm_connector)
      return;
   pdata = vid->data;
   if (drm_mode_from_crt_switch(&custom))
      m = &custom;
   else if (!(m = drm_find_mode(g_drm_connector, VIDEO_SCALE_W(dims),
               VIDEO_SCALE_H(dims), 1.0f,
               settings ? settings->floats.video_refresh_rate : 60.0f)))
      return;
   if (!memcmp(m, &pdata->mode, sizeof(*m)))
      return;

   while (pdata->pageflip_pending > 0)
      drm_wait_flip(-1);
   old_mode  = pdata->mode;
   old_pages = pdata->pages;
   exynos_take_mode(pdata, m);
   if (exynos_alloc_pages(pdata, pdata->device) != 0)
   {
      RARCH_ERR("[Exynos] Failed to set %ux%u.\n", m->hdisplay, m->vdisplay);
      exynos_take_mode(pdata, &old_mode);
      pdata->pages = old_pages;
      return;
   }
   exynos_clean_up_pages(old_pages, pdata->num_pages);
   free(old_pages);

   pdata->dst->width  = VIDEO_SCALE_W(pdata->dims);
   pdata->dst->height = VIDEO_SCALE_H(pdata->dims);
   pdata->dst->stride = pdata->pitch;
   vid->font_rect[0]  = vid->font_rect[1] = vid->font_rect[2]
      = vid->font_rect[3] = 0;
   if (vid->dims)
      exynos_setup_scale(pdata, VIDEO_SCALE_W(vid->dims),
            VIDEO_SCALE_H(vid->dims), vid->bytes_per_pixel);
   video_driver_set_output_dims(pdata->dims);
}

static void exynos_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   const enum exynos_buffer_type buf_type = defaults[EXYNOS_IMAGE_MENU].buf_type;
   struct exynos_video *vid = data;
   struct exynos_data *pdata = vid->data;
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_MENU];
   const unsigned size = VIDEO_SCALE_AREA(dims) * (rgb32 ? 4 : 2);

   if (exynos_realloc_buffer(pdata, buf_type, size) != 0)
      return;

   src->width = VIDEO_SCALE_W(dims);
   src->height = VIDEO_SCALE_H(dims);
   src->stride = VIDEO_SCALE_W(dims) * (rgb32 ? 4 : 2);
   src->color_mode = rgb32 ? G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_RGBAX :
      G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_RGBAX;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_memcpy(&pdata->perf, true);
#endif

   /* The G2D's blend takes no global alpha: the menu's own alpha, the
    * lowest nibble or byte of each pixel, is scaled as it is copied */
   if (alpha < 1.0f)
   {
      size_t i;
      unsigned a = alpha <= 0.0f ? 0u : (unsigned)(alpha * 256.0f);
      if (rgb32)
      {
         const uint32_t *in = (const uint32_t*)frame;
         uint32_t *out      = (uint32_t*)pdata->buf[buf_type]->vaddr;
         for (i = 0; i < size / 4; i++)
            out[i] = (in[i] & 0xffffff00u) | (((in[i] & 0xffu) * a) >> 8);
      }
      else
      {
         const uint16_t *in = (const uint16_t*)frame;
         uint16_t *out      = (uint16_t*)pdata->buf[buf_type]->vaddr;
         for (i = 0; i < size / 2; i++)
            out[i] = (uint16_t)((in[i] & 0xfff0u)
                  | (((in[i] & 0xfu) * a) >> 8));
      }
   }
   else
      memcpy(pdata->buf[buf_type]->vaddr, frame, size);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_memcpy(&pdata->perf, false);
#endif
}

static void exynos_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct exynos_video *vid = data;
   if (vid)
      vid->menu_active = state;
}

static void exynos_set_osd_msg(void *data, const char *msg, size_t msg_len,
      const struct font_params *params, void *font) { }
static void exynos_show_mouse(void *data, bool state) { }

static uint32_t exynos_get_flags(void *data)
{
   uint32_t flags           = 0;
   struct exynos_video *vid = (struct exynos_video*)data;
   if (vid)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&vid->views_fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

static const video_poke_interface_t exynos_poke_interface = {
   exynos_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   exynos_set_video_mode,
   NULL, /* refresh_rate - handled by display server */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   exynos_set_aspect_ratio,
   exynos_apply_state_changes,
   exynos_set_texture_frame,
   exynos_set_texture_enable,
   exynos_set_osd_msg,
   exynos_show_mouse,
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   exynos_get_current_software_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void exynos_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &exynos_poke_interface;
}

static bool exynos_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   return false;
}

video_driver_t video_exynos = {
   exynos_init,
   exynos_frame,
   exynos_set_nonblock_state,
   exynos_alive,
   exynos_focus,
   exynos_suppress_screensaver,
   NULL, /* has_windowed */
   exynos_set_shader,
   exynos_free,
   "exynos",
   NULL, /* set_viewport */
   exynos_set_rotation,
   exynos_viewport_info,
   exynos_read_viewport,
#ifdef HAVE_OVERLAY
   exynos_get_overlay_interface,
#endif
   exynos_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
