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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <drm_fourcc.h>
#include <libdrm/exynos_drmif.h>
#include <exynos/exynos_fimg2d.h>

#include <retro_inline.h>
#include <retro_assert.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../common/drm_common.h"
#include "../font_driver.h"
#include "../../configuration.h"
#include "../../retroarch.h"

/* TODO: Honor these properties: vsync, menu rotation, menu alpha, aspect ratio change */

/* Set to '1' to enable debug logging code. */
#define EXYNOS_GFX_DEBUG_LOG 0

/* Set to '1' to enable debug perf code. */
#define EXYNOS_GFX_DEBUG_PERF 0

extern void *memcpy_neon(void *dst, const void *src, size_t n);

/* We use two GEM buffers (main and aux) to handle 'data' from the frontend. */
enum exynos_buffer_type
{
  EXYNOS_BUFFER_MAIN = 0,
  EXYNOS_BUFFER_AUX,
  EXYNOS_BUFFER_COUNT
};

/* We have to handle three types of 'data' from the frontend, each abstracted by a *
 * G2D image object. The image objects are then backed by some storage buffer.     *
 * (1) the core framebuffer (backed by main buffer)                            *
 * (2) the menu buffer (backed by aux buffer)                                      *
 * (3) the font rendering buffer (backed by aux buffer)                            */
enum exynos_image_type
{
  EXYNOS_IMAGE_FRAME = 0,
  EXYNOS_IMAGE_FRONT,
  EXYNOS_IMAGE_MENU,
  EXYNOS_IMAGE_COUNT
};

static const struct exynos_config_default
{
   unsigned width, height;
   enum exynos_buffer_type buf_type;
   unsigned g2d_color_mode;
   unsigned bpp; /* bytes per pixel */
} defaults[EXYNOS_IMAGE_COUNT] = {
   {1024, 640, EXYNOS_BUFFER_MAIN, G2D_COLOR_FMT_RGB565   | G2D_ORDER_AXRGB, 2}, /* frame */
   {720,  368, EXYNOS_BUFFER_AUX,  G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_AXRGB, 2}, /* font */
   {400,  240, EXYNOS_BUFFER_AUX,  G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_RGBAX, 2}  /* menu */
};

struct exynos_data;

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

  /* framebuffer dimensions */
  unsigned width, height;

  /* framebuffer aspect ratio */
  float aspect;

  /* parameters for blitting core fb to screen */
  unsigned blit_params[6];

  /* bytes per pixel */
  unsigned bpp;

  /* framebuffer parameters */
  unsigned pitch, size;

  bool sync;

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

      if (string_is_equal(ver->name, "exynos"))
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

#if (EXYNOS_GFX_DEBUG_LOG == 1)
   RARCH_LOG("[video_exynos]: in exynos_page_flip_handler, page = %p\n", page);
