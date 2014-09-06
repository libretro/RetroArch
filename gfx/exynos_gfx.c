/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Tobias Jakobi
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
#include <assert.h>

#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <libdrm/exynos_drmif.h>

#include <exynos/exynos_fimg2d.h>

#include "../general.h"
#include "gfx_common.h"
#include "fonts/fonts.h"

/* TODO: Honor these properties: vsync, menu rotation, menu alpha, aspect ratio change */

/* Set to '1' to enable debug logging code. */
#define EXYNOS_GFX_DEBUG_LOG 0

/* Set to '1' to enable debug perf code. */
#define EXYNOS_GFX_DEBUG_PERF 0

extern void *memcpy_neon(void *dst, const void *src, size_t n);


/* We use two GEM buffers (main and aux) to handle 'data' from the frontend. */
enum exynos_buffer_type {
  exynos_buffer_main = 0,
  exynos_buffer_aux,
  exynos_buffer_count
};

/* We have to handle three types of 'data' from the frontend, each abstracted by a *
 * G2D image object. The image objects are then backed by some storage buffer.     *
 * (1) the emulator framebuffer (backed by main buffer)                            *
 * (2) the menu buffer (backed by aux buffer)                                      *
 * (3) the font rendering buffer (backed by aux buffer)                            */
enum exynos_image_type {
  exynos_image_frame = 0,
  exynos_image_font,
  exynos_image_menu,
  exynos_image_count
};

static const struct exynos_config_default {
  unsigned width, height;
  enum exynos_buffer_type buf_type;
  unsigned g2d_color_mode;
  unsigned bpp; /* bytes per pixel */
} defaults[exynos_image_count] = {
  {1024, 640, exynos_buffer_main, G2D_COLOR_FMT_RGB565   | G2D_ORDER_AXRGB, 2}, /* frame */
  {720,  368, exynos_buffer_aux,  G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_AXRGB, 2}, /* font */
  {400,  240, exynos_buffer_aux,  G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_RGBAX, 2}  /* menu */
};


struct exynos_data;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
struct exynos_perf {
  unsigned memcpy_calls;
  unsigned g2d_calls;

  unsigned long long memcpy_time;
  unsigned long long g2d_time;

  struct timespec tspec;
};
#endif

struct exynos_page {
  struct exynos_bo *bo;
  uint32_t buf_id;

  struct exynos_data *base;

  bool used; /* Set if page is currently used. */
  bool clear; /* Set if page has to be cleared. */
};

struct exynos_fliphandler {
  struct pollfd fds;
  drmEventContext evctx;
};

struct exynos_drm {
  drmModeRes *resources;
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  drmModeModeInfo *mode;
  drmModeCrtc *orig_crtc;

  uint32_t crtc_id;
  uint32_t connector_id;
};

struct exynos_data {
  char drmname[32];
  int fd;
  struct exynos_device *device;

  struct exynos_drm *drm;
  struct exynos_fliphandler *fliphandler;

  /* G2D is used for scaling to framebuffer dimensions. */
  struct g2d_context *g2d;
  struct g2d_image *dst;
  struct g2d_image *src[exynos_image_count];

  struct exynos_bo *buf[exynos_buffer_count];

  struct exynos_page *pages;
  unsigned num_pages;

  /* currently displayed page */
  struct exynos_page *cur_page;

  unsigned pageflip_pending;

  /* framebuffer dimensions */
  unsigned width, height;

  /* framebuffer aspect ratio */
  float aspect;

  /* parameters for blitting emulator fb to screen */
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

static inline unsigned align_common(unsigned i, unsigned j) {
  return (i + j - 1) & ~(j - 1);
}

/* Find the index of a compatible DRM device. */
static int get_device_index(void) {
  char buf[32];
  drmVersionPtr ver;

  int index = 0;
  int fd;
  bool found = false;

  while (!found) {
    snprintf(buf, sizeof(buf), "/dev/dri/card%d", index);

    fd = open(buf, O_RDWR);
    if (fd == -1) break;

    ver = drmGetVersion(fd);

    if (strcmp("exynos", ver->name) == 0)
      found = true;
    else
      ++index;

    drmFreeVersion(ver);
    close(fd);
  }

  return (found ? index : -1);
}

/* Restore the original CRTC. */
static void restore_crtc(struct exynos_drm *d, int fd) {
  if (d->orig_crtc == NULL) return;

  drmModeSetCrtc(fd, d->orig_crtc->crtc_id,
                 d->orig_crtc->buffer_id,
                 d->orig_crtc->x,
                 d->orig_crtc->y,
                 &d->connector_id, 1, &d->orig_crtc->mode);

  drmModeFreeCrtc(d->orig_crtc);
  d->orig_crtc = NULL;
}

static void clean_up_drm(struct exynos_drm *d, int fd) {
  if (d->encoder) drmModeFreeEncoder(d->encoder);
  if (d->connector) drmModeFreeConnector(d->connector);
  if (d->resources) drmModeFreeResources(d->resources);

  free(d);
  close(fd);
}

/* The main pageflip handler, which the DRM executes when it flips to the page. *
 * Decreases the pending pageflip count and updates the current page.           */
static void page_flip_handler(int fd, unsigned frame, unsigned sec,
                              unsigned usec, void *data) {
  struct exynos_page *page = data;

#if (EXYNOS_GFX_DEBUG_LOG == 1)
  RARCH_LOG("video_exynos: in page_flip_handler, page = %p\n", page);
#endif

  if (page->base->cur_page != NULL) {
    page->base->cur_page->used = false;
  }

  page->base->pageflip_pending--;
  page->base->cur_page = page;
}

static void wait_flip(struct exynos_fliphandler *fh) {
  const int timeout = -1;

  fh->fds.revents = 0;

  if (poll(&fh->fds, 1, timeout) < 0)
    return;

  if (fh->fds.revents & (POLLHUP | POLLERR))
    return;

  if (fh->fds.revents & POLLIN)
    drmHandleEvent(fh->fds.fd, &fh->evctx);
}

static struct exynos_page *get_free_page(struct exynos_page *p, unsigned cnt) {
  unsigned i;

