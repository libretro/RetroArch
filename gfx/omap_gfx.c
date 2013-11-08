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

#include "omapfb.h"
#include "fbdev.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MIN(a, b) ( ((a) < (b)) ? (a) : (b) )

enum omap_layer_size {
  OMAP_LAYER_UNSCALED,
  OMAP_LAYER_FULLSCREEN,
  OMAP_LAYER_SCALED,
  OMAP_LAYER_PIXELPERFECT,
  OMAP_LAYER_CUSTOM
};

typedef struct omapfb_state {
  struct omapfb_plane_info pi;
  struct omapfb_mem_info mi;
  struct omapfb_plane_info pi_old;
  struct omapfb_mem_info mi_old;
} omapfb_state_t;

typedef struct osdl_data
{
  struct vout_fbdev *fbdev;
  void *front_buffer;
  void *saved_layer;
  /* physical/native screen size */
  int phys_w, phys_h;
  /* layer */
  int layer_x, layer_y, layer_w, layer_h;
  enum omap_layer_size layer_size;
  bool vsync;
} osdl_data_t;

static const char *get_fb_device(void)
{
  const char *fbname = getenv("OMAP_FBDEV");
  if (fbname == NULL)
    fbname = "/dev/fb1";

  return fbname;
}

static int osdl_setup_omapfb(struct omapfb_state *ostate, int fd, int enabled,
                             int x, int y, int w, int h, int mem, int buffer_count)
{
  struct omapfb_plane_info pi;
  struct omapfb_mem_info mi;
  unsigned int size_cur;
  int ret;

  RARCH_LOG("in osdl_setup_omapfb\n");

  memset(&pi, 0, sizeof(pi));
  memset(&mi, 0, sizeof(mi));

  ret = ioctl(fd, OMAPFB_QUERY_PLANE, &pi);
  if (ret != 0) {
    RARCH_ERR("omapfb: QUERY_PLANE\n");
    return -1;
  }

  ret = ioctl(fd, OMAPFB_QUERY_MEM, &mi);
  if (ret != 0) {
    RARCH_ERR("omapfb: QUERY_MEM\n");
    return -1;
  }
  size_cur = mi.size;

  /* must disable when changing stuff */
  if (pi.enabled) {
    pi.enabled = 0;
    ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
    if (ret != 0)
      RARCH_ERR("SETUP_PLANE\n");
  }

  /* if needed increase memory allocation */
  if (size_cur < mem * buffer_count) {
    mi.size = mem * buffer_count;
    ret = ioctl(fd, OMAPFB_SETUP_MEM, &mi);
    if (ret != 0) {
      RARCH_ERR("omapfb: SETUP_MEM\n");
      RARCH_ERR("Failed to allocate %d bytes of vram.\n", mem * buffer_count);
      return -1;
    }
  }

  pi.pos_x = x;
  pi.pos_y = y;
  pi.out_width = w;
  pi.out_height = h;
  pi.enabled = enabled;

  ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
  if (ret != 0) {
    RARCH_ERR("omapfb: SETUP_PLANE (%d %d %d %d)\n", x, y, w, h);
    return -1;
  }

  ostate->pi = pi;
  ostate->mi = mi;

  return 0;
}

static int osdl_setup_omapfb_enable(struct omapfb_state *ostate,
                                    int fd, int enabled)
{
  int ret;

  ostate->pi.enabled = enabled;
  ret = ioctl(fd, OMAPFB_SETUP_PLANE, &ostate->pi);
  if (ret != 0) RARCH_ERR("omapfb: SETUP_PLANE\n");

  return ret;
}

static int read_sysfs(const char *fname, char *buff, size_t size)
{
  FILE *f;
  int ret;

  f = fopen(fname, "r");
  if (f == NULL) {
    RARCH_ERR("video_omap: open %s failed\n", fname);
    return -1;
  }

  ret = fread(buff, 1, size - 1, f);
  fclose(f);
  if (ret <= 0) {
    RARCH_ERR("video_omap: read %s failed\n", fname);
    return -1;
  }

  buff[ret] = 0;
  for (ret--; ret >= 0 && isspace(buff[ret]); ret--)
    buff[ret] = 0;

  return 0;
}