#endif

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
      if (p[i].bo != NULL)
      {
         if (p[i].buf_id != 0)
            drmModeRmFB(p[i].buf_id, p[i].bo->handle);

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
      default:
         retro_assert(false);
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
      RARCH_ERR("[video_exynos]: failed to create temp buffer object\n");
      return NULL;
   }

   if (!exynos_bo_map(buf))
   {
      RARCH_ERR("[video_exynos]: failed to map temp buffer object\n");
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
      RARCH_LOG("[video_exynos]: reallocating %s buffer (%u -> %u bytes)\n",
            exynos_buffer_name(type), buf->size, size);
#endif

      exynos_bo_destroy(buf);
      buf = exynos_create_mapped_buffer(pdata->device, size);

      if (!buf)
      {
         RARCH_ERR("[video_exynos]: reallocation failed\n");
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
      RARCH_ERR("[video_exynos]: failed to clear buffer using G2D\n");

   return ret;
}

/* Put a font glyph at a position in the buffer that is backing the G2D font image object. */
static void exynos_put_glyph_rgba4444(struct exynos_data *pdata,
      const uint8_t *__restrict__ src,
      uint16_t color, unsigned g_width, unsigned g_height,
      unsigned g_pitch, unsigned dst_x, unsigned dst_y)
{
   unsigned x, y;
   const enum exynos_image_type buf_type = defaults[EXYNOS_IMAGE_FONT].buf_type;
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
   RARCH_LOG("[video_exynos]: debug: total memcpy calls: %u\n", p->memcpy_calls);
   RARCH_LOG("[video_exynos]: debug: total g2d calls: %u\n", p->g2d_calls);

   RARCH_LOG("[video_exynos]: debug: total memcpy time: %f seconds\n",
         (double)p->memcpy_time / 1000000.0);
   RARCH_LOG("[video_exynos]: debug: total g2d time: %f seconds\n",
         (double)p->g2d_time / 1000000.0);

   RARCH_LOG("[video_exynos]: debug: average time per memcpy call: %f microseconds\n",
         (double)p->memcpy_time / (double)p->memcpy_calls);
   RARCH_LOG("[video_exynos]: debug: average time per g2d call: %f microseconds\n",
         (double)p->g2d_time / (double)p->g2d_calls);
}

static void exynos_perf_memcpy(struct exynos_perf *p, bool start)
{
   if (start)
      clock_gettime(CLOCK_MONOTONIC, &p->tspec);
   else
   {
      struct timespec new = { 0 };
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
      struct timespec new = { 0 };
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
   dst->width      = pdata->width;
   dst->height     = pdata->height;
   dst->stride     = pdata->pitch;
   dst->color      = 0xff000000; /* Clear color for solid fill operation. */

   for (i = 0; i < EXYNOS_IMAGE_COUNT; ++i)
   {
      const enum exynos_buffer_type buf_type = defaults[i].buf_type;
      const unsigned buf_size = defaults[i].width * defaults[i].height * defaults[i].bpp;
      struct g2d_image *src   = (struct g2d_image*)calloc(1, sizeof(struct g2d_image));
      if (!src)
         break;

      src->width       = defaults[i].width;
      src->height      = defaults[i].height;
      src->stride      = defaults[i].width * defaults[i].bpp;

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
   unsigned i;
   int fd                                 = -1;
   char buf[32]                           = {0};
   int devidx                             = exynos_get_device_index();

   if (pdata)
      g_drm_fd                            = -1;

   if (devidx != -1)
      snprintf(buf, sizeof(buf), "/dev/dri/card%d", devidx);
   else
   {
      RARCH_ERR("[video_exynos]: no compatible DRM device found\n");
      return -1;
   }

   fd = open(buf, O_RDWR);

   if (fd < 0)
   {
      RARCH_ERR("[video_exynos]: can't open DRM device\n");
      return -1;
   }

   if (!drm_get_resources(fd))
      goto fail;

   if (!drm_get_decoder(fd))
      goto fail;

   if (!drm_get_encoder(fd))
      goto fail;

   /* Setup the flip handler. */
   g_drm_fds.fd                         = fd;
   g_drm_fds.events                     = POLLIN;
   g_drm_evctx.version                  = DRM_EVENT_CONTEXT_VERSION;
   g_drm_evctx.page_flip_handler        = exynos_page_flip_handler;

   strncpy(pdata->drmname, buf, sizeof(buf));
   g_drm_fd = fd;

   RARCH_LOG("[video_exynos]: using DRM device \"%s\" with connector id %u.\n",
         pdata->drmname, g_drm_connector->connector_id);

   return 0;

fail:
   drm_free();
   close(g_drm_fd);

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

static int exynos_init(struct exynos_data *pdata, unsigned bpp)
{
   unsigned i;
   settings_t *settings   = config_get_ptr();

   if (settings->uints.video_fullscreen_x != 0 &&
         settings->uints.video_fullscreen_y != 0)
   {
      for (i = 0; i < g_drm_connector->count_modes; i++)
      {
         if (g_drm_connector->modes[i].hdisplay == settings->uints.video_fullscreen_x &&
               g_drm_connector->modes[i].vdisplay == settings->uints.video_fullscreen_y)
         {
            g_drm_mode = &g_drm_connector->modes[i];
            break;
         }
      }

      if (!g_drm_mode)
      {
         RARCH_ERR("[video_exynos]: requested resolution (%ux%u) not available\n",
               settings->uints.video_fullscreen_x,
               settings->uints.video_fullscreen_y);
         goto fail;
      }

   }
   else
   {
      /* Select first mode, which is the native one. */
      g_drm_mode = &g_drm_connector->modes[0];
   }

   if (g_drm_mode->hdisplay == 0 || g_drm_mode->vdisplay == 0)
   {
      RARCH_ERR("[video_exynos]: failed to select sane resolution\n");
      goto fail;
   }

   drm_setup(g_drm_fd);

   pdata->width      = g_drm_mode->hdisplay;
   pdata->height     = g_drm_mode->vdisplay;

   pdata->aspect = (float)g_drm_mode->hdisplay / (float)g_drm_mode->vdisplay;

   /* Always use triple buffering to reduce chance of tearing. */
   pdata->num_pages  = 3;

   pdata->bpp        = bpp;
   pdata->pitch      = bpp * pdata->width;
   pdata->size       = pdata->pitch * pdata->height;

   RARCH_LOG("[video_exynos]: selected %ux%u resolution with %u bpp\n",
         pdata->width, pdata->height, pdata->bpp);

   return 0;

fail:
   drm_restore_crtc();

   g_drm_mode         = NULL;

   return -1;
}

/* Counterpart to exynos_init. */
static void exynos_deinit(struct exynos_data *pdata)
{
   drm_restore_crtc();

   g_drm_mode       = NULL;
   pdata->width     = 0;
   pdata->height    = 0;
   pdata->num_pages = 0;
   pdata->bpp       = 0;
   pdata->pitch     = 0;
   pdata->size      = 0;
}

static int exynos_alloc(struct exynos_data *pdata)
{
   struct exynos_bo *bo;
   struct exynos_page *pages;
   unsigned i;
   uint32_t pixel_format;
   const unsigned flags = 0;
   uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
   struct exynos_device *device = exynos_device_create(g_drm_fd);

   if (!device)
   {
      RARCH_ERR("[video_exynos]: failed to create device from fd\n");
      return -1;
   }

   pages = (struct exynos_page*)calloc(pdata->num_pages,
         sizeof(struct exynos_page));

   if (!pages)
   {
      RARCH_ERR("[video_exynos]: failed to allocate pages\n");
      goto fail_alloc;
   }

   for (i = 0; i < EXYNOS_BUFFER_COUNT; ++i)
   {
      const unsigned buffer_size = defaults[i].width * defaults[i].height * defaults[i].bpp;

      bo = exynos_create_mapped_buffer(device, buffer_size);
      if (!bo)
         break;

      pdata->buf[i] = bo;
   }

   if (i != EXYNOS_BUFFER_COUNT)
   {
      while (i-- > 0)
      {
         exynos_bo_destroy(pdata->buf[i]);
         pdata->buf[i] = NULL;
      }

      goto fail;
   }

   for (i = 0; i < pdata->num_pages; ++i)
   {
      bo = exynos_bo_create(device, pdata->size, flags);
      if (!bo)
      {
         RARCH_ERR("[video_exynos]: failed to create buffer object\n");
         goto fail;
      }

      /* Don't map the BO, since we don't access it through userspace. */

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

      if (drmModeAddFB2(g_drm_fd, pdata->width, pdata->height,
               pixel_format, handles, pitches, offsets,
               &pages[i].buf_id, flags))
      {
         RARCH_ERR("[video_exynos]: failed to add bo %u to fb\n", i);
         goto fail;
      }
   }

   /* Setup CRTC: display the last allocated page. */
   if (drmModeSetCrtc(g_drm_fd, g_crtc_id,
            pages[pdata->num_pages - 1].buf_id,
            0, 0, &g_drm_connector_id, 1, g_drm_mode))
   {
      RARCH_ERR("[video_exynos]: initial CRTC setup failed.\n");
      goto fail;
   }

   pdata->pages  = pages;
   pdata->device = device;

   return 0;

fail:
   exynos_clean_up_pages(pages, pdata->num_pages);

fail_alloc:
   exynos_device_destroy(device);

   return -1;
}

/* Counterpart to exynos_alloc. */
static void exynos_free(struct exynos_data *pdata)
{
   unsigned i;

   /* Disable the CRTC. */
   if (drmModeSetCrtc(g_drm_fd, g_crtc_id, 0,
            0, 0, NULL, 0, NULL))
      RARCH_WARN("[video_exynos]: failed to disable the CRTC.\n");

   exynos_clean_up_pages(pdata->pages, pdata->num_pages);

   free(pdata->pages);
   pdata->pages = NULL;

   for (i = 0; i < EXYNOS_BUFFER_COUNT; ++i)
   {
      exynos_bo_destroy(pdata->buf[i]);
      pdata->buf[i] = NULL;
   }

   exynos_device_destroy(pdata->device);
   pdata->device = NULL;
}

#if (EXYNOS_GFX_DEBUG_LOG == 1)
static void exynos_alloc_status(struct exynos_data *pdata)
{
   unsigned i;
   struct exynos_page *pages = pdata->pages;

   RARCH_LOG("[video_exynos]: Allocated %u pages with %u bytes each (pitch = %u bytes)\n",
         pdata->num_pages, pdata->size, pdata->pitch);

   for (i = 0; i < pdata->num_pages; ++i)
   {
      RARCH_LOG("[video_exynos]: page %u: BO at %p, buffer id = %u\n",
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
   const float aspect = (float)width / (float)height;

   src->width      = width;
   src->height     = height;
   src->color_mode = (src_bpp == 2) ?
      G2D_COLOR_FMT_RGB565 | G2D_ORDER_AXRGB:
      G2D_COLOR_FMT_XRGB8888 | G2D_ORDER_AXRGB;

   if (fabsf(pdata->aspect - aspect) < 0.0001f)
   {
      w = pdata->width;
      h = pdata->height;
   }
   else
   {
      if (pdata->aspect > aspect)
      {
         w = (float)pdata->width * aspect / pdata->aspect;
         h = pdata->height;
      }
      else
      {
         w = pdata->width;
         h = (float)pdata->height * pdata->aspect / aspect;
      }
   }

   pdata->blit_params[0] = (pdata->width - w) / 2;
   pdata->blit_params[1] = (pdata->height - h) / 2;
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
   pdata->blit_params[2] = pdata->width;
   pdata->blit_params[3] = pdata->height;

   for (i = 0; i < pdata->num_pages; ++i)
      pdata->pages[i].clear = true;
}

static int exynos_blit_frame(struct exynos_data *pdata, const void *frame,
                             unsigned src_pitch)
{
   const enum exynos_buffer_type buf_type = defaults[EXYNOS_IMAGE_FRAME].buf_type;
   const unsigned size   = src_pitch * pdata->blit_params[5];
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_FRAME];

   if (exynos_realloc_buffer(pdata, buf_type, size) != 0)
      return -1;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_memcpy(&pdata->perf, true);
#endif

   /* HACK: Without IOMMU the G2D only works properly between GEM buffers. */
   memcpy_neon(pdata->buf[buf_type]->vaddr, frame, size);
   src->stride = src_pitch;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_memcpy(&pdata->perf, false);
#endif

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, true);
#endif

   if (g2d_copy_with_scale(pdata->g2d, src, pdata->dst, 0, 0,
            pdata->blit_params[4], pdata->blit_params[5],
            pdata->blit_params[0], pdata->blit_params[1],
            pdata->blit_params[2], pdata->blit_params[3], 0) ||
         g2d_exec(pdata->g2d))
   {
      RARCH_ERR("[video_exynos]: failed to blit frame.\n");
      return -1;
   }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, false);
#endif

   return 0;
}

static int exynos_blend_menu(struct exynos_data *pdata,
                             unsigned rotation)
{
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_MENU];

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, true);
#endif

   if (g2d_scale_and_blend(pdata->g2d, src, pdata->dst, 0, 0,
            src->width, src->height, pdata->blit_params[0],
            pdata->blit_params[1], pdata->blit_params[2],
            pdata->blit_params[3], G2D_OP_INTERPOLATE) ||
         g2d_exec(pdata->g2d))
   {
      RARCH_ERR("[video_exynos]: failed to blend menu.\n");
      return -1;
   }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, false);
#endif

   return 0;
}

