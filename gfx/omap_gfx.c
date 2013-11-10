/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2013      - Tobias Jakobi
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

#include "../driver.h"
#include <stdlib.h>
#include <string.h>
#include "../general.h"
#include "scaler/scaler.h"
#include "gfx_common.h"
#include "gfx_context.h"
#include "fonts/fonts.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include <sys/mman.h>
#include <linux/omapfb.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

typedef struct omapfb_page {
  unsigned yoffset;
  void *buf;
  bool used;
} omapfb_page_t;

typedef struct omapfb_state {
  struct omapfb_plane_info pi;
  struct omapfb_mem_info mi;
  struct fb_var_screeninfo si;
  void* mem;
} omapfb_state_t;

typedef struct omapfb_data {
  const char* fbname;
  int fd;

  void *fb_mem;
  unsigned fb_framesize;

  omapfb_page_t *pages;
  int num_pages;
  omapfb_page_t *cur_page;
  omapfb_page_t *old_page;

  /* current and saved (for later restore) states */
  omapfb_state_t* current_state;
  omapfb_state_t* saved_state;

  /* native screen size */
  unsigned nat_w, nat_h;

  /* bytes per pixel */
  unsigned bpp;
} omapfb_data_t;


static const char *get_fb_device(void) {
  const int fbidx = g_settings.video.monitor_index;
  static char fbname[12];

  if (fbidx == 0) return "/dev/fb0";

  snprintf(fbname, sizeof(fbname), "/dev/fb%d", fbidx - 1);
  RARCH_LOG("video_omap: Using %s as framebuffer device\n", fbname);
  return fbname;
}

static omapfb_page_t *get_page(omapfb_data_t *pdata) {
  omapfb_page_t *page = NULL;
  unsigned i;

  for (i = 0; i < pdata->num_pages; ++i) {
    if (&pdata->pages[i] == pdata->cur_page)
      continue;
    if (&pdata->pages[i] == pdata->old_page)
      continue;
    if (!pdata->pages[i].used) {
      RARCH_LOG("video_omap: page %u is free\n", i);
      page = &pdata->pages[i];
      break;
    }
  }

  return page;
}

static void page_flip(omapfb_data_t *pdata) {
  ioctl(pdata->fd, OMAPFB_WAITFORGO);

  /* TODO: should we use the manual update feature of the OMAP here? */

  pdata->current_state->si.yoffset = pdata->cur_page->yoffset;
  ioctl(pdata->fd, FBIOPAN_DISPLAY, &pdata->current_state->si);

  if (pdata->old_page)
    pdata->old_page->used = false;
}

static int read_sysfs(const char *fname, char *buff, size_t size) {
  FILE *f;
  int ret;

  f = fopen(fname, "r");
  if (f == NULL) return -1;

  ret = fread(buff, 1, size - 1, f);
  fclose(f);
  if (ret <= 0) return -1;

  buff[ret] = 0;
  for (ret--; ret >= 0 && isspace(buff[ret]); ret--)
    buff[ret] = 0;

  return 0;
}