  for (i = 0; i < cnt; ++i) {
    if (!p[i].used) return &p[i];
  }

  return NULL;
}

/* Count the number of used pages. */
static unsigned pages_used(struct exynos_page *p, unsigned cnt) {
  unsigned i;
  unsigned count = 0;

  for (i = 0; i < cnt; ++i) {
    if (p[i].used) ++count;
  }

  return count;
}

static void clean_up_pages(struct exynos_page *p, unsigned cnt) {
  unsigned i;

  for (i = 0; i < cnt; ++i) {
    if (p[i].bo != NULL) {
      if (p[i].buf_id != 0)
        drmModeRmFB(p[i].buf_id, p[i].bo->handle);

      exynos_bo_destroy(p[i].bo);
    }
  }
}

#if (EXYNOS_GFX_DEBUG_LOG == 1)
static const char *buffer_name(enum exynos_buffer_type type) {
  switch (type) {
    case exynos_buffer_main:
      return "main";
    case exynos_buffer_aux:
      return "aux";
    default:
      assert(false);
      return NULL;
  }
}
#endif

/* Create a GEM buffer with userspace mapping. Buffer is cleared after creation. */
static struct exynos_bo *create_mapped_buffer(struct exynos_device *dev, unsigned size) {
  struct exynos_bo *buf;
  const unsigned flags = 0;

  buf = exynos_bo_create(dev, size, flags);
  if (buf == NULL) {
    RARCH_ERR("video_exynos: failed to create temp buffer object\n");
    return NULL;
  }

  if (exynos_bo_map(buf) == NULL) {
    RARCH_ERR("video_exynos: failed to map temp buffer object\n");
    exynos_bo_destroy(buf);
    return NULL;
  }

  memset(buf->vaddr, 0, size);

  return buf;
}

static int realloc_buffer(struct exynos_data *pdata,
                          enum exynos_buffer_type type, unsigned size) {
  struct exynos_bo *buf = pdata->buf[type];
  unsigned i;

  if (size > buf->size) {
#if (EXYNOS_GFX_DEBUG_LOG == 1)
    RARCH_LOG("video_exynos: reallocating %s buffer (%u -> %u bytes)\n",
              buffer_name(type), buf->size, size);
#endif

    exynos_bo_destroy(buf);
    buf = create_mapped_buffer(pdata->device, size);

    if (buf == NULL) {
      RARCH_ERR("video_exynos: reallocation failed\n");
      return -1;
    }

    pdata->buf[type] = buf;

    /* Map new GEM buffer to the G2D images backed by it. */
    for (i = 0; i < exynos_image_count; ++i) {
      if (defaults[i].buf_type == type)
        pdata->src[i]->bo[0] = buf->handle;
    }
  }

  return 0;
}

/* Clear a buffer associated to a G2D image by doing a (fast) solid fill. */
static int clear_buffer(struct g2d_context *g2d, struct g2d_image *img) {
  int ret;

  ret = g2d_solid_fill(g2d, img, 0, 0, img->width, img->height);

  if (ret == 0)
    ret = g2d_exec(g2d);

  if (ret != 0)
    RARCH_ERR("video_exynos: failed to clear buffer using G2D\n");

  return ret;
}

/* Put a font glyph at a position in the buffer that is backing the G2D font image object. */
static void put_glyph_rgba4444(struct exynos_data *pdata, const uint8_t *__restrict__ src,
                               uint16_t color, unsigned g_width, unsigned g_height,
                               unsigned g_pitch, unsigned dst_x, unsigned dst_y) {
  const enum exynos_image_type buf_type = defaults[exynos_image_font].buf_type;
  const unsigned buf_width = pdata->src[exynos_image_font]->width;

  unsigned x, y;
  uint16_t *__restrict__ dst = (uint16_t*)pdata->buf[buf_type]->vaddr +
                               dst_y * buf_width + dst_x;

  for (y = 0; y < g_height; ++y, src += g_pitch, dst += buf_width) {
    for (x = 0; x < g_width; ++x) {
      const uint16_t blend = src[x];

      dst[x] = color | ((blend << 8) & 0xf000);
    }
  }
}

#if (EXYNOS_GFX_DEBUG_PERF == 1)
void perf_init(struct exynos_perf *p) {
  p->memcpy_calls = 0;
  p->g2d_calls = 0;

  p->memcpy_time = 0;
  p->g2d_time = 0;

  memset(&p->tspec, 0, sizeof(struct timespec));
}

void perf_finish(struct exynos_perf *p) {
  RARCH_LOG("video_exynos: debug: total memcpy calls: %u\n", p->memcpy_calls);
  RARCH_LOG("video_exynos: debug: total g2d calls: %u\n", p->g2d_calls);

  RARCH_LOG("video_exynos: debug: total memcpy time: %f seconds\n",
            (double)p->memcpy_time / 1000000.0);
  RARCH_LOG("video_exynos: debug: total g2d time: %f seconds\n",
            (double)p->g2d_time / 1000000.0);

  RARCH_LOG("video_exynos: debug: average time per memcpy call: %f microseconds\n",
            (double)p->memcpy_time / (double)p->memcpy_calls);
  RARCH_LOG("video_exynos: debug: average time per g2d call: %f microseconds\n",
            (double)p->g2d_time / (double)p->g2d_calls);
}

void perf_memcpy(struct exynos_perf *p, bool start) {
  if (start) {
    clock_gettime(CLOCK_MONOTONIC, &p->tspec);
  } else {
    struct timespec new = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &new);

    p->memcpy_time += (new.tv_sec - p->tspec.tv_sec) * 1000000;
    p->memcpy_time += (new.tv_nsec - p->tspec.tv_nsec) / 1000;
    ++p->memcpy_calls;
  }
}

