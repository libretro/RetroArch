/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>

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
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <retro_inline.h>
#include <retro_assert.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <string/stdstring.h>

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../driver.h"
#include "../../retroarch.h"

typedef struct omapfb_page
{
  unsigned yoffset;
  void *buf;
  bool used;
} omapfb_page_t;

typedef struct omapfb_state
{
  struct omapfb_plane_info pi;
  struct omapfb_mem_info mi;
  struct fb_var_screeninfo si;
  void* mem;
} omapfb_state_t;

typedef struct omapfb_data
{
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

  bool sync;
} omapfb_data_t;

static const char *omapfb_get_fb_device(void)
{
   static char fbname[12] = {0};
   settings_t   *settings = config_get_ptr();
   const int        fbidx = settings->uints.video_monitor_index;

   if (fbidx == 0)
      return "/dev/fb0";

   snprintf(fbname, sizeof(fbname), "/dev/fb%d", fbidx - 1);
   RARCH_LOG("[video_omap]: Using %s as framebuffer device.\n", fbname);
   return fbname;
}

static omapfb_page_t *omapfb_get_page(omapfb_data_t *pdata)
{
   unsigned i;
   omapfb_page_t *page = NULL;

   for (i = 0; i < pdata->num_pages; ++i)
   {
      if (&pdata->pages[i] == pdata->cur_page)
         continue;
      if (&pdata->pages[i] == pdata->old_page)
         continue;

      if (!pdata->pages[i].used)
      {
         page = &pdata->pages[i];
         break;
      }
   }

   return page;
}

static void omapfb_page_flip(omapfb_data_t *pdata)
{
   if (pdata->sync)
      ioctl(pdata->fd, OMAPFB_WAITFORGO);

   /* TODO: should we use the manual update feature of the OMAP here? */

   pdata->current_state->si.yoffset = pdata->cur_page->yoffset;
   ioctl(pdata->fd, FBIOPAN_DISPLAY, &pdata->current_state->si);

   if (pdata->old_page)
      pdata->old_page->used = false;
}

static int omapfb_read_sysfs(const char *fname, char *buff, size_t size)
{
   int ret;
   FILE *f = fopen(fname, "r");

   if (!f)
      return -1;

   ret = fread(buff, 1, size - 1, f);
   fclose(f);

   if (ret <= 0)
      return -1;

   buff[ret] = 0;
   for (ret--; ret >= 0 && isspace(buff[ret]); ret--)
      buff[ret] = 0;

   return 0;
}

static INLINE void omapfb_put_pixel_rgb565(
      uint16_t *p, unsigned r, unsigned g, unsigned b)
{
   *p = (((r  >> 3) & 0x1f) << 11) | (((g >> 2) & 0x3f) << 5) | ((b  >> 3) & 0x1f);
}

static INLINE void omapfb_put_pixel_argb8888(
      uint32_t *p, unsigned r, unsigned g, unsigned b)
{
   *p = ((r << 16) & 0xff0000) | ((g << 8) & 0x00ff00) | ((b << 0) & 0x0000ff);
}