static int osdl_fbdev_init(struct osdl_data *pdata, int fd)
{
  pdata->fbdev = vout_fbdev_preinit(fd);

  if (pdata->fbdev == NULL) return -1;

  return 0;
}

static int osdl_video_detect_screen(struct osdl_data *pdata, const char *fbname)
{
  int fb_id, overlay_id = -1, display_id = -1;
  char buff[64], manager_name[64], display_name[64];
  struct stat status;
  int fd, i, ret;
  int w, h;
  FILE *f;

  pdata->phys_w = pdata->phys_h = 0;

  /* Figure out screen resolution, we need to know default
   * resolution for centering stuff.
   * The only way to achieve this seems to be walking some sysfs files.. */
  ret = stat(fbname, &status);
  if (ret != 0) {
    RARCH_ERR("video_omap: can't stat %s\n", fbname);
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

  RARCH_LOG("video_omap: detected %dx%d '%s' (%d) display attached to fb %d and overlay %d\n",
            w, h, display_name, display_id, fb_id, overlay_id);

  pdata->phys_w = w;
  pdata->phys_h = h;

  return 0;
}

static int osdl_init(struct osdl_data *pdata)
{
  const char *fbname;
  int ret, fb;

  fbname = get_fb_device();
  ret = osdl_video_detect_screen(pdata, fbname);

  if (ret != 0) return ret;

  fb = open(fbname, O_RDWR);
  if (fb == -1) {
    RARCH_ERR("video_omap: can't open fb device\n");
    return -1;
  }

  ret = osdl_fbdev_init(pdata, fb);

  return ret;
}

static int osdl_setup_omap_layer(struct osdl_data *pdata, int width,
                                 int height, int bpp, int buffer_count)
{
  int x = 0, y = 0, w = width, h = height; /* layer size and pos */
  int screen_w = w, screen_h = h;
  int tmp_w, tmp_h;
  const char *tmp;
  int retval = -1;
  int ret;

  RARCH_LOG("in osdl_setup_omap_layer\n");

  const int fd = vout_fbdev_get_fd(pdata->fbdev);

  if (fd == -1) {
    RARCH_ERR("got no fbdev fd\n");
  }

  pdata->layer_x = pdata->layer_y = pdata->layer_w = pdata->layer_h = 0;

  if (pdata->phys_w != 0)
    screen_w = pdata->phys_w;
  if (pdata->phys_h != 0)
    screen_h = pdata->phys_h;

  /* FIXME: assuming layer doesn't change here */
  if (pdata->saved_layer == NULL) {
    struct omapfb_state *slayer;
    slayer = calloc(1, sizeof(*slayer));
    if (slayer == NULL)
      goto out;

    ret = ioctl(fd, OMAPFB_QUERY_PLANE, &slayer->pi_old);
    if (ret != 0) {
      RARCH_ERR("omapfb: QUERY_PLANE\n");
      goto out;
    }

    ret = ioctl(fd, OMAPFB_QUERY_MEM, &slayer->mi_old);
    if (ret != 0) {
      RARCH_ERR("omapfb: QUERY_MEM\n");
      goto out;
    }

    pdata->saved_layer = slayer;
  }

  switch (pdata->layer_size) {
    case OMAP_LAYER_FULLSCREEN:
    {
      w = screen_w, h = screen_h;
    }
    break;

    case OMAP_LAYER_SCALED:
    {
      const float factor = MIN(((float)screen_w) / width, ((float)screen_h) / height);
      w = (int)(factor*width), h = (int)(factor*height);
    }
    break;

    case OMAP_LAYER_PIXELPERFECT:
    {
      const float factor = MIN(((float)screen_w) / width, ((float)screen_h) / height);
      w = ((int)factor) * width, h = ((int)factor) * height;
      /* factor < 1.f => 0x0 layer, so fall back to 'scaled' */
      if (!w || !h) {
        w = (int)(factor * width), h = (int)(factor * height);
      }
    }
    break;

    // TODO: use g_settings.video.fullscreen_x for this!
    case OMAP_LAYER_CUSTOM:
    {
      tmp = getenv("OMAP_LAYER_SIZE");

      if (tmp != NULL && sscanf(tmp, "%dx%d", &tmp_w, &tmp_h) == 2) {
        w = tmp_w, h = tmp_h;
      } else {
        RARCH_ERR("omap_video: custom layer size specified incorrectly, "
                  "should be like 800x480\n");
      }
    }
    break;

    default:
      break;
  }

  /* the layer can't be set larger than screen */
  tmp_w = w, tmp_h = h;
  if (w > screen_w) w = screen_w;
  if (h > screen_h) h = screen_h;
  if (w != tmp_w || h != tmp_h)
    RARCH_LOG("omap_video: layer resized %dx%d -> %dx%d to fit screen\n", tmp_w, tmp_h, w, h);

  x = screen_w / 2 - w / 2;
  y = screen_h / 2 - h / 2;
  ret = osdl_setup_omapfb(pdata->saved_layer, fd, 0, x, y, w, h,
                          width * height * ((bpp + 7) / 8), buffer_count);

  if (ret == 0) {
    pdata->layer_x = x;
    pdata->layer_y = y;
    pdata->layer_w = w;
    pdata->layer_h = h;
  }

  retval = ret;

out:
  return retval;
}

static void *osdl_video_flip(struct osdl_data *pdata)
{
  void *ret;

  if (pdata->fbdev == NULL)
    return NULL;

  ret = vout_fbdev_flip(pdata->fbdev);

  if (pdata->vsync)
    vout_fbdev_wait_vsync(pdata->fbdev);

  return ret;
}

void osdl_video_finish(struct osdl_data *pdata)
{
  vout_fbdev_release(pdata->fbdev);

  /* restore the OMAP layer */
  if (pdata->saved_layer != NULL) {
    struct omapfb_state *slayer = pdata->saved_layer;
    int fd = vout_fbdev_get_fd(pdata->fbdev);

    int enabled = slayer->pi_old.enabled;

    /* be sure to disable while setting up */
    slayer->pi_old.enabled = 0;
    ioctl(fd, OMAPFB_SETUP_PLANE, &slayer->pi_old);
    ioctl(fd, OMAPFB_SETUP_MEM, &slayer->mi_old);
    if (enabled) {
      slayer->pi_old.enabled = enabled;
      ioctl(fd, OMAPFB_SETUP_PLANE, &slayer->pi_old);
    }

    free(slayer);
    pdata->saved_layer = NULL;
  }

  vout_fbdev_teardown(pdata->fbdev);
  pdata->fbdev = NULL;
}

static void *osdl_video_set_mode(struct osdl_data *pdata, int width,
                                 int height, int bpp)
{
  int num_buffers;
  void *result;
  int ret;

  RARCH_LOG("osdl: setting video mode\n");

  vout_fbdev_release(pdata->fbdev);

  /* always use triple buffering for reduced chance of tearing */
  num_buffers = 3;

  RARCH_LOG("width = %d, height = %d\n", width, height);

  ret = osdl_setup_omap_layer(pdata, width, height, bpp, num_buffers);
  if (ret < 0)
    goto fail;

  ret = vout_fbdev_init(pdata->fbdev, &width, &height, bpp, num_buffers);
  if (ret == -1)
    goto fail;

  result = osdl_video_flip(pdata);
  if (result == NULL)
    goto fail;

  ret = osdl_setup_omapfb_enable(pdata->saved_layer,
                                 vout_fbdev_get_fd(pdata->fbdev), 1);
  if (ret != 0) {
    RARCH_ERR("video_omap: layer enable failed\n");
    goto fail;
  }

  return result;

fail:
  osdl_video_finish(pdata);
  return NULL;
}

void *osdl_video_get_active_buffer(struct osdl_data *pdata)
{
  if (pdata->fbdev == NULL) return NULL;

  return vout_fbdev_get_active_mem(pdata->fbdev);
}

int osdl_video_pause(struct osdl_data *pdata, int is_pause)
{
  struct omapfb_state *state = pdata->saved_layer;
  struct omapfb_plane_info pi;
  struct omapfb_mem_info mi;
  int enabled;
  int fd = -1;
  int ret;

  if (pdata->fbdev != NULL)
    fd = vout_fbdev_get_fd(pdata->fbdev);
  if (fd == -1) {
    RARCH_ERR("bad fd %d", fd);
    return -1;
  }
  if (state == NULL) {
    RARCH_ERR("missing layer state\n");
    return -1;
  }

	if (is_pause) {
		ret = vout_fbdev_save(pdata->fbdev);
		if (ret != 0)
			return ret;
		pi = state->pi_old;
		mi = state->mi_old;
		enabled = pi.enabled;
	} else {
		pi = state->pi;
		mi = state->mi;
		enabled = 1;
	}
	pi.enabled = 0;
	ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
	if (ret != 0) {
		RARCH_ERR("SETUP_PLANE");
		return -1;
	}

	ret = ioctl(fd, OMAPFB_SETUP_MEM, &mi);
	if (ret != 0)
		RARCH_ERR("SETUP_MEM");

	if (!is_pause) {
		ret = vout_fbdev_restore(pdata->fbdev);
		if (ret != 0) {
			RARCH_ERR("fbdev_restore failed\n");
			return ret;
		}
	}

	if (enabled) {
		pi.enabled = 1;
		ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
		if (ret != 0) {
			RARCH_ERR("SETUP_PLANE");
			return -1;
		}
	}

	return 0;
}


typedef struct omap_video
{
  struct osdl_data osdl;
  void* pixels;

  void *font;
  const font_renderer_driver_t *font_driver;
  uint8_t font_r;
  uint8_t font_g;
  uint8_t font_b;

  /* current settings */
  unsigned width;
  unsigned height;
  unsigned bytes_per_pixel;
} omap_video_t;

static void omap_gfx_free(void *data)
{
  omap_video_t *vid = (omap_video_t*)data;
  if (!vid) return;

  osdl_video_finish(&vid->osdl);

  if (vid->font) vid->font_driver->free(vid->font);

  free(vid);
}

static void omap_init_font(omap_video_t *vid, const char *font_path, unsigned font_size)
{
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
    RARCH_LOG("Could not initialize fonts.\n");
  }
}