void perf_g2d(struct exynos_perf *p, bool start) {
  if (start) {
    clock_gettime(CLOCK_MONOTONIC, &p->tspec);
  } else {
    struct timespec new = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &new);

    p->g2d_time += (new.tv_sec - p->tspec.tv_sec) * 1000000;
    p->g2d_time += (new.tv_nsec - p->tspec.tv_nsec) / 1000;
    ++p->g2d_calls;
  }
}
#endif


static int exynos_g2d_init(struct exynos_data *pdata) {
  struct g2d_image *dst;
  struct g2d_context *g2d;
  unsigned i;

  g2d = g2d_init(pdata->fd);
  if (g2d == NULL) return -1;

  dst = calloc(1, sizeof(struct g2d_image));
  if (dst == NULL) goto fail;

  dst->buf_type = G2D_IMGBUF_GEM;
  dst->color_mode = (pdata->bpp == 2) ? G2D_COLOR_FMT_RGB565 | G2D_ORDER_AXRGB :
                                        G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_AXRGB;
  dst->width = pdata->width;
  dst->height = pdata->height;
  dst->stride = pdata->pitch;
  dst->color = 0xff000000; /* Clear color for solid fill operation. */

  for (i = 0; i < exynos_image_count; ++i) {
    const enum exynos_buffer_type buf_type = defaults[i].buf_type;
    const unsigned buf_size = defaults[i].width * defaults[i].height * defaults[i].bpp;

    struct g2d_image *src;

    src = calloc(1, sizeof(struct g2d_image));
    if (src == NULL) break;

    src->width = defaults[i].width;
    src->height = defaults[i].height;
    src->stride = defaults[i].width * defaults[i].bpp;

    src->color_mode = defaults[i].g2d_color_mode;

    /* Associate GEM buffer storage with G2D image. */
    src->buf_type = G2D_IMGBUF_GEM;
    src->bo[0] = pdata->buf[buf_type]->handle;

    src->repeat_mode = G2D_REPEAT_MODE_PAD; /* Pad creates no border artifacts. */

    /* Make sure that the storage buffer is large enough. If the code is working *
     * properly, then this is just a NOP. Still put it here as an insurance.     */
    realloc_buffer(pdata, buf_type, buf_size);

    pdata->src[i] = src;
  }

  if (i != exynos_image_count) {
    while (i-- > 0) {
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

static void exynos_g2d_free(struct exynos_data *pdata) {
  unsigned i;

  free(pdata->dst);

  for (i = 0; i < exynos_image_count; ++i) {
    free(pdata->src[i]);
    pdata->src[i] = NULL;
  }

  g2d_fini(pdata->g2d);
}

static int exynos_open(struct exynos_data *pdata) {
  char buf[32];
  int devidx;

  int fd = -1;
  struct exynos_drm *drm = NULL;
  struct exynos_fliphandler *fliphandler = NULL;
  unsigned i;

  pdata->fd = -1;

  devidx = get_device_index();
  if (devidx != -1) {
    snprintf(buf, sizeof(buf), "/dev/dri/card%d", devidx);
  } else {
    RARCH_ERR("video_exynos: no compatible drm device found\n");
    return -1;
  }

  fd = open(buf, O_RDWR);
  if (fd == -1) {
    RARCH_ERR("video_exynos: can't open drm device\n");
    return -1;
  }

  drm = calloc(1, sizeof(struct exynos_drm));
  if (drm == NULL) {
    RARCH_ERR("video_exynos: failed to allocate drm\n");
    close(fd);
    return -1;
  }

  drm->resources = drmModeGetResources(fd);
  if (drm->resources == NULL) {
    RARCH_ERR("video_exynos: failed to get drm resources\n");
    goto fail;
  }

  for (i = 0; i < drm->resources->count_connectors; ++i) {
    if (g_settings.video.monitor_index != 0 &&
        g_settings.video.monitor_index - 1 != i)
      continue;

    drm->connector = drmModeGetConnector(fd, drm->resources->connectors[i]);
    if (drm->connector == NULL)
      continue;
 
    if (drm->connector->connection == DRM_MODE_CONNECTED &&
        drm->connector->count_modes > 0)
      break;

    drmModeFreeConnector(drm->connector);
    drm->connector = NULL;
  }

  if (i == drm->resources->count_connectors) {
    RARCH_ERR("video_exynos: no currently active connector found\n");
    goto fail;
  }

  for (i = 0; i < drm->resources->count_encoders; i++) {
    drm->encoder = drmModeGetEncoder(fd, drm->resources->encoders[i]);
 
    if (drm->encoder == NULL) continue;
 
    if (drm->encoder->encoder_id == drm->connector->encoder_id)
      break;
 
    drmModeFreeEncoder(drm->encoder);
    drm->encoder = NULL;
  }

  fliphandler = calloc(1, sizeof(struct exynos_fliphandler));
  if (fliphandler == NULL) {
    RARCH_ERR("video_exynos: failed to allocate fliphandler\n");
    goto fail;
  }

  /* Setup the flip handler. */
  fliphandler->fds.fd = fd;
  fliphandler->fds.events = POLLIN;
  fliphandler->evctx.version = DRM_EVENT_CONTEXT_VERSION;
  fliphandler->evctx.page_flip_handler = page_flip_handler;

  strncpy(pdata->drmname, buf, sizeof(buf));
  pdata->fd = fd;

  pdata->drm = drm;
  pdata->fliphandler = fliphandler;

  RARCH_LOG("video_exynos: using DRM device \"%s\" with connector id %u\n",
            pdata->drmname, pdata->drm->connector->connector_id);

  return 0;

fail:
  free(fliphandler);
  clean_up_drm(drm, fd);

  return -1;
}

/* Counterpart to exynos_open. */
static void exynos_close(struct exynos_data *pdata) {
  free(pdata->fliphandler);
  pdata->fliphandler = NULL;

  memset(pdata->drmname, 0, sizeof(char) * 32);

  clean_up_drm(pdata->drm, pdata->fd);
  pdata->fd = -1;
  pdata->drm = NULL;
}

static int exynos_init(struct exynos_data *pdata, unsigned bpp) {
  struct exynos_drm *drm = pdata->drm;
  int fd = pdata->fd;

  unsigned i;

  if (g_settings.video.fullscreen_x != 0 &&
      g_settings.video.fullscreen_y != 0) {
    for (i = 0; i < drm->connector->count_modes; i++) {
      if (drm->connector->modes[i].hdisplay == g_settings.video.fullscreen_x &&
          drm->connector->modes[i].vdisplay == g_settings.video.fullscreen_y) {
        drm->mode = &drm->connector->modes[i];
        break;
      }
    }

    if (drm->mode == NULL) {
      RARCH_ERR("video_exynos: requested resolution (%ux%u) not available\n",
                g_settings.video.fullscreen_x, g_settings.video.fullscreen_y);
      goto fail;
    }

  } else {
    /* Select first mode, which is the native one. */
    drm->mode = &drm->connector->modes[0];
  }

  if (drm->mode->hdisplay == 0 || drm->mode->vdisplay == 0) {
    RARCH_ERR("video_exynos: failed to select sane resolution\n");
    goto fail;
  }

  drm->crtc_id = drm->encoder->crtc_id;
  drm->orig_crtc = drmModeGetCrtc(fd, drm->crtc_id);
  if (!drm->orig_crtc)
    RARCH_WARN("video_exynos: cannot find original crtc\n");

  pdata->width = drm->mode->hdisplay;
  pdata->height = drm->mode->vdisplay;

  pdata->aspect = (float)drm->mode->hdisplay / (float)drm->mode->vdisplay;

  /* Always use triple buffering to reduce chance of tearing. */
  pdata->num_pages = 3;

  pdata->bpp = bpp;
  pdata->pitch = bpp * pdata->width;
  pdata->size = pdata->pitch * pdata->height;

  RARCH_LOG("video_exynos: selected %ux%u resolution with %u bpp\n",
            pdata->width, pdata->height, pdata->bpp);

  return 0;

fail:
  restore_crtc(drm, fd);

  drm->mode = NULL;

  return -1;
}

/* Counterpart to exynos_init. */
static void exynos_deinit(struct exynos_data *pdata) {
  struct exynos_drm *drm = pdata->drm;

  restore_crtc(drm, pdata->fd);

  drm = NULL;

  pdata->width = 0;
  pdata->height = 0;

  pdata->num_pages = 0;

  pdata->bpp = 0;
  pdata->pitch = 0;
  pdata->size = 0;
}

static int exynos_alloc(struct exynos_data *pdata) {
  struct exynos_device *device;
  struct exynos_bo *bo;
  struct exynos_page *pages;
  unsigned i;
  uint32_t pixel_format;
  uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};

  const unsigned flags = 0;

  device = exynos_device_create(pdata->fd);
  if (device == NULL) {
    RARCH_ERR("video_exynos: failed to create device from fd\n");
    return -1;
  }

  pages = calloc(pdata->num_pages, sizeof(struct exynos_page));
  if (pages == NULL) {
    RARCH_ERR("video_exynos: failed to allocate pages\n");
    goto fail_alloc;
  }

  for (i = 0; i < exynos_buffer_count; ++i) {
    const unsigned buffer_size = defaults[i].width * defaults[i].height * defaults[i].bpp;

    bo = create_mapped_buffer(device, buffer_size);
    if (bo == NULL) break;

    pdata->buf[i] = bo;
  }

  if (i != exynos_buffer_count) {
    while (i-- > 0) {
      exynos_bo_destroy(pdata->buf[i]);
      pdata->buf[i] = NULL;
    }

    goto fail;
  }

  for (i = 0; i < pdata->num_pages; ++i) {
    bo = exynos_bo_create(device, pdata->size, flags);
    if (bo == NULL) {
      RARCH_ERR("video_exynos: failed to create buffer object\n");
      goto fail;
    }

    /* Don't map the BO, since we don't access it through userspace. */

    pages[i].bo = bo;
    pages[i].base = pdata;

    pages[i].used = false;
    pages[i].clear = true;
  }

  pixel_format = (pdata->bpp == 2) ? DRM_FORMAT_RGB565 : DRM_FORMAT_XRGB8888;
  pitches[0] = pdata->pitch;
  offsets[0] = 0;

  for (i = 0; i < pdata->num_pages; ++i) {
    handles[0] = pages[i].bo->handle;

    if (drmModeAddFB2(pdata->fd, pdata->width, pdata->height,
                      pixel_format, handles, pitches, offsets,
                      &pages[i].buf_id, flags)) {
      RARCH_ERR("video_exynos: failed to add bo %u to fb\n", i);
      goto fail;
    }
  }

  pdata->pages = pages;
  pdata->device = device;

  /* Setup CRTC: display the last allocated page. */
  drmModeSetCrtc(pdata->fd, pdata->drm->crtc_id, pages[pdata->num_pages - 1].buf_id,
                 0, 0, &pdata->drm->connector_id, 1, pdata->drm->mode);

  return 0;

fail:
  clean_up_pages(pages, pdata->num_pages);

fail_alloc:
  exynos_device_destroy(device);

  return -1;
}

/* Counterpart to exynos_alloc. */
static void exynos_free(struct exynos_data *pdata) {
  unsigned i;

  /* Disable the CRTC. */
  drmModeSetCrtc(pdata->fd, pdata->drm->crtc_id, 0,
                 0, 0, &pdata->drm->connector_id, 1, NULL);

  clean_up_pages(pdata->pages, pdata->num_pages);

  free(pdata->pages);
  pdata->pages = NULL;

  for (i = 0; i < exynos_buffer_count; ++i) {
    exynos_bo_destroy(pdata->buf[i]);
    pdata->buf[i] = NULL;
  }
}

#if (EXYNOS_GFX_DEBUG_LOG == 1)
static void exynos_alloc_status(struct exynos_data *pdata) {
  unsigned i;
  struct exynos_page *pages = pdata->pages;

  RARCH_LOG("video_exynos: allocated %u pages with %u bytes each (pitch = %u bytes)\n",
            pdata->num_pages, pdata->size, pdata->pitch);

  for (i = 0; i < pdata->num_pages; ++i) {
    RARCH_LOG("video_exynos: page %u: BO at %p, buffer id = %u\n",
              i, pages[i].bo, pages[i].buf_id);
  }
}
#endif

/* Find a free page, clear it if necessary, and return the page. If  *
 * no free page is available when called, wait for a page flip.      */
static struct exynos_page *exynos_free_page(struct exynos_data *pdata) {
  struct exynos_page *page = NULL;
  struct g2d_image *dst = pdata->dst;

  /* Wait until a free page is available. */
  while (page == NULL) {
    page = get_free_page(pdata->pages, pdata->num_pages);

    if (page == NULL) wait_flip(pdata->fliphandler);
  }

  dst->bo[0] = page->bo->handle;

  if (page->clear) {
    if (clear_buffer(pdata->g2d, dst) == 0)
      page->clear = false;
  }

  page->used = true;
  return page;
}

static void exynos_setup_scale(struct exynos_data *pdata, unsigned width,
                               unsigned height, unsigned src_bpp) {
  struct g2d_image *src = pdata->src[exynos_image_frame];
  unsigned i;
  unsigned w, h;

  const float aspect = (float)width / (float)height;

  src->width = width;
  src->height = height;

  src->color_mode = (src_bpp == 2) ?
                    G2D_COLOR_FMT_RGB565 | G2D_ORDER_AXRGB:
                    G2D_COLOR_FMT_XRGB8888 | G2D_ORDER_AXRGB;

  if (fabsf(pdata->aspect - aspect) < 0.0001f) {
    w = pdata->width;
    h = pdata->height;
  } else {
    if (pdata->aspect > aspect) {
      w = (float)pdata->width * aspect / pdata->aspect;
      h = pdata->height;
    } else {
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

static void exynos_set_fake_blit(struct exynos_data *pdata) {
  unsigned i;

  pdata->blit_params[0] = 0;
  pdata->blit_params[1] = 0;
  pdata->blit_params[2] = pdata->width;
  pdata->blit_params[3] = pdata->height;

  for (i = 0; i < pdata->num_pages; ++i)
    pdata->pages[i].clear = true;
}

static int exynos_blit_frame(struct exynos_data *pdata, const void *frame,
                             unsigned src_pitch) {
  const enum exynos_buffer_type buf_type = defaults[exynos_image_frame].buf_type;
  const unsigned size = src_pitch * pdata->blit_params[5];

  struct g2d_image *src = pdata->src[exynos_image_frame];

  if (realloc_buffer(pdata, buf_type, size) != 0)
    return -1;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_memcpy(&pdata->perf, true);
#endif

  /* HACK: Without IOMMU the G2D only works properly between GEM buffers. */
  memcpy_neon(pdata->buf[buf_type]->vaddr, frame, size);
  src->stride = src_pitch;

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_memcpy(&pdata->perf, false);
#endif

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_g2d(&pdata->perf, true);
#endif

  if (g2d_copy_with_scale(pdata->g2d, src, pdata->dst, 0, 0,
                          pdata->blit_params[4], pdata->blit_params[5],
                          pdata->blit_params[0], pdata->blit_params[1],
                          pdata->blit_params[2], pdata->blit_params[3], 0) ||
      g2d_exec(pdata->g2d)) {
    RARCH_ERR("video_exynos: failed to blit frame\n");
    return -1;
  }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_g2d(&pdata->perf, false);
#endif

  return 0;
}

static int exynos_blend_menu(struct exynos_data *pdata,
                             unsigned rotation) {
  struct g2d_image *src = pdata->src[exynos_image_menu];

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_g2d(&pdata->perf, true);
#endif

  if (g2d_scale_and_blend(pdata->g2d, src, pdata->dst, 0, 0,
                          src->width, src->height, pdata->blit_params[0],
                          pdata->blit_params[1], pdata->blit_params[2],
                          pdata->blit_params[3], G2D_OP_INTERPOLATE) ||
      g2d_exec(pdata->g2d)) {
    RARCH_ERR("video_exynos: failed to blend menu\n");
    return -1;
  }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_g2d(&pdata->perf, false);
#endif

  return 0;
}

static int exynos_blend_font(struct exynos_data *pdata) {
  struct g2d_image *src = pdata->src[exynos_image_font];

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_g2d(&pdata->perf, true);
#endif

  if (g2d_scale_and_blend(pdata->g2d, src, pdata->dst, 0, 0, src->width,
                          src->height, 0, 0, pdata->width, pdata->height,
                          G2D_OP_INTERPOLATE) ||
      g2d_exec(pdata->g2d)) {
    RARCH_ERR("video_exynos: failed to blend font\n");
    return -1;
  }

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_g2d(&pdata->perf, false);
#endif

  return 0;
}

static int exynos_flip(struct exynos_data *pdata, struct exynos_page *page) {
  /* We don't queue multiple page flips. */
  if (pdata->pageflip_pending > 0) {
    wait_flip(pdata->fliphandler);
  }

  /* Issue a page flip at the next vblank interval. */
  if (drmModePageFlip(pdata->fd, pdata->drm->crtc_id, page->buf_id,
                      DRM_MODE_PAGE_FLIP_EVENT, page) != 0) {
    RARCH_ERR("video_exynos: failed to issue page flip\n");
    return -1;
  } else {
    pdata->pageflip_pending++;
  }

  /* On startup no frame is displayed. We therefore wait for the initial flip to finish. */
  if (pdata->cur_page == NULL) wait_flip(pdata->fliphandler);

  return 0;
}


struct exynos_video {
  struct exynos_data *data;

  void *font;
  const font_renderer_driver_t *font_driver;
  uint16_t font_color; /* ARGB4444 */

  unsigned bytes_per_pixel;

  /* current dimensions of the emulator fb */
  unsigned width;
  unsigned height;

  /* menu data */
  unsigned menu_rotation;
  bool menu_active;

  bool aspect_changed;
};


static int exynos_init_font(struct exynos_video *vid) {
  struct exynos_data *pdata = vid->data;
  struct g2d_image *src = pdata->src[exynos_image_font];

  const unsigned buf_height = defaults[exynos_image_font].height;
  const unsigned buf_width = align_common(pdata->aspect * (float)buf_height, 16);
  const unsigned buf_bpp = defaults[exynos_image_font].bpp;

  if (!g_settings.video.font_enable) return 0;

  if (font_renderer_create_default(&vid->font_driver, &vid->font,
      *g_settings.video.font_path ? g_settings.video.font_path : NULL,
      g_settings.video.font_size)) {
    const int r = g_settings.video.msg_color_r * 15;
    const int g = g_settings.video.msg_color_g * 15;
    const int b = g_settings.video.msg_color_b * 15;

    vid->font_color = ((b < 0 ? 0 : (b > 15 ? 15 : b)) << 0) |
                      ((g < 0 ? 0 : (g > 15 ? 15 : g)) << 4) |
                      ((r < 0 ? 0 : (r > 15 ? 15 : r)) << 8);
  } else {
    RARCH_ERR("video_exynos: creating font renderer failed\n");
    return -1;
  }

  /* The font buffer color type is ARGB4444. */
  if (realloc_buffer(pdata, defaults[exynos_image_font].buf_type,
                     buf_width * buf_height * buf_bpp) != 0) {
    vid->font_driver->free(vid->font);
    return -1;
  }

  src->width = buf_width;
  src->height = buf_height;
  src->stride = buf_width * buf_bpp;

#if (EXYNOS_GFX_DEBUG_LOG == 1)
  RARCH_LOG("video_exynos: using font rendering image with size %ux%u\n",
            buf_width, buf_height);
#endif

  return 0;
}

static int exynos_render_msg(struct exynos_video *vid,
                             const char *msg) {
  struct exynos_data *pdata = vid->data;
  struct g2d_image *dst = pdata->src[exynos_image_font];

  const struct font_atlas *atlas;

  int msg_base_x = g_settings.video.msg_pos_x * dst->width;
  int msg_base_y = (1.0f - g_settings.video.msg_pos_y) * dst->height;

  if (vid->font == NULL || vid->font_driver == NULL)
    return -1;

  if (clear_buffer(pdata->g2d, dst) != 0)
    return -1;

  atlas = vid->font_driver->get_atlas(vid->font);

  for (; *msg; ++msg) {
    const struct font_glyph *glyph = vid->font_driver->get_glyph(vid->font, (uint8_t)*msg);
    if (glyph == NULL)
      continue;

    int base_x = msg_base_x + glyph->draw_offset_x;
    int base_y = msg_base_y + glyph->draw_offset_y;

    const int max_width  = dst->width - base_x;
    const int max_height = dst->height - base_y;

    int glyph_width  = glyph->width;
    int glyph_height = glyph->height;

    const uint8_t *src = atlas->buffer + glyph->atlas_offset_x + glyph->atlas_offset_y * atlas->width;

    if (base_x < 0) {
       src -= base_x;
       glyph_width += base_x;
       base_x = 0;
    }

    if (base_y < 0) {
       src -= base_y * (int)atlas->width;
       glyph_height += base_y;
       base_y = 0;
    }

    if (max_width <= 0 || max_height <= 0) continue;

    if (glyph_width > max_width) glyph_width = max_width;
    if (glyph_height > max_height) glyph_height = max_height;

    put_glyph_rgba4444(pdata, src, vid->font_color,
                       glyph_width, glyph_height,
                       atlas->width, base_x, base_y);

    msg_base_x += glyph->advance_x;
    msg_base_y += glyph->advance_y;
  }

  return exynos_blend_font(pdata);
}


static void *exynos_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data) {
  struct exynos_video *vid;

  const unsigned fb_bpp = 4; /* Use XRGB8888 framebuffer. */

  vid = calloc(1, sizeof(struct exynos_video));
  if (!vid) return NULL;

  vid->data = calloc(1, sizeof(struct exynos_data));
  if (!vid->data) goto fail_data;

  vid->bytes_per_pixel = video->rgb32 ? 4 : 2;

  if (exynos_open(vid->data) != 0) {
    RARCH_ERR("video_exynos: opening device failed\n");
    goto fail;
  }

  if (exynos_init(vid->data, fb_bpp) != 0) {
    RARCH_ERR("video_exynos: initialization failed\n");
    goto fail_init;
  }

  if (exynos_alloc(vid->data) != 0) {
    RARCH_ERR("video_exynos: allocation failed\n");
    goto fail_alloc;
  }

  if (exynos_g2d_init(vid->data) != 0) {
    RARCH_ERR("video_exynos: G2D initialization failed\n");
    goto fail_g2d;
  }

#if (EXYNOS_GFX_DEBUG_LOG == 1)
  exynos_alloc_status(vid->data);
#endif

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_init(&vid->data->perf);
#endif

  if (input && input_data) {
    *input = NULL;
  }

  if (exynos_init_font(vid) != 0) {
    RARCH_ERR("video_exynos: font initialization failed\n");
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

static void exynos_gfx_free(void *data) {
  struct exynos_video *vid = data;
  struct exynos_data *pdata;

  if (!vid) return;

  pdata = vid->data;

  exynos_g2d_free(pdata);

  /* Flush pages: One page remains, the one being displayed at this moment. */
  while (pages_used(pdata->pages, pdata->num_pages) > 1) {
    wait_flip(pdata->fliphandler);
  }

  exynos_free(pdata);
  exynos_deinit(pdata);
  exynos_close(pdata);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_finish(&pdata->perf);
#endif

  free(pdata);

  if (vid->font != NULL && vid->font_driver != NULL)
    vid->font_driver->free(vid->font);

  free(vid);
}

static bool exynos_gfx_frame(void *data, const void *frame, unsigned width,
                             unsigned height, unsigned pitch, const char *msg) {
  struct exynos_video *vid = data;
  struct exynos_page *page = NULL;

  /* Check if neither menu nor emulator framebuffer is to be displayed. */
  if (!vid->menu_active && frame == NULL) return true;

  if (frame != NULL) {
    if (width != vid->width || height != vid->height) {
      /* Sanity check on new dimension parameters. */
      if (width == 0 || height == 0) return true;

      RARCH_LOG("video_exynos: resolution changed by core: %ux%u -> %ux%u\n",
                vid->width, vid->height, width, height);
      exynos_setup_scale(vid->data, width, height, vid->bytes_per_pixel);

      vid->width = width;
      vid->height = height;
    }

    page = exynos_free_page(vid->data);

    if (exynos_blit_frame(vid->data, frame, pitch) != 0)
      goto fail;
  }

  if (g_settings.fps_show) {
    char buffer[128], buffer_fps[128];

    gfx_get_fps(buffer, sizeof(buffer), g_settings.fps_show ? buffer_fps : NULL, sizeof(buffer_fps));
    msg_queue_push(g_extern.msg_queue, buffer_fps, 1, 1);
  }

  if (vid->width == 0 || vid->height == 0) {
    /* If at this point the dimension parameters are still zero, setup some  *
     * fake blit parameters so that menu and font rendering work properly.   */
    exynos_set_fake_blit(vid->data);
  }

  if (page == NULL)
    page = exynos_free_page(vid->data);

  if (vid->menu_active) {
    if (exynos_blend_menu(vid->data, vid->menu_rotation) != 0)
      goto fail;
  }

  if (msg) {
    if (exynos_render_msg(vid, msg) != 0) goto fail;

    /* Font is blitted to the entire screen, so issue clear afterwards. */
    page->clear = true;
  }

  if (exynos_flip(vid->data, page) != 0) goto fail;

  g_extern.frame_count++;

  return true;

fail:
  /* Since we didn't manage to issue a pageflip to this page, set *
   * it to 'unused' again, and hope that it works next time.      */
  page->used = false;

  return false;
}

static void exynos_gfx_set_nonblock_state(void *data, bool state) {
  struct exynos_video *vid = data;

  vid->data->sync = !state;
}

static bool exynos_gfx_alive(void *data) {
  (void)data;
  return true; /* always alive */
}

static bool exynos_gfx_focus(void *data) {
  (void)data;
  return true; /* drm device always has focus */
}

static void exynos_gfx_set_rotation(void *data, unsigned rotation) {
  struct exynos_video *vid = data;

  vid->menu_rotation = rotation;
}

static void exynos_gfx_viewport_info(void *data, struct rarch_viewport *vp) {
  struct exynos_video *vid = data;

  vp->x = vp->y = 0;

  vp->width  = vp->full_width  = vid->width;
  vp->height = vp->full_height = vid->height;
}

static void exynos_set_aspect_ratio(void *data, unsigned aspect_ratio_idx) {
  struct exynos_video *vid = data;

  switch (aspect_ratio_idx) {
    case ASPECT_RATIO_SQUARE:
      gfx_set_square_pixel_viewport(g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);
    break;

    case ASPECT_RATIO_CORE:
      gfx_set_core_viewport();
    break;

    case ASPECT_RATIO_CONFIG:
      gfx_set_config_viewport();
    break;

    default:
    break;
  }

  g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
  vid->aspect_changed = true;
}

static void exynos_apply_state_changes(void *data) {
  (void)data;
}

static void exynos_set_texture_frame(void *data, const void *frame, bool rgb32,
                                     unsigned width, unsigned height, float alpha) {
  const enum exynos_buffer_type buf_type = defaults[exynos_image_menu].buf_type;

  struct exynos_video *vid = data;
  struct exynos_data *pdata = vid->data;
  struct g2d_image *src = pdata->src[exynos_image_menu];

  const unsigned size = width * height * (rgb32 ? 4 : 2);

  if (realloc_buffer(pdata, buf_type, size) != 0)
    return;

  src->width = width;
  src->height = height;
  src->stride = width * (rgb32 ? 4 : 2);
  src->color_mode = rgb32 ? G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_RGBAX :
                            G2D_COLOR_FMT_ARGB4444 | G2D_ORDER_RGBAX;

  src->component_alpha = (unsigned char)(255.0f * alpha);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_memcpy(&pdata->perf, true);
#endif

  memcpy_neon(pdata->buf[buf_type]->vaddr, frame, size);

#if (EXYNOS_GFX_DEBUG_PERF == 1)
  perf_memcpy(&pdata->perf, false);
#endif
}

static void exynos_set_texture_enable(void *data, bool state, bool full_screen) {
  struct exynos_video *vid = data;
  vid->menu_active = state;
}

static void exynos_set_osd_msg(void *data, const char *msg, const struct font_params *params) {
  struct exynos_video *vid = data;

  /* TODO: what does this do? */
  (void)msg;
  (void)params;
}

static void exynos_show_mouse(void *data, bool state) {
  (void)data;
}

static const video_poke_interface_t exynos_poke_interface = {
  NULL, /* set_filtering */
#ifdef HAVE_FBO
  NULL, /* get_current_framebuffer */
  NULL, /* get_proc_address */
#endif
  exynos_set_aspect_ratio,
  exynos_apply_state_changes,
#ifdef HAVE_MENU
  exynos_set_texture_frame,
  exynos_set_texture_enable,
#endif
  exynos_set_osd_msg,
  exynos_show_mouse
};

static void exynos_gfx_get_poke_interface(void *data, const video_poke_interface_t **iface) {
  (void)data;
  *iface = &exynos_poke_interface;
}

const video_driver_t video_exynos = {
  exynos_gfx_init,
  exynos_gfx_frame,
  exynos_gfx_set_nonblock_state,
  exynos_gfx_alive,
  exynos_gfx_focus,
  NULL, /* set_shader */
  exynos_gfx_free,
  "exynos",
  exynos_gfx_set_rotation,
  exynos_gfx_viewport_info,
  NULL, /* read_viewport */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  exynos_gfx_get_poke_interface
};