static int omapfb_detect_screen(omapfb_data_t *pdata) {
  int fb_id, overlay_id = -1, display_id = -1;
  char buff[64], manager_name[64], display_name[64];
  struct stat status;
  int fd, i, ret;
  int w, h;
  FILE *f;

  /* Find out the native screen resolution, which is needed to 
   * properly center the scaled image data. */
  ret = stat(pdata->fbname, &status);
  if (ret != 0) {
    RARCH_ERR("video_omap: can't stat %s\n", pdata->fbname);
    return -1;
  }
  fb_id = minor(status.st_rdev);

  snprintf(buff, sizeof(buff), "/sys/class/graphics/fb%d/overlays", fb_id);
  f = fopen(buff, "r");
  if (f == NULL) {
    RARCH_ERR("video_omap: can't open %s\n", buff);
    return -1;
  }

  ret = fscanf(f, "%d", &overlay_id);
  fclose(f);
  if (ret != 1) {
    RARCH_ERR("video_omap: can't parse %s\n", buff);
    return -1;
  }

  snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/overlay%d/manager", overlay_id);
  ret = read_sysfs(buff, manager_name, sizeof(manager_name));
  if (ret < 0) {
    RARCH_ERR("video_omap: can't read manager name\n");
    return -1;
  }

  for (i = 0; ; i++) {
    snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/manager%d/name", i);
    ret = read_sysfs(buff, buff, sizeof(buff));
    if (ret < 0) break;

    if (strcmp(manager_name, buff) == 0) {
      snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/manager%d/display", i);
      ret = read_sysfs(buff, display_name, sizeof(display_name));

      if (ret < 0) {
         RARCH_ERR("video_omap: can't read display name\n");
         return -1;
      }

      break;
    }
  }

  if (ret < 0) {
    RARCH_ERR("video_omap: couldn't find manager\n");
    return -1;
  }

  for (i = 0; ; i++) {
    snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/display%d/name", i);
    ret = read_sysfs(buff, buff, sizeof(buff));
    if (ret < 0) break;

    if (strcmp(display_name, buff) == 0) {
      display_id = i;
      break;
    }
  }

  if (display_id < 0) {
    RARCH_ERR("video_omap: couldn't find display\n");
    return -1;
  }

  snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/display%d/timings", display_id);
  f = fopen(buff, "r");
  if (f == NULL) {
    RARCH_ERR("video_omap: can't open %s\n", buff);
    return -1;
  }

  ret = fscanf(f, "%*d,%d/%*d/%*d/%*d,%d/%*d/%*d/%*d", &w, &h);
  fclose(f);
  if (ret != 2) {
    RARCH_ERR("video_omap: can't parse %s (%d)\n", buff, ret);
    return -1;
  }

  if (w <= 0 || h <= 0) {
    RARCH_ERR("video_omap: unsane dimensions detected (%dx%d)\n", w, h);
    return -1;
  }

  RARCH_LOG("video_omap: detected %dx%d '%s' (%d) display attached to fb %d and overlay %d\n",
            w, h, display_name, display_id, fb_id, overlay_id);

  pdata->nat_w = w;
  pdata->nat_h = h;

  return 0;
}

static int omapfb_setup_pages(omapfb_data_t *pdata) {
  int i;

  if (pdata->pages == NULL) {
    pdata->pages = calloc(pdata->num_pages, sizeof(omapfb_page_t));

    if (pdata->pages == NULL) {
      RARCH_ERR("video_omap: pages allocation failed\n");
      return -1;
    }
  }

  for (i = 0; i < pdata->num_pages; ++i) {
    pdata->pages[i].yoffset = i * pdata->current_state->si.yres;
    pdata->pages[i].buf = pdata->fb_mem + (i * pdata->fb_framesize);
    pdata->pages[i].used = false;
  }

  pdata->old_page = NULL;
  pdata->cur_page = &pdata->pages[0];

  memset(pdata->cur_page->buf, 0, pdata->fb_framesize);

  page_flip(pdata);
  pdata->cur_page->used = true;

  return 0;
}

static int omapfb_mmap(omapfb_data_t *pdata) {
  assert(pdata->fb_mem == NULL);

  pdata->fb_mem = mmap(NULL, pdata->current_state->mi.size, PROT_WRITE,
                       MAP_SHARED, pdata->fd, 0);

  if (pdata->fb_mem == MAP_FAILED) {
    pdata->fb_mem = NULL;
    RARCH_ERR("video_omap: framebuffer mmap failed\n");

    return -1;
  }

  return 0;
}