/*static void omap_render_msg(omap_video_t *vid, SDL_Surface *buffer,
      const char *msg, unsigned width, unsigned height, const SDL_PixelFormat *fmt)
{
  if (!vid->font) return;

   struct font_output_list out;
   vid->font_driver->render_msg(vid->font, msg, &out);
   struct font_output *head = out.head;

   int msg_base_x = g_settings.video.msg_pos_x * width;
   int msg_base_y = (1.0 - g_settings.video.msg_pos_y) * height;

   unsigned rshift = fmt->Rshift;
   unsigned gshift = fmt->Gshift;
   unsigned bshift = fmt->Bshift;

   for (; head; head = head->next)
   {
      int base_x = msg_base_x + head->off_x;
      int base_y = msg_base_y - head->off_y - head->height;

      int glyph_width  = head->width;
      int glyph_height = head->height;

      const uint8_t *src = head->output;

      if (base_x < 0)
      {
         src -= base_x;
         glyph_width += base_x;
         base_x = 0;
      }

      if (base_y < 0)
      {
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

      for (int y = 0; y < glyph_height; y++, src += head->pitch, out += buffer->pitch >> 2)
      {
         for (int x = 0; x < glyph_width; x++)
         {
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
   }

   vid->font_driver->free_output(vid->font, &out);
}*/