static int omapfb_detect_screen(omapfb_data_t *pdata)
{
   struct stat status;
   int i, ret;
   int w, h;
   FILE *f;
   char buff[64];
   char manager_name[64];
   char display_name[64];
   int fb_id, overlay_id = -1, display_id      = -1;

   buff[0] = manager_name[0] = display_name[0] = '\0';

   /* Find out the native screen resolution, which is needed to
    * properly center the scaled image data. */
   ret = stat(pdata->fbname, &status);

   if (ret != 0)
   {
      RARCH_ERR("[video_omap]: can't stat %s.\n", pdata->fbname);
      return -1;
   }
   fb_id = minor(status.st_rdev);

   snprintf(buff, sizeof(buff), "/sys/class/graphics/fb%d/overlays", fb_id);
   f = fopen(buff, "r");
   if (!f)
   {
      RARCH_ERR("[video_omap]: can't open %s.\n", buff);
      return -1;
   }

   ret = fscanf(f, "%d", &overlay_id);
   fclose(f);
   if (ret != 1)
   {
      RARCH_ERR("[video_omap]: can't parse %s.\n", buff);
      return -1;
   }

   snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/overlay%d/manager", overlay_id);
   ret = omapfb_read_sysfs(buff, manager_name, sizeof(manager_name));
   if (ret < 0)
   {
      RARCH_ERR("[video_omap]: can't read manager name.\n");
      return -1;
   }

   for (i = 0; ; i++)
   {
      snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/manager%d/name", i);
      ret = omapfb_read_sysfs(buff, buff, sizeof(buff));

      if (ret < 0)
         break;

      if (string_is_equal(manager_name, buff))
      {
         snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/manager%d/display", i);
         ret = omapfb_read_sysfs(buff, display_name, sizeof(display_name));

         if (ret < 0)
         {
            RARCH_ERR("[video_omap]: can't read display name.\n");
            return -1;
         }

         break;
      }
   }

   if (ret < 0)
   {
      RARCH_ERR("[video_omap]: couldn't find manager.\n");
      return -1;
   }

   for (i = 0; ; i++)
   {
      snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/display%d/name", i);
      ret = omapfb_read_sysfs(buff, buff, sizeof(buff));

      if (ret < 0)
         break;

      if (string_is_equal(display_name, buff))
      {
         display_id = i;
         break;
      }
   }

   if (display_id < 0)
   {
      RARCH_ERR("[video_omap]: couldn't find display.\n");
      return -1;
   }

   snprintf(buff, sizeof(buff), "/sys/devices/platform/omapdss/display%d/timings", display_id);
   f = fopen(buff, "r");
   if (!f)
   {
      RARCH_ERR("[video_omap]: can't open %s.\n", buff);
      return -1;
   }

   ret = fscanf(f, "%*d,%d/%*d/%*d/%*d,%d/%*d/%*d/%*d", &w, &h);
   fclose(f);
   if (ret != 2)
   {
      RARCH_ERR("[video_omap]: can't parse %s (%d).\n", buff, ret);
      return -1;
   }

   if (w <= 0 || h <= 0)
   {
      RARCH_ERR("[video_omap]: unsane dimensions detected (%dx%d).\n", w, h);
      return -1;
   }

   RARCH_LOG("[video_omap]: detected %dx%d '%s' (%d) display attached to fb %d and overlay %d.\n",
         w, h, display_name, display_id, fb_id, overlay_id);

   pdata->nat_w = w;
   pdata->nat_h = h;

   return 0;
}

static int omapfb_setup_pages(omapfb_data_t *pdata)
{
   int i;

   if (!pdata->pages)
   {
      pdata->pages = (omapfb_page_t*)calloc(pdata->num_pages, sizeof(omapfb_page_t));

      if (!pdata->pages)
      {
         RARCH_ERR("[video_omap]: pages allocation failed.\n");
         return -1;
      }
   }

   for (i = 0; i < pdata->num_pages; ++i)
   {
      pdata->pages[i].yoffset = i * pdata->current_state->si.yres;
      pdata->pages[i].buf     = (void*)((uint8_t*)pdata->fb_mem + (i * pdata->fb_framesize));
      pdata->pages[i].used    = false;
   }

   pdata->old_page = NULL;
   pdata->cur_page = &pdata->pages[0];

   memset(pdata->cur_page->buf, 0, pdata->fb_framesize);

   omapfb_page_flip(pdata);
   pdata->cur_page->used = true;

   return 0;
}

static int omapfb_mmap(omapfb_data_t *pdata)
{
   retro_assert(pdata->fb_mem == NULL);

   pdata->fb_mem = mmap(NULL, pdata->current_state->mi.size, PROT_WRITE,
         MAP_SHARED, pdata->fd, 0);

   if (pdata->fb_mem == MAP_FAILED)
   {
      pdata->fb_mem = NULL;
      RARCH_ERR("[video_omap]: framebuffer mmap failed\n");

      return -1;
   }

   return 0;
}

