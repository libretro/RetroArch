/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Alfred Agrell
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

#define _XOPEN_SOURCE 700 /* TODO: this doesn't really belong here. */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../common/x11_common.h"
#include "../video_views.h"
#include "../video_views_cpu.h"
#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct xshm
{
   XShmSegmentInfo shmInfo;
   /* The frame, or each of a core's views, scaled into the image */
   video_views_cpu_t cpu;
   video_viewport_t vp;
   XImage* image;
   uint8_t *fbptr;
   /* Both eyes side by side, for anaglyph and interlaced */
   uint32_t *canvas;
   GC gc;
   int width;
   int height;
   unsigned canvas_dims;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t fallback;
   enum scaler_pix_fmt fmt;
   bool use_shm;
   bool smooth;
   bool direct_seen;
} xshm_t;

static void *xshm_init(const video_info_t *video)
{
   unsigned dims;
   xshm_t* xshm = (xshm_t*)calloc(1, sizeof(xshm_t));
   Window parent;
   XSetWindowAttributes attributes;

   XInitThreads();

   g_x11_dpy     = XOpenDisplay(NULL);

   /* Fullscreen at the desktop's size is asked for as 0x0 */
   dims          = video->dims;
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      dims       = VIDEO_SCALE_PACK(
            DisplayWidth(g_x11_dpy,  DefaultScreen(g_x11_dpy)),
            DisplayHeight(g_x11_dpy, DefaultScreen(g_x11_dpy)));

   xshm->use_shm = true;

   if (!XShmQueryExtension(g_x11_dpy))
   {
      RARCH_LOG("[X11] XShm extension not found.\n");
      xshm->use_shm        = false;
   }

#ifdef RARCH_INTERNAL
   parent                  = DefaultRootWindow(g_x11_dpy);
#else
   parent                  = video->parent;
#endif
   attributes.border_pixel = 0;
   g_x11_win               = XCreateWindow(g_x11_dpy, parent,
			     0, 0, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims),
			     0, 24, CopyFromParent, NULL, CWBorderPixel, &attributes);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);
   XMapWindow(g_x11_dpy, g_x11_win);

   if (xshm->use_shm)
   {
      xshm->shmInfo.shmid = shmget(IPC_PRIVATE, sizeof(uint32_t) * VIDEO_SCALE_AREA(dims),
				   IPC_CREAT | 0600);
      if (xshm->shmInfo.shmid < 0)
         abort();/* seems like an OOM situation... let's just blow up. */

      xshm->shmInfo.shmaddr  = (char*)shmat(xshm->shmInfo.shmid, 0, 0);
      xshm->shmInfo.readOnly = False;
      XShmAttach(g_x11_dpy, &xshm->shmInfo);
      XSync(g_x11_dpy, False);/* no idea why this is required, but I get weird errors without it. */
      xshm->image = XShmCreateImage(g_x11_dpy, NULL, 24, ZPixmap,
				    xshm->shmInfo.shmaddr, &xshm->shmInfo, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      xshm->fbptr = (uint8_t*)xshm->shmInfo.shmaddr;
   }
   else
   {
      size_t pitch = VIDEO_SCALE_W(dims) * 4;
      void *data   = malloc (pitch * VIDEO_SCALE_H(dims));
      if (!data)
         abort(); /* seems like an OOM situation... let's just blow up. */
      xshm->image  = XCreateImage(g_x11_dpy, NULL, 24, ZPixmap, 0,
				 (char *) data, VIDEO_SCALE_W(dims),
				 VIDEO_SCALE_H(dims), 8, pitch);
      xshm->fbptr  = (uint8_t*)data;
      XSync(g_x11_dpy, False);
   }

   xshm->gc     = XCreateGC(g_x11_dpy, g_x11_win, 0, NULL);

   xshm->width  = VIDEO_SCALE_W(dims);
   xshm->height = VIDEO_SCALE_H(dims);
   xshm->fmt    = video->rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
   xshm->smooth = video->smooth;
   xshm->vp.full_dims = dims;
   /* The size screenshots are cut to */
   video_driver_set_output_dims(dims);

   if (!x11_input_ctx_new(true))
      goto error;

   /* no input driver of this driver's own: the frontend starts the
    * one that goes with an X11 window */
   input_driver_video_window(INPUT_WINDOW_X11, NULL);

   return xshm;
 error:
   free (xshm);
   return NULL;
}