static int exynos_blend_font(struct exynos_data *pdata)
{
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_FONT];

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, true);
#endif

   if (g2d_scale_and_blend(pdata->g2d, src, pdata->dst, 0, 0, src->width,
            src->height, 0, 0, pdata->width, pdata->height,
            G2D_OP_INTERPOLATE) ||
         g2d_exec(pdata->g2d))
   {
      RARCH_ERR("[video_exynos]: failed to blend font\n");
      return -1;
   }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_g2d(&pdata->perf, false);
#endif

   return 0;
}

static int exynos_flip(struct exynos_data *pdata, struct exynos_page *page)
{
   /* We don't queue multiple page flips. */
   if (pdata->pageflip_pending > 0)
      drm_wait_flip(-1);

   /* Issue a page flip at the next vblank interval. */
   if (drmModePageFlip(g_drm_fd, g_crtc_id, page->buf_id,
            DRM_MODE_PAGE_FLIP_EVENT, page) != 0)
   {
      RARCH_ERR("[video_exynos]: failed to issue page flip\n");
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

struct exynos_video
{
   struct exynos_data *data;

   void *font;
   const font_renderer_driver_t *font_driver;
   uint16_t font_color; /* ARGB4444 */

   unsigned bytes_per_pixel;

   /* current dimensions of the core fb */
   unsigned width;
   unsigned height;

   /* menu data */
   unsigned menu_rotation;
   bool menu_active;

   bool aspect_changed;
};

static int exynos_init_font(struct exynos_video *vid)
{
   struct exynos_data *pdata = vid->data;
   struct g2d_image *src     = pdata->src[EXYNOS_IMAGE_FONT];
   const unsigned buf_height = defaults[EXYNOS_IMAGE_FONT].height;
   const unsigned buf_width  = align_common(pdata->aspect * (float)buf_height, 16);
   const unsigned buf_bpp    = defaults[EXYNOS_IMAGE_FONT].bpp;
   settings_t *settings      = config_get_ptr();

   if (!settings->bools.video_font_enable)
      return 0;

   if (font_renderer_create_default(&vid->font_driver, &vid->font,
            *settings->video.font_path ? settings->video.font_path : NULL,
            settings->floats.video_font_size))
   {
      const int r = settings->floats.video_msg_color_r * 15;
      const int g = settings->floats.video_msg_color_g * 15;
      const int b = settings->floats.video_msg_color_b * 15;

      vid->font_color = ((b < 0 ? 0 : (b > 15 ? 15 : b)) << 0) |
         ((g < 0 ? 0 : (g > 15 ? 15 : g)) << 4) |
         ((r < 0 ? 0 : (r > 15 ? 15 : r)) << 8);
   }
   else
   {
      RARCH_ERR("[video_exynos]: creating font renderer failed\n");
      return -1;
   }

   /* The font buffer color type is ARGB4444. */
   if (exynos_realloc_buffer(pdata, defaults[EXYNOS_IMAGE_FONT].buf_type,
            buf_width * buf_height * buf_bpp) != 0)
   {
      vid->font_driver->free(vid->font);
      return -1;
   }

   src->width = buf_width;
   src->height = buf_height;
   src->stride = buf_width * buf_bpp;

#if (EXYNOS_GFX_DEBUG_LOG == 1)
   RARCH_LOG("[video_exynos]: using font rendering image with size %ux%u\n",
         buf_width, buf_height);
#endif

   return 0;
}

static int exynos_render_msg(struct exynos_video *vid,
      const char *msg)
{
   const struct font_atlas *atlas;
   struct exynos_data *pdata = vid->data;
   struct g2d_image *dst     = pdata->src[EXYNOS_IMAGE_FONT];
   settings_t *settings      = config_get_ptr();
   int msg_base_x            = settings->floats.video_msg_pos_x * dst->width;
   int msg_base_y            = (1.0f - settings->floats.video_msg_pos_y) * dst->height;

   if (!vid->font || !vid->font_driver)
      return -1;

   if (exynos_clear_buffer(pdata->g2d, dst) != 0)
      return -1;

   atlas = vid->font_driver->get_atlas(vid->font);

   for (; *msg; ++msg)
   {
      int base_x, base_y;
      int glyph_width, glyph_height;
      const uint8_t *src = NULL;
      const struct font_glyph *glyph = vid->font_driver->get_glyph(vid->font, (uint8_t)*msg);
      if (!glyph)
         continue;

      base_x = msg_base_x + glyph->draw_offset_x;
      base_y = msg_base_y + glyph->draw_offset_y;

      const int max_width  = dst->width - base_x;
      const int max_height = dst->height - base_y;

      glyph_width  = glyph->width;
      glyph_height = glyph->height;

      src = atlas->buffer + glyph->atlas_offset_x + glyph->atlas_offset_y * atlas->width;

      if (base_x < 0)
      {
         src -= base_x;
         glyph_width += base_x;
         base_x = 0;
      }

      if (base_y < 0)
      {
         src -= base_y * (int)atlas->width;
         glyph_height += base_y;
         base_y = 0;
      }

      if (max_width <= 0 || max_height <= 0)
         continue;

      if (glyph_width > max_width)
         glyph_width = max_width;
      if (glyph_height > max_height)
         glyph_height = max_height;

      exynos_put_glyph_rgba4444(pdata, src, vid->font_color,
            glyph_width, glyph_height,
            atlas->width, base_x, base_y);

      msg_base_x += glyph->advance_x;
      msg_base_y += glyph->advance_y;
   }

   return exynos_blend_font(pdata);
}

static void *exynos_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
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
      RARCH_ERR("[video_exynos]: opening device failed\n");
      goto fail;
   }

   if (exynos_init(vid->data, fb_bpp) != 0)
   {
      RARCH_ERR("[video_exynos]: initialization failed\n");
      goto fail_init;
   }

   if (exynos_alloc(vid->data) != 0)
   {
      RARCH_ERR("[video_exynos]: allocation failed\n");
      goto fail_alloc;
   }

   if (exynos_g2d_init(vid->data) != 0)
   {
      RARCH_ERR("[video_exynos]: G2D initialization failed\n");
      goto fail_g2d;
   }

#if (EXYNOS_GFX_DEBUG_LOG == 1)
   exynos_alloc_status(vid->data);
#endif

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_init(&vid->data->perf);
#endif

   if (input && input_data)
      *input = NULL;

   if (exynos_init_font(vid) != 0)
   {
      RARCH_ERR("[video_exynos]: font initialization failed\n");
      goto fail_font;
   }

   return vid;

fail_font:
   exynos_g2d_free(vid->data);

fail_g2d:
   exynos_free(vid->data);

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

static void exynos_gfx_free(void *data)
{
   struct exynos_video *vid = data;
   struct exynos_data *pdata;

   if (!vid)
      return;

   pdata = vid->data;

   exynos_g2d_free(pdata);

   /* Flush pages: One page remains, the one being displayed at this moment. */
   while (exynos_pages_used(pdata->pages, pdata->num_pages) > 1)
      drm_wait_flip(-1);

   exynos_free(pdata);
   exynos_deinit(pdata);
   exynos_close(pdata);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_finish(&pdata->perf);
#endif

   free(pdata);

   if (vid->font != NULL && vid->font_driver != NULL)
      vid->font_driver->free(vid->font);

   free(vid);
}

static bool exynos_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   struct exynos_video *vid = data;
   struct exynos_page *page = NULL;

   /* Check if neither menu nor core framebuffer is to be displayed. */
   if (!vid->menu_active && !frame)
      return true;

   if (frame != NULL)
   {
      if (width != vid->width || height != vid->height)
      {
         /* Sanity check on new dimension parameters. */
         if (width == 0 || height == 0)
            return true;

         RARCH_LOG("[video_exynos]: resolution changed by core: %ux%u -> %ux%u\n",
               vid->width, vid->height, width, height);
         exynos_setup_scale(vid->data, width, height, vid->bytes_per_pixel);

         vid->width = width;
         vid->height = height;
      }

      page = exynos_free_page(vid->data);

      if (exynos_blit_frame(vid->data, frame, pitch) != 0)
         goto fail;
   }

   /* If at this point the dimension parameters are still zero, setup some  *
    * fake blit parameters so that menu and font rendering work properly.   */
   if (vid->width == 0 || vid->height == 0)
      exynos_set_fake_blit(vid->data);

   if (!page)
      page = exynos_free_page(vid->data);

   if (vid->menu_active)
   {
      if (exynos_blend_menu(vid->data, vid->menu_rotation) != 0)
         goto fail;
#ifdef HAVE_MENU
      menu_driver_frame(video_info);
#endif
   }
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = video_info ?
         (struct font_params*)&video_info->osd_stat_params : NULL;

      if (osd_params)
         font_driver_render_msg(vid, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
   }

   if (msg)
   {
      if (exynos_render_msg(vid, msg) != 0)
         goto fail;

      /* Font is blitted to the entire screen, so issue clear afterwards. */
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

static void exynos_gfx_set_nonblock_state(void *data, bool state)
{
   struct exynos_video *vid = data;
   if (vid && vid->data)
      vid->data->sync = !state;
}

static bool exynos_gfx_alive(void *data)
{
   (void)data;
   return true; /* always alive */
}

static bool exynos_gfx_focus(void *data)
{
   (void)data;
   return true; /* drm device always has focus */
}

static bool exynos_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static void exynos_gfx_set_rotation(void *data, unsigned rotation)
{
   struct exynos_video *vid = (struct exynos_video*)data;
   if (vid)
      vid->menu_rotation = rotation;
}

static void exynos_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   struct exynos_video *vid = (struct exynos_video*)data;

   if (!vid)
      return;

   vp->x = vp->y = 0;

   vp->width  = vp->full_width  = vid->width;
   vp->height = vp->full_height = vid->height;
}

static void exynos_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   struct exynos_video *vid = (struct exynos_video*)data;

   if (!vid)
      return;

   vid->aspect_changed = true;
}