static int omapfb_backup_state(omapfb_data_t *pdata)
{
   void* mem = NULL;

   retro_assert(pdata->saved_state == NULL);

   pdata->saved_state = calloc(1, sizeof(omapfb_state_t));
   if (!pdata->saved_state) return -1;

   if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pdata->saved_state->pi) != 0)
   {
      RARCH_ERR("[video_omap]: backup layer (plane) failed\n");
      return -1;
   }

   if (ioctl(pdata->fd, OMAPFB_QUERY_MEM, &pdata->saved_state->mi) != 0)
   {
      RARCH_ERR("[video_omap]: backup layer (mem) failed\n");
      return -1;
   }

   if (ioctl(pdata->fd, FBIOGET_VSCREENINFO, &pdata->saved_state->si) != 0)
   {
      RARCH_ERR("[video_omap]: backup layer (screeninfo) failed\n");
      return -1;
   }

   pdata->saved_state->mem = malloc(pdata->saved_state->mi.size);
   mem = mmap(NULL, pdata->saved_state->mi.size, PROT_WRITE|PROT_READ,
         MAP_SHARED, pdata->fd, 0);
   if (pdata->saved_state->mem == NULL || mem == MAP_FAILED)
   {
      RARCH_ERR("[video_omap]: backup layer (mem backup) failed\n");
      munmap(mem, pdata->saved_state->mi.size);
      return -1;
   }
   memcpy(pdata->saved_state->mem, mem, pdata->saved_state->mi.size);
   munmap(mem, pdata->saved_state->mi.size);

   return 0;
}

static int omapfb_alloc_mem(omapfb_data_t *pdata)
{
   unsigned mem_size;
   struct omapfb_plane_info pi;
   struct omapfb_mem_info mi;
   void                              *mem = NULL;
   const struct retro_game_geometry *geom = NULL;
   struct retro_system_av_info *av_info   = NULL;

   retro_assert(pdata->current_state == NULL);

   pdata->current_state = (omapfb_state_t*)calloc(1, sizeof(omapfb_state_t));

   if (!pdata->current_state)
      goto error;

   if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pi) != 0)
   {
      RARCH_ERR("[video_omap]: alloc mem (query plane) failed\n");
      goto error;
   }

   if (ioctl(pdata->fd, OMAPFB_QUERY_MEM, &mi) != 0)
   {
      RARCH_ERR("[video_omap]: alloc mem (query mem) failed\n");
      goto error;
   }

   /* disable plane when changing memory allocation */
   if (pi.enabled)
   {
      pi.enabled = 0;
      if (ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pi) != 0)
      {
         RARCH_ERR("[video_omap]: alloc mem (disable plane) failed\n");
         goto error;
      }
   }

   av_info  = video_viewport_get_system_av_info();

   if (av_info)
      geom     = &av_info->geometry;

   if (!geom)
      goto error;

   mem_size = geom->max_width * geom->max_height *
      pdata->bpp * pdata->num_pages;

   if (mi.size < mem_size)
   {
      mi.size = mem_size;

      if (ioctl(pdata->fd, OMAPFB_SETUP_MEM, &mi) != 0)
      {
         RARCH_ERR("[video_omap]: allocation of %u bytes of VRAM failed\n", mem_size);
         goto error;
      }
   }

   mem = mmap(NULL, mi.size, PROT_WRITE|PROT_READ, MAP_SHARED, pdata->fd, 0);
   if (mem == MAP_FAILED)
   {
      RARCH_ERR("[video_omap]: zeroing framebuffer failed\n");
      goto error;
   }
   memset(mem, 0, mi.size);
   munmap(mem, mi.size);

   pdata->current_state->mi = mi;

   /* Don't re-enable the plane here (setup not yet complete) */

   return 0;

error:
   if (pdata->current_state)
      free(pdata->current_state);
   return -1;
}

static int omapfb_setup_screeninfo(omapfb_data_t *pdata, int width, int height)
{
   omapfb_state_t* state    = pdata->current_state;

   state->si.xres           = width;
   state->si.yres           = height;
   state->si.xres_virtual   = width;
   state->si.yres_virtual   = height * pdata->num_pages;
   state->si.xoffset        = 0;
   state->si.yoffset        = 0;
   state->si.bits_per_pixel = pdata->bpp * 8;

   /* OMAPFB_COLOR_ARGB32 for bpp=4, OMAPFB_COLOR_RGB565 for bpp=2 */
   state->si.nonstd         = 0;

   if (ioctl(pdata->fd, FBIOPUT_VSCREENINFO, &state->si) != 0)
   {
      RARCH_ERR("[video_omap]: setup screeninfo failed\n");
      return -1;
   }

   pdata->fb_framesize      = width * height * pdata->bpp;

   return 0;
}

static float omapfb_scaling(omapfb_data_t *pdata, int width, int height)
{
   const float w_factor = (float)pdata->nat_w / (float)width;
   const float h_factor = (float)pdata->nat_h / (float)height;

   return (w_factor < h_factor ? w_factor : h_factor);
}