static int omapfb_backup_state(omapfb_data_t *pdata) {
  struct omapfb_plane_info pi;
  struct omapfb_mem_info mi;
  void* mem;

  assert(pdata->saved_state == NULL);

  pdata->saved_state = calloc(1, sizeof(omapfb_state_t));
  if (!pdata->saved_state) return -1;

  if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pdata->saved_state->pi) != 0) {
    RARCH_ERR("video_omap: backup layer (plane) failed\n");
    return -1;
  }

  if (ioctl(pdata->fd, OMAPFB_QUERY_MEM, &pdata->saved_state->mi) != 0) {
    RARCH_ERR("video_omap: backup layer (mem) failed\n");
    return -1;
  }

  if (ioctl(pdata->fd, FBIOGET_VSCREENINFO, &pdata->saved_state->si) != 0) {
    RARCH_ERR("video_omap: backup layer (screeninfo) failed\n");
    return -1;
  }

  pdata->saved_state->mem = malloc(pdata->saved_state->mi.size);
  mem = mmap(NULL, pdata->saved_state->mi.size, PROT_WRITE|PROT_READ,
             MAP_SHARED, pdata->fd, 0);
  if (pdata->saved_state->mem == NULL || mem == MAP_FAILED) {
    RARCH_ERR("video_omap: backup layer (mem backup) failed\n");
    munmap(mem, pdata->saved_state->mi.size);
    return -1;
  }
  memcpy(pdata->saved_state->mem, mem, pdata->saved_state->mi.size);
  munmap(mem, pdata->saved_state->mi.size);

  return 0;
}

static int omapfb_alloc_mem(omapfb_data_t *pdata) {
  struct omapfb_plane_info pi;
  struct omapfb_mem_info mi;
  const struct retro_game_geometry *geom;
  unsigned mem_size;
  void* mem;

  assert(pdata->current_state == NULL);

  pdata->current_state = calloc(1, sizeof(omapfb_state_t));
  if (!pdata->current_state) return -1;

  if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pi) != 0) {
    RARCH_ERR("video_omap: alloc mem (query plane) failed\n");
    return -1;
  }

  if (ioctl(pdata->fd, OMAPFB_QUERY_MEM, &mi) != 0) {
    RARCH_ERR("video_omap: alloc mem (query mem) failed\n");
    return -1;
  }

  /* disable plane when changing memory allocation */
  if (pi.enabled) {
    pi.enabled = 0;
    if (ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pi) != 0) {
      RARCH_ERR("video_omap: alloc mem (disable plane) failed\n");
      return -1;
    }
  }

  geom = &g_extern.system.av_info.geometry;
  mem_size = geom->max_width * geom->max_height *
             pdata->bpp * pdata->num_pages;

  mi.size = mem_size;

  if (ioctl(pdata->fd, OMAPFB_SETUP_MEM, &mi) != 0) {
    RARCH_ERR("video_omap: allocation of %d bytes of VRAM failed\n", mem_size);
    return -1;
  }

  mem = mmap(NULL, mi.size, PROT_WRITE|PROT_READ, MAP_SHARED, pdata->fd, 0);
  if (mem == MAP_FAILED) {
    RARCH_ERR("video_omap: zeroing framebuffer failed\n");
    return -1;
  }
  memset(mem, 0, mi.size);
  munmap(mem, mi.size);

  pdata->current_state->mi = mi;

  /* Don't re-enable the plane here (setup not yet complete) */

  return 0;
}

static int omapfb_setup_screeninfo(omapfb_data_t *pdata, int width, int height) {
  omapfb_state_t* state = pdata->current_state;

  state->si.xres = width;
  state->si.yres = height;
  state->si.xres_virtual = width;
  state->si.yres_virtual = height * pdata->num_pages;

  state->si.xoffset = 0;
  state->si.yoffset = 0;

  state->si.bits_per_pixel = pdata->bpp * 8;

  /* OMAPFB_COLOR_ARGB32 for bpp=4, OMAPFB_COLOR_RGB565 for bpp=2 */
  state->si.nonstd = 0;

  if (ioctl(pdata->fd, FBIOPUT_VSCREENINFO, &state->si) != 0) {
    RARCH_ERR("video_omap: setup screeninfo failed\n");
    return -1;
  }

  pdata->fb_framesize = width * height * pdata->bpp;

  return 0;
}