static void exynos_apply_state_changes(void *data)
{
   (void)data;
}

static void exynos_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   const enum exynos_buffer_type buf_type = defaults[EXYNOS_IMAGE_MENU].buf_type;
   struct exynos_video *vid = data;
   struct exynos_data *pdata = vid->data;
   struct g2d_image *src = pdata->src[EXYNOS_IMAGE_MENU];
   const unsigned size = width * height * (rgb32 ? 4 : 2);

   if (exynos_realloc_buffer(pdata, buf_type, size) != 0)
      return;

   src->width = width;
   src->height = height;
   src->stride = width * (rgb32 ? 4 : 2);
   src->color_mode = rgb32 ? G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_RGBAX :
      G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_RGBAX;

   src->component_alpha = (unsigned char)(255.0f * alpha);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
   exynos_perf_memcpy(&pdata->perf, true);
#endif

   memcpy_neon(pdata->buf[buf_type]->vaddr, frame, size);

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

static void exynos_set_osd_msg(void *data, const char *msg,
      const struct font_params *params)
{
   (void)data;
   (void)msg;
   (void)params;
}

static void exynos_show_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static const video_poke_interface_t exynos_poke_interface = {
   NULL, /* get_flags */
   NULL,
   NULL,
   NULL, /* set_video_mode */
   drm_get_refresh_rate,
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
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void exynos_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &exynos_poke_interface;
}

static bool exynos_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_exynos = {
  exynos_gfx_init,
  exynos_gfx_frame,
  exynos_gfx_set_nonblock_state,
  exynos_gfx_alive,
  exynos_gfx_focus,
  exynos_gfx_suppress_screensaver,
  NULL, /* has_windowed */
  exynos_gfx_set_shader,
  exynos_gfx_free,
  "exynos",
  NULL, /* set_viewport */
  exynos_gfx_set_rotation,
  exynos_gfx_viewport_info,
  NULL, /* read_viewport */
  NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
  exynos_gfx_get_poke_interface
};