/* The image's pixels, rows 4 bytes per pixel */
#define XSHM_STRIDE(xshm) ((unsigned)(xshm)->width * sizeof(uint32_t))

/* Whether this frame draws its views. Anaglyph and interlaced need the
 * canvas, both eyes side by side. */
static bool xshm_views_setup(xshm_t *xshm,
      const video_frame_info_t *video_info, unsigned frame_dims)
{
   const video_views_layout_t *layout = &video_info->views_layout;
   bool views = video_info->views.num_views
      && video_views_fit_frame(&video_info->views, frame_dims);

   if (views && layout->offscreen && xshm->canvas_dims != layout->canvas_dims)
   {
      free(xshm->canvas);
      xshm->canvas_dims = 0;
      if ((xshm->canvas = (uint32_t*)malloc(VIDEO_SCALE_AREA(
                  layout->canvas_dims) * sizeof(uint32_t))))
         xshm->canvas_dims = layout->canvas_dims;
      views = xshm->canvas != NULL;
   }
   else if (!views || !layout->offscreen)
   {
      free(xshm->canvas);
      xshm->canvas      = NULL;
      xshm->canvas_dims = 0;
   }
   retro_atomic_store_release_int(&xshm->fallback,
         (video_info->views.num_views && !views) ? 1 : 0);
   if (views && !xshm->direct_seen)
   {
      xshm->direct_seen = true;
      RARCH_LOG("[XSHM] Views drawn straight from the frame.\n");
   }
   return views;
}

/* The frame's views scaled into the image, or into the canvas whose
 * halves are blended into it */
static void xshm_views_render(xshm_t *xshm,
      const video_frame_info_t *video_info, const void *frame,
      unsigned pitch)
{
   unsigned i;
   const video_views_layout_t *layout = &video_info->views_layout;
   uint8_t *pixels = layout->offscreen ? (uint8_t*)xshm->canvas : xshm->fbptr;
   unsigned dims   = layout->offscreen ? xshm->canvas_dims
      : VIDEO_SCALE_PACK(xshm->width, xshm->height);
   unsigned stride = VIDEO_SCALE_W(dims) * sizeof(uint32_t);
   enum scaler_type type = xshm->smooth
      ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;

   if (layout->offscreen)
      memset(pixels, 0, VIDEO_SCALE_AREA(dims) * sizeof(uint32_t));
   for (i = 0; i < layout->num_placements; i++)
   {
      video_views_rect_t cut;
      const video_views_placement_t *p = &layout->placements[i];
      if (video_views_visible(layout, p, dims, &cut))
         video_views_cpu_draw_view(&xshm->cpu, xshm->fmt, type,
               &video_info->views.views[p->view], frame, pitch,
               pixels, stride, &p->dst, &cut);
   }
   if (layout->offscreen)
      video_views_blend_xrgb8888(xshm->canvas, VIDEO_SCALE_W(dims),
            (uint32_t*)xshm->fbptr, (size_t)xshm->width,
            VIDEO_SCALE_PACK(xshm->width, xshm->height),
            layout->stereo_mode);
}

static bool xshm_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned width     = VIDEO_SCALE_W(dims);
   unsigned height    = VIDEO_SCALE_H(dims);
   bool views         = false;
   xshm_t      *xshm  = (xshm_t*)data;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (frame && width && height)
   {
      views = xshm_views_setup(xshm, video_info, dims);
      /* The viewport is the whole window while views draw. */
      video_driver_update_viewport(&xshm->vp, views, true, true);
      memset(xshm->fbptr, 0, (size_t)xshm->height * XSHM_STRIDE(xshm));
      if (views)
         xshm_views_render(xshm, video_info, frame, pitch);
      else
      {
         video_views_rect_t dst, cut;
         dst.pos  = xshm->vp.pos;
         dst.dims = xshm->vp.dims;
         if (video_views_clip(&dst,
                  VIDEO_SCALE_PACK(xshm->width, xshm->height), &cut))
            video_views_cpu_draw(&xshm->cpu, xshm->fmt,
                  xshm->smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT,
                  frame, width, height, pitch, xshm->fbptr,
                  XSHM_STRIDE(xshm), &dst, &cut);
      }
   }

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (xshm->use_shm)
      XShmPutImage(g_x11_dpy, g_x11_win, xshm->gc, xshm->image,
		   0, 0, 0, 0, xshm->width, xshm->height, False);
   else
      XPutImage(g_x11_dpy, g_x11_win, xshm->gc, xshm->image,
		0, 0, 0, 0, xshm->width, xshm->height);
   XFlush(g_x11_dpy);

   return true;
}