static float omapfb_scaling(omapfb_data_t *pdata, int width, int height) {
  const float w_factor = (float)pdata->nat_w / (float)width;
  const float h_factor = (float)pdata->nat_h / (float)height;

  return (w_factor < h_factor ? w_factor : h_factor);
}

static int omapfb_setup_plane(omapfb_data_t *pdata, int width, int height) {
  struct omapfb_plane_info pi = {0};
  int x, y, w, h;
  float scale;

  scale = omapfb_scaling(pdata, width, height);
  w = (int)(scale * width);
  h = (int)(scale * height);

  RARCH_LOG("omap_video: scaling %dx%d to %dx%d\n", width, height, w, h);

  x = pdata->nat_w / 2 - w / 2;
  y = pdata->nat_h / 2 - h / 2;

  if (width * height * pdata->bpp * pdata->num_pages > pdata->current_state->mi.size) {
    RARCH_ERR("omap_video: fb dimensions too large for allocated buffer\n");
    return -1;
  }

  if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pi) != 0) {
    RARCH_ERR("video_omap: setup plane (query) failed\n");
    return -1;
  }

  pi.pos_x = x;
  pi.pos_y = y;
  pi.out_width = w;
  pi.out_height = h;
  pi.enabled = 0; /* TODO: do we need to disable the plane for setup? */

  if (ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pi) != 0) {
    RARCH_ERR("video_omap: setup plane (param = %d %d %d %d) failed\n", x, y, w, h);
    return -1;
  }

  pdata->current_state->pi = pi;

  return 0;
}

static int omapfb_enable_plane(omapfb_data_t *pdata) {
  struct omapfb_plane_info pi = {0};

  if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pi) != 0) {
    RARCH_ERR("video_omap: enable plane (query) failed\n");
    return -1;
  }

  pi.enabled = 1;

  if (ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pi) != 0) {
    RARCH_ERR("video_omap: enable plane failed\n");
    return -1;
  }

  return 0;
}

static int omapfb_init(omapfb_data_t *pdata, unsigned bpp) {
  const char *fbname;
  int fd;

  fbname = get_fb_device();

  fd = open(fbname, O_RDWR);
  if (fd == -1) {
    RARCH_ERR("video_omap: can't open framebuffer device\n");
    return -1;
  }

  pdata->fbname = fbname;
  pdata->fd = fd;

  if (omapfb_detect_screen(pdata)) {
    close(fd);

    pdata->fbname = NULL;
    pdata->fd = -1;

    return -1;
  }

  /* always use triple buffering to reduce chance of tearing */
  pdata->bpp = bpp;
  pdata->num_pages = 3;

  return 0;
}

void omapfb_free(omapfb_data_t *pdata) {
  /* unmap the framebuffer memory */
  if (pdata->fb_mem != NULL) {
    munmap(pdata->fb_mem, pdata->current_state->mi.size);
    pdata->fb_mem = NULL;
  }

  /* restore the framebuffer state (OMAP plane state, screen info) */
  if (pdata->saved_state != NULL) {
    int enabled;
    void *mem;

    enabled = pdata->saved_state->pi.enabled;

    /* be sure to disable while setting up */
    pdata->saved_state->pi.enabled = 0;
    ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pdata->saved_state->pi);
    ioctl(pdata->fd, OMAPFB_SETUP_MEM, &pdata->saved_state->mi);
    if (enabled) {
      pdata->saved_state->pi.enabled = enabled;
      ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pdata->saved_state->pi);
    }

    /* restore framebuffer content */
    mem = mmap(0, pdata->saved_state->mi.size, PROT_WRITE|PROT_READ,
               MAP_SHARED, pdata->fd, 0);
    if (mem != MAP_FAILED) {
      memcpy(mem, pdata->saved_state->mem, pdata->saved_state->mi.size);
      munmap(mem, pdata->saved_state->mi.size);
    }

    /* restore screen info */
    ioctl(pdata->fd, FBIOPUT_VSCREENINFO, &pdata->saved_state->si);

    free(pdata->saved_state->mem);
    pdata->saved_state->mem = NULL;

    free(pdata->saved_state);
    pdata->saved_state = NULL;
  }

  free(pdata->current_state);
  pdata->current_state = NULL;

  close(pdata->fd);
  pdata->fd = -1;
}