static void *omap_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
  void* ret = NULL;

  omap_video_t *vid = (omap_video_t*)calloc(1, sizeof(*vid));
  if (!vid) return NULL;

  if (osdl_init(&vid->osdl) != 0) {
    goto fail;
  }

  RARCH_LOG("Detecting native resolution %ux%u.\n",
            vid->osdl.phys_w, vid->osdl.phys_h);

  if (!video->fullscreen)
    RARCH_LOG("Creating unscaled output @ %ux%u.\n", video->width, video->height);

  if (video->fullscreen) {
    vid->osdl.layer_size = video->force_aspect ?
                           OMAP_LAYER_SCALED : OMAP_LAYER_FULLSCREEN;
  } else {
    vid->osdl.layer_size = OMAP_LAYER_UNSCALED;
  }

  vid->osdl.vsync = video->vsync;
  vid->bytes_per_pixel = video->rgb32 ? 4 : 2;

  // TODO: use geom from geom->base_width / geom->base_height
  // const struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;

  RARCH_LOG("calling osdl_video_set_mode with width = %d, height = %d\n", video->width, video->height);

  // TODO: handle width = height = 0

  ret = osdl_video_set_mode(&vid->osdl, video->width, video->height,
                            vid->bytes_per_pixel * 8);

  if (ret == NULL) {
    goto fail;
  }

  vid->width = video->width;
  vid->height = video->height;
  vid->pixels = ret;

  if (input && input_data) {
    *input = NULL;
    //input_data = NULL;
  }

  omap_init_font(vid, g_settings.video.font_path, g_settings.video.font_size);

  return vid;