static void xshm_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool xshm_alive(void *data) { return true; }
static bool xshm_focus(void *data) { return true; }
static bool xshm_suppress_screensaver(void *data, bool enable) { return false; }
static void xshm_free(void *data)
{
   xshm_t *xshm = (xshm_t*)data;
   if (!xshm)
      return;
   video_views_cpu_free(&xshm->cpu);
   free(xshm->canvas);
   free(xshm);
}

static void xshm_poke_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   xshm_t *xshm = (xshm_t*)data;
   if (xshm)
      xshm->smooth = smooth;
}

static uint32_t xshm_get_flags(void *data)
{
   uint32_t flags = 0;
   xshm_t  *xshm  = (xshm_t*)data;
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   if (xshm)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&xshm->fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

static void xshm_viewport_info(void *data, struct video_viewport *vp)
{
   xshm_t *xshm = (xshm_t*)data;
   if (xshm)
      *vp = xshm->vp;
}

/* The viewport's pixels in the image the last frame was composed in,
 * bottom row first as BGR24 */
static bool xshm_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   int x, y, vx, vy, vw, vh;
   xshm_t *xshm = (xshm_t*)data;
   if (!xshm)
      return false;
   vx = VIDEO_POS_X(xshm->vp.pos);
   vy = VIDEO_POS_Y(xshm->vp.pos);
   vw = (int)VIDEO_SCALE_W(xshm->vp.dims);
   vh = (int)VIDEO_SCALE_H(xshm->vp.dims);
   for (y = vh - 1; y >= 0; y--)
   {
      for (x = 0; x < vw; x++)
      {
         uint32_t px = 0;
         int sx      = vx + x;
         int sy      = vy + y;
         if (sx >= 0 && sy >= 0 && sx < xshm->width && sy < xshm->height)
            px = ((const uint32_t*)xshm->fbptr)[(size_t)sy * xshm->width + sx];
         *buffer++ = (uint8_t)(px);
         *buffer++ = (uint8_t)(px >> 8);
         *buffer++ = (uint8_t)(px >> 16);
      }
   }
   return true;
}
static void xshm_poke_set_aspect_ratio(void *data, unsigned aspect_ratio_idx) { }
static void xshm_poke_apply_state_changes(void *data) { }
static void xshm_poke_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned dims, float alpha) { }
static void xshm_poke_texture_enable(void *data,
      bool enable, bool full_screen) { }
static void xshm_poke_set_osd_msg(void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params, void *font) { }
static void xshm_show_mouse(void *data, bool state) { }
static void xshm_grab_mouse_toggle(void *data) { }

static video_poke_interface_t xshm_video_poke_interface = {
   xshm_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
#ifdef HAVE_XF86VM
   x11_get_refresh_rate,
#else
   NULL, /* get_refresh_rate */
#endif
   xshm_poke_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   xshm_poke_set_aspect_ratio,
   xshm_poke_apply_state_changes,
   xshm_poke_set_texture_frame,
   xshm_poke_texture_enable,
   xshm_poke_set_osd_msg,
   xshm_show_mouse,
   xshm_grab_mouse_toggle,
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void xshm_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &xshm_video_poke_interface;
}
static bool xshm_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

video_driver_t video_xshm = {
   xshm_init,
   xshm_frame,
   xshm_set_nonblock_state,
   xshm_alive,
   xshm_focus,
   xshm_suppress_screensaver,
   NULL, /* has_windowed */
   xshm_set_shader,
   xshm_free,
   "x11",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   xshm_viewport_info,
   xshm_read_viewport,
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   xshm_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