static int omapfb_set_mode(omapfb_data_t *pdata, int width, int height) {
  ioctl(pdata->fd, OMAPFB_WAITFORGO);

  if (omapfb_setup_plane(pdata, width, height) != 0)
    return -1;

  if (omapfb_setup_screeninfo(pdata, width, height) != 0 ||
      omapfb_setup_pages(pdata) != 0 ||
      omapfb_enable_plane(pdata) != 0) {
    return -1;
  }

  return 0;
}

static void omapfb_prepare(omapfb_data_t *pdata) {
  omapfb_page_t *page;

  /* issue flip before getting free page */
  page_flip(pdata);
  page = get_page(pdata);
  assert(page != NULL);

  pdata->old_page = pdata->cur_page;
  pdata->cur_page = page;

  pdata->cur_page->used = true;
}

static void omapfb_blit_frame(omapfb_data_t *pdata, const void *src,
                              unsigned height, unsigned src_pitch) {
  unsigned i, dst_pitch;
  void *dst;

  dst = pdata->cur_page->buf;
  dst_pitch = pdata->current_state->si.xres * pdata->bpp;

  for (i = 0; i < height; i++) {
    memcpy(dst + dst_pitch * i, src + src_pitch * i, dst_pitch);
  }
}


typedef struct omap_video {
  omapfb_data_t *omap;

  void *font;
  const font_renderer_driver_t *font_driver;
  uint8_t font_r;
  uint8_t font_g;
  uint8_t font_b;

  unsigned bytes_per_pixel;

  /* current dimensions */
  unsigned width;
  unsigned height;
} omap_video_t;


static void omap_gfx_free(void *data) {
  omap_video_t *vid = data;
  if (!vid) return;

  omapfb_free(vid->omap);

  if (vid->font) vid->font_driver->free(vid->font);

  free(vid);
}

static void omap_init_font(omap_video_t *vid, const char *font_path, unsigned font_size) {
  if (!g_settings.video.font_enable) return;

  if (font_renderer_create_default(&vid->font_driver, &vid->font)) {
    int r = g_settings.video.msg_color_r * 255;
    int g = g_settings.video.msg_color_g * 255;
    int b = g_settings.video.msg_color_b * 255;

    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);

    vid->font_r = r;
    vid->font_g = g;
    vid->font_b = b;
  } else {
    RARCH_LOG("video_omap: font init failed\n");
  }
}

static void omap_render_msg(omap_video_t *vid, const char *msg) {
  if (!vid->font) return;

  struct font_output_list out;
  vid->font_driver->render_msg(vid->font, msg, &out);
  struct font_output *head = out.head;

  return; /* TODO: implement */

  /*int msg_base_x = g_settings.video.msg_pos_x * width;
  int msg_base_y = (1.0 - g_settings.video.msg_pos_y) * height;

  unsigned rshift = fmt->Rshift;
  unsigned gshift = fmt->Gshift;
  unsigned bshift = fmt->Bshift;

  for (; head; head = head->next) {
    int base_x = msg_base_x + head->off_x;
    int base_y = msg_base_y - head->off_y - head->height;

    int glyph_width  = head->width;
    int glyph_height = head->height;

    const uint8_t *src = head->output;

    if (base_x < 0) {
       src -= base_x;
       glyph_width += base_x;
       base_x = 0;
    }

    if (base_y < 0) {
       src -= base_y * (int)head->pitch;
       glyph_height += base_y;
       base_y = 0;
    }

    int max_width  = width - base_x;
    int max_height = height - base_y;

    if (max_width <= 0 || max_height <= 0)
      continue;

    if (glyph_width > max_width)
      glyph_width = max_width;
    if (glyph_height > max_height)
      glyph_height = max_height;

    uint32_t *out = (uint32_t*)buffer->pixels + base_y * (buffer->pitch >> 2) + base_x;

    for (int y = 0; y < glyph_height; y++, src += head->pitch, out += buffer->pitch >> 2) {
      for (int x = 0; x < glyph_width; x++) {
        unsigned blend = src[x];
        unsigned out_pix = out[x];
        unsigned r = (out_pix >> rshift) & 0xff;
        unsigned g = (out_pix >> gshift) & 0xff;
        unsigned b = (out_pix >> bshift) & 0xff;

        unsigned out_r = (r * (256 - blend) + vid->font_r * blend) >> 8;
        unsigned out_g = (g * (256 - blend) + vid->font_g * blend) >> 8;
        unsigned out_b = (b * (256 - blend) + vid->font_b * blend) >> 8;
        out[x] = (out_r << rshift) | (out_g << gshift) | (out_b << bshift);
      }
    }
  }*/

  vid->font_driver->free_output(vid->font, &out);
}