static int omapfb_setup_plane(omapfb_data_t *pdata, int width, int height)
{
   int x, y;
   struct omapfb_plane_info pi = {0};
   float scale = omapfb_scaling(pdata, width, height);
   int w = (int)(scale * width);
   int h = (int)(scale * height);

   RARCH_LOG("omap_video: scaling %dx%d to %dx%d\n", width, height, w, h);

   x = pdata->nat_w / 2 - w / 2;
   y = pdata->nat_h / 2 - h / 2;

   if (width * height * pdata->bpp * pdata->num_pages > pdata->current_state->mi.size)
   {
      RARCH_ERR("omap_video: fb dimensions too large for allocated buffer\n");
      return -1;
   }

   if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pi) != 0)
   {
      RARCH_ERR("[video_omap]: setup plane (query) failed\n");
      return -1;
   }

   /* Disable the plane during setup to avoid garbage on screen. */
   pi.pos_x      = x;
   pi.pos_y      = y;
   pi.out_width  = w;
   pi.out_height = h;
   pi.enabled    = 0;

   if (ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pi) != 0)
   {
      RARCH_ERR("[video_omap]: setup plane (param = %d %d %d %d) failed\n", x, y, w, h);
      return -1;
   }

   pdata->current_state->pi = pi;

   return 0;
}

static int omapfb_enable_plane(omapfb_data_t *pdata)
{
   struct omapfb_plane_info pi = {0};

   if (ioctl(pdata->fd, OMAPFB_QUERY_PLANE, &pi) != 0)
   {
      RARCH_ERR("[video_omap]: enable plane (query) failed\n");
      return -1;
   }

   pi.enabled = 1;

   if (ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pi) != 0)
   {
      RARCH_ERR("[video_omap]: enable plane failed\n");
      return -1;
   }

   return 0;
}

static int omapfb_init(omapfb_data_t *pdata, unsigned bpp)
{
   const char *fbname   = omapfb_get_fb_device();
   int             fd   = open(fbname, O_RDWR);
   settings_t *settings = config_get_ptr();

   if (fd == -1)
   {
      RARCH_ERR("[video_omap]: can't open framebuffer device\n");
      return -1;
   }

   pdata->fbname        = fbname;
   pdata->fd            = fd;

   if (omapfb_detect_screen(pdata))
   {
      close(fd);

      pdata->fbname      = NULL;
      pdata->fd          = -1;

      return -1;
   }

   /* always use triple buffering to reduce chance of tearing */
   pdata->bpp           = bpp;
   pdata->num_pages     = 3;
   pdata->sync          = settings->bools.video_vsync;

   return 0;
}