fail:
  RARCH_ERR("Failed to init OMAP video output.\n");
  omap_gfx_free(vid);
  return NULL;
}

static void omap_blit_frame(omap_video_t *video, const void *src,
                            unsigned src_pitch)
{
  unsigned i;
  const unsigned pitch = video->width * video->bytes_per_pixel;

  RARCH_LOG("in omap_blit_frame\n");

  for (i = 0; i < video->height; i++) {
    memcpy(video->pixels + pitch * i, src + src_pitch * i, pitch);
  }
}

static bool omap_gfx_frame(void *data, const void *frame, unsigned width,
                           unsigned height, unsigned pitch, const char *msg)
{
  if (!frame) return true;

  omap_video_t *vid = (omap_video_t*)data;

  if (width != vid->width || height != vid->height) {
    void* pixels;

    RARCH_LOG("Dimensions changed -> OMAP reinit\n");
    pixels = osdl_video_set_mode(&vid->osdl, width, height,
                          vid->bytes_per_pixel * 8);

    if (pixels == NULL) {
      RARCH_ERR("OMAP reinit failed\n");
      return false;
    }

    vid->width  = width;
    vid->height = height;
  }

  omap_blit_frame(vid, frame, pitch);

  /*if (msg)
    omap_render_msg(vid, vid->screen, msg, vid->screen->w, vid->screen->h, vid->screen->format);*/

  vid->pixels = osdl_video_flip(&vid->osdl);
  g_extern.frame_count++;

  return true;
}

static void omap_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data; /* NOP */
   (void)state;
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

static void omap_gfx_viewport_info(void *data, struct rarch_viewport *vp)
{
   omap_video_t *vid = (omap_video_t*)data;
   vp->x = vp->y = 0;

   // TODO: maybe set full_width,height to phys_w,h
   vp->width  = vp->full_width  = vid->width;
   vp->height = vp->full_height = vid->height;
}

const video_driver_t video_omap = {
   omap_gfx_init,
   omap_gfx_frame,
   omap_gfx_set_nonblock_state,
   omap_gfx_alive,
   omap_gfx_focus,
   NULL,
   omap_gfx_free,
   "omap",

#ifdef HAVE_MENU
   NULL,
   NULL,
#endif

   NULL,
   omap_gfx_viewport_info,
};