static void *omap_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data) {
  omap_video_t *vid = NULL;

  /* Don't support filters at the moment since they make estimations  *
   * on the maximum used resolution difficult.                        */
  if (g_extern.filter.active) {
    RARCH_ERR("video_omap: filters are not supported\n");
    return NULL;
  }

  vid = calloc(1, sizeof(omap_video_t));
  if (!vid) return NULL;

  vid->omap = calloc(1, sizeof(omapfb_data_t));
  if (!vid->omap) return NULL;

  vid->bytes_per_pixel = video->rgb32 ? 4 : 2;

  if (omapfb_init(vid->omap, vid->bytes_per_pixel) != 0) {
    goto fail;
  }

  if (omapfb_backup_state(vid->omap) != 0 ||
      omapfb_alloc_mem(vid->omap) != 0 ||
      omapfb_mmap(vid->omap) != 0) goto fail;

  if (input && input_data) {
    *input = NULL;
  }

  omap_init_font(vid, g_settings.video.font_path, g_settings.video.font_size);

  return vid;

fail:
  RARCH_ERR("video_omap: initialization failed\n");
  omap_gfx_free(vid);
  return NULL;
}

static bool omap_gfx_frame(void *data, const void *frame, unsigned width,
                           unsigned height, unsigned pitch, const char *msg) {
  omap_video_t *vid;

  if (!frame) return true;
  vid = data;

  if (width != vid->width || height != vid->height) {
    if (width == 0 || height == 0) return true;

    RARCH_LOG("video_omap: mode set (resolution changed by core)\n");

    if (omapfb_set_mode(vid->omap, width, height) != 0) {
      RARCH_ERR("video_omap: mode set failed\n");
      return false;
    }

    vid->width = width;
    vid->height = height;
  }

  omapfb_prepare(vid->omap);
  omapfb_blit_frame(vid->omap, frame, vid->height, pitch);
  if (msg) omap_render_msg(vid, msg);

  g_extern.frame_count++;

  return true;
}

static void omap_gfx_set_nonblock_state(void *data, bool state) {
  /* TODO: add sync flag to omapfb_data and only WAITFORGO when enabled */

  (void)data;
  (void)state;
}

static bool omap_gfx_alive(void *data) {
  (void)data;
  return true; /* always alive */
}

static bool omap_gfx_focus(void *data) {
  (void)data;
  return true; /* fb device always has focus */
}

static void omap_gfx_viewport_info(void *data, struct rarch_viewport *vp) {
  omap_video_t *vid = (omap_video_t*)data;
  vp->x = vp->y = 0;

  vp->width  = vp->full_width  = vid->width;
  vp->height = vp->full_height = vid->height;
}

const video_driver_t video_omap = {
  omap_gfx_init,
  omap_gfx_frame,
  omap_gfx_set_nonblock_state,
  omap_gfx_alive,
  omap_gfx_focus,
  NULL, /* set_shader */
  omap_gfx_free,
  "omap",

#ifdef HAVE_MENU
  NULL, /* restart */
#endif

  NULL, /* set_rotation */
  omap_gfx_viewport_info,
  NULL, /* read_viewport */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  NULL /* poke_interface */
};