void omapfb_free(omapfb_data_t *pdata)
{
   if (pdata->sync)
      ioctl(pdata->fd, OMAPFB_WAITFORGO);

   /* unmap the framebuffer memory */
   if (pdata->fb_mem)
   {
      munmap(pdata->fb_mem, pdata->current_state->mi.size);
      pdata->fb_mem = NULL;
   }

   /* restore the framebuffer state (OMAP plane state, screen info) */
   if (pdata->saved_state)
   {
      void *mem;
      int enabled = pdata->saved_state->pi.enabled;

      /* be sure to disable while setting up */
      pdata->saved_state->pi.enabled = 0;
      ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pdata->saved_state->pi);
      ioctl(pdata->fd, OMAPFB_SETUP_MEM, &pdata->saved_state->mi);

      if (enabled)
      {
         pdata->saved_state->pi.enabled = enabled;
         ioctl(pdata->fd, OMAPFB_SETUP_PLANE, &pdata->saved_state->pi);
      }

      /* restore framebuffer content */
      mem = mmap(0, pdata->saved_state->mi.size, PROT_WRITE|PROT_READ,
            MAP_SHARED, pdata->fd, 0);

      if (mem != MAP_FAILED)
      {
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

static int omapfb_set_mode(omapfb_data_t *pdata, int width, int height)
{
   if (pdata->sync)
      ioctl(pdata->fd, OMAPFB_WAITFORGO);

   if (omapfb_setup_plane(pdata, width, height) != 0)
      return -1;

   if (omapfb_setup_screeninfo(pdata, width, height) != 0 ||
         omapfb_setup_pages(pdata) != 0 ||
         omapfb_enable_plane(pdata) != 0)
      return -1;

   return 0;
}

static void omapfb_prepare(omapfb_data_t *pdata)
{
   omapfb_page_t *page = NULL;

   /* issue flip before getting free page */
   omapfb_page_flip(pdata);

   page            = omapfb_get_page(pdata);

   retro_assert(page != NULL);

   pdata->old_page = pdata->cur_page;
   pdata->cur_page = page;

   pdata->cur_page->used = true;
}

static void omapfb_blend_glyph_rgb565(omapfb_data_t *pdata,
      const uint8_t *src, uint8_t *f_rgb,
      unsigned g_width, unsigned g_height, unsigned g_pitch,
      unsigned dst_x, unsigned dst_y)
{
   unsigned x, y;
   unsigned r, g, b;
   unsigned dst_pitch = (pdata->current_state->si.xres * pdata->bpp) >> 1;
   uint16_t *dst      = (uint16_t*)pdata->cur_page->buf + dst_y * dst_pitch + dst_x;

   for (y = 0; y < g_height; ++y, src += g_pitch, dst += dst_pitch)
   {
      for (x = 0; x < g_width; ++x)
      {
         const uint8_t blend = src[x];
         const uint16_t out  = dst[x];

         if (blend == 0)
            continue;

         if (blend == 255)
         {
            omapfb_put_pixel_rgb565(&dst[x], f_rgb[0], f_rgb[1], f_rgb[2]);
            continue;
         }

         r = (out & 0xf800) >> 11;
         g = (out & 0x07e0) >> 5;
         b = (out & 0x001f) >> 0;

         r = (r << 3) | (r >> 2);
         g = (g << 2) | (g >> 4);
         b = (b << 3) | (b >> 2);

         omapfb_put_pixel_rgb565(&dst[x], (r * (256 - blend) + f_rgb[0] * blend) >> 8,
               (g * (256 - blend) + f_rgb[1] * blend) >> 8,
               (b * (256 - blend) + f_rgb[2] * blend) >> 8);
      }
   }
}

static void omapfb_blend_glyph_argb8888(omapfb_data_t *pdata,
      const uint8_t *src, uint8_t *f_rgb,
      unsigned g_width, unsigned g_height, unsigned g_pitch,
      unsigned dst_x, unsigned dst_y)
{
   unsigned x, y;
   unsigned r, g, b;
   unsigned dst_pitch = (pdata->current_state->si.xres * pdata->bpp) >> 2;
   uint32_t *dst      = (uint32_t*)pdata->cur_page->buf + dst_y * dst_pitch + dst_x;

   for (y = 0; y < g_height; ++y, src += g_pitch, dst += dst_pitch)
   {
      for (x = 0; x < g_width; ++x)
      {
         const uint8_t blend = src[x];
         const uint32_t out = dst[x];

         if (blend == 0)
            continue;
         if (blend == 255)
         {
            omapfb_put_pixel_argb8888(&dst[x], f_rgb[0], f_rgb[1], f_rgb[2]);
            continue;
         }

         r = (out & 0xff0000) >> 16;
         g = (out & 0x00ff00) >> 8;
         b = (out & 0x0000ff) >> 0;

         omapfb_put_pixel_argb8888(&dst[x], (r * (256 - blend) + f_rgb[0] * blend) >> 8,
               (g * (256 - blend) + f_rgb[1] * blend) >> 8,
               (b * (256 - blend) + f_rgb[2] * blend) >> 8);
      }
   }
}

static void omapfb_blit_frame(omapfb_data_t *pdata, const void *src,
      unsigned height, unsigned src_pitch)
{
   unsigned i;
   void          *dst = pdata->cur_page->buf;
   unsigned dst_pitch = pdata->current_state->si.xres * pdata->bpp;

   for (i = 0; i < height; i++)
      memcpy(dst + dst_pitch * i, src + src_pitch * i, dst_pitch);
}

typedef struct omap_video
{
   omapfb_data_t *omap;

   void *font;
   const font_renderer_driver_t *font_driver;
   uint8_t font_rgb[4];

   unsigned bytes_per_pixel;

   /* current dimensions */
   unsigned width;
   unsigned height;

   struct
   {
      bool active;
      void *frame;
      struct scaler_ctx scaler;
   } menu;
} omap_video_t;

static void omap_gfx_free(void *data)
{
   omap_video_t *vid = data;
   if (!vid)
      return;

   omapfb_free(vid->omap);
   free(vid->omap);

   if (vid->font)
      vid->font_driver->free(vid->font);

   scaler_ctx_gen_reset(&vid->menu.scaler);
   free(vid->menu.frame);

   free(vid);
}

static void omap_init_font(omap_video_t *vid, const char *font_path, unsigned font_size)
{
   int r, g, b;
   settings_t *settings = config_get_ptr();

   if (!settings->bools.video_font_enable)
      return;

   if (!(font_renderer_create_default(&vid->font_driver, &vid->font,
               *settings->paths.path_font ? settings->paths.path_font : NULL, settings->video.font_size)))
   {
      RARCH_LOG("[video_omap]: font init failed\n");
      return;
   }

   r = settings->floats.video_msg_color_r * 255;
   g = settings->floats.video_msg_color_g * 255;
   b = settings->floats.video_msg_color_b * 255;

   r = (r < 0) ? 0 : (r > 255 ? 255 : r);
   g = (g < 0) ? 0 : (g > 255 ? 255 : g);
   b = (b < 0) ? 0 : (b > 255 ? 255 : b);

   vid->font_rgb[0] = r;
   vid->font_rgb[1] = g;
   vid->font_rgb[2] = b;
}

static void omap_render_msg(omap_video_t *vid, const char *msg)
{
   const struct font_atlas *atlas = NULL;
   settings_t *settings = config_get_ptr();
   int msg_base_x = settings->floats.video_msg_pos_x * vid->width;
   int msg_base_y = (1.0 - settings->floats.video_msg_pos_y) * vid->height;

   if (!vid->font)
      return;

   atlas = vid->font_driver->get_atlas(vid->font);

   for (; *msg; msg++)
   {
      int base_x, base_y;
      int glyph_width, glyph_height;
      const uint8_t *src = NULL;
      const struct font_glyph *glyph =
         vid->font_driver->get_glyph(vid->font, (uint8_t)*msg);

      if (!glyph)
         continue;

      base_x               = msg_base_x + glyph->draw_offset_x;
      base_y               = msg_base_y + glyph->draw_offset_y;

      const int max_width  = vid->width - base_x;
      const int max_height = vid->height - base_y;

      glyph_width          = glyph->width;
      glyph_height         = glyph->height;

      src                  = atlas->buffer + glyph->atlas_offset_x +
         glyph->atlas_offset_y * atlas->width;

      if (base_x < 0)
      {
         src         -= base_x;
         glyph_width += base_x;
         base_x       = 0;
      }

      if (base_y < 0)
      {
         src          -= base_y * (int)atlas->width;
         glyph_height += base_y;
         base_y        = 0;
      }

      if (max_width <= 0 || max_height <= 0)
         continue;

      if (glyph_width > max_width)
         glyph_width = max_width;
      if (glyph_height > max_height)
         glyph_height = max_height;

      if (vid->bytes_per_pixel == 2)
      {
         omapfb_blend_glyph_rgb565(vid->omap, src, vid->font_rgb,
               glyph_width, glyph_height,
               atlas->width, base_x, base_y);
      }
      else
      {
         omapfb_blend_glyph_argb8888(vid->omap, src, vid->font_rgb,
               glyph_width, glyph_height,
               atlas->width, base_x, base_y);
      }

      msg_base_x += glyph->advance_x;
      msg_base_y += glyph->advance_y;
   }
}

/* FIXME/TODO: Filters not supported. */
static void *omap_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   settings_t *settings = config_get_ptr();
   omap_video_t *vid    = (omap_video_t*)calloc(1, sizeof(omap_video_t));
   if (!vid)
      return NULL;

   vid->omap = (omapfb_data_t*)calloc(1, sizeof(omapfb_data_t));
   if (!vid->omap)
      goto fail;

   vid->bytes_per_pixel = video->rgb32 ? 4 : 2;

   if (omapfb_init(vid->omap, vid->bytes_per_pixel) != 0)
      goto fail_omapfb;

   if (omapfb_backup_state(vid->omap) != 0 ||
         omapfb_alloc_mem(vid->omap) != 0 ||
         omapfb_mmap(vid->omap) != 0)
      goto fail_omapfb;

   /* set some initial mode for the menu */
   vid->width  = 320;
   vid->height = 240;

   if (omapfb_set_mode(vid->omap, vid->width, vid->height) != 0)
      goto fail_omapfb;

   if (input && input_data)
      *input = NULL;

   omap_init_font(vid, settings->paths.path_font, settings->video.font_size);

   vid->menu.frame = calloc(vid->width * vid->height, vid->bytes_per_pixel);
   if (!vid->menu.frame)
      goto fail_omapfb;

   vid->menu.scaler.scaler_type = SCALER_TYPE_BILINEAR;
   vid->menu.scaler.out_fmt = (vid->bytes_per_pixel == 4)
      ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;

   return vid;

fail_omapfb:
   omapfb_free(vid->omap);
   free(vid->omap);
fail:
   free(vid);
   RARCH_ERR("[video_omap]: initialization failed\n");
   return NULL;
}

static bool omap_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   omap_video_t *vid = (omap_video_t*)data;

   if (!frame)
      return true;

   if (width > 4 && height > 4 && (width != vid->width || height != vid->height))
   {
      RARCH_LOG("[video_omap]: mode set (resolution changed by core)\n");

      if (omapfb_set_mode(vid->omap, width, height) != 0)
      {
         RARCH_ERR("[video_omap]: mode set failed\n");
         return false;
      }

      vid->width = width;
      vid->height = height;
   }

   omapfb_prepare(vid->omap);
   omapfb_blit_frame(vid->omap, frame, vid->height, pitch);

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   if (vid->menu.active)
      omapfb_blit_frame(vid->omap, vid->menu.frame,
            vid->menu.scaler.out_height,
            vid->menu.scaler.out_stride);

   if (msg)
      omap_render_msg(vid, msg);

   return true;
}

static void omap_gfx_set_nonblock_state(void *data, bool state)
{
   omap_video_t *vid;

   if (!data)
      return;

   vid = data;
   vid->omap->sync = !state;
}

static bool omap_gfx_alive(void *data)
{
   (void)data;
   return true; /* always alive */
}

static bool omap_gfx_focus(void *data)
{
   (void)data;
   return true; /* fb device always has focus */
}

static void omap_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   omap_video_t *vid = (omap_video_t*)data;

   if (!vid)
      return;

   vp->x = vp->y = 0;

   vp->width  = vp->full_width  = vid->width;
   vp->height = vp->full_height = vid->height;
}

static bool omap_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool omap_gfx_has_windowed(void *data)
{
   (void)data;

   /* TODO - implement. */
   return true;
}

static bool omap_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void omap_gfx_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   omap_video_t          *vid = (omap_video_t*)data;
   enum scaler_pix_fmt format = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGBA4444;

   video_frame_scale(
         &vid->menu.scaler,
         vid->menu.frame,
         frame,
         format,
         vid->width,
         vid->height,
         vid->width * vid->bytes_per_pixel,
         width,
         height,
         width * (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t)));
}

static void omap_gfx_set_texture_enable(void *data, bool state, bool full_screen)
{
   omap_video_t *vid = (omap_video_t*)data;
   vid->menu.active = state;

   (void) full_screen;
}

static float omap_get_refresh_rate(void *data)
{
   omap_video_t *vid = (omap_video_t*)data;
   struct fb_var_screeninfo *s = &vid->omap->current_state->si;

   return 1000000.0f / s->pixclock /
          (s->xres + s->left_margin + s->right_margin + s->hsync_len) * 1000000.0f /
          (s->yres + s->upper_margin + s->lower_margin + s->vsync_len);
}

static const video_poke_interface_t omap_gfx_poke_interface = {
   NULL, /* get_flags  */
   NULL,
   NULL,
   NULL,
   omap_get_refresh_rate,
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   omap_gfx_set_texture_frame,
   omap_gfx_set_texture_enable,
   NULL,
   NULL,                         /* show_mouse */
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void omap_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &omap_gfx_poke_interface;
}

video_driver_t video_omap = {
   omap_gfx_init,
   omap_gfx_frame,
   omap_gfx_set_nonblock_state,
   omap_gfx_alive,
   omap_gfx_focus,
   omap_gfx_suppress_screensaver,
   omap_gfx_has_windowed,
   omap_gfx_set_shader,
   omap_gfx_free,
   "omap",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   omap_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   omap_gfx_get_poke_interface
};
