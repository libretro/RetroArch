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

#include "../../configuration.h"
#include "../font_driver.h"
#include "../common/x11_common.h"
#include "../../verbosity.h"

typedef struct xshm
{
   int width;
   int height;
   bool use_shm;
   uint8_t *fbptr;

   XShmSegmentInfo shmInfo;
   XImage* image;
   GC gc;
} xshm_t;

static void *xshm_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   xshm_t* xshm = (xshm_t*)malloc(sizeof(xshm_t));
   Window parent;
   XSetWindowAttributes attributes;

   XInitThreads();

   g_x11_dpy = XOpenDisplay(NULL);

   xshm->use_shm = true;

   if (!XShmQueryExtension(g_x11_dpy))
   {
      RARCH_LOG("[X11]: XShm extension not found.\n");
      xshm->use_shm = false;
   }

#ifdef RARCH_INTERNAL
   parent = DefaultRootWindow(g_x11_dpy);
#else
   parent = video->parent;
#endif
   attributes.border_pixel=0;
   g_x11_win = XCreateWindow(g_x11_dpy, parent,
			     0, 0, video->width, video->height,
			     0, 24, CopyFromParent, NULL, CWBorderPixel, &attributes);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);
   XMapWindow(g_x11_dpy, g_x11_win);

   if (xshm->use_shm)
   {
      xshm->shmInfo.shmid = shmget(IPC_PRIVATE, sizeof(uint32_t) * video->width * video->height,
				   IPC_CREAT|0600);
      if (xshm->shmInfo.shmid<0) abort();/* seems like an out of memory situation... let's just blow up. */

      xshm->shmInfo.shmaddr = (char*)shmat(xshm->shmInfo.shmid, 0, 0);
      xshm->shmInfo.readOnly = False;
      XShmAttach(g_x11_dpy, &xshm->shmInfo);
      XSync(g_x11_dpy, False);/* no idea why this is required, but I get weird errors without it. */
      xshm->image = XShmCreateImage(g_x11_dpy, NULL, 24, ZPixmap,
				    xshm->shmInfo.shmaddr, &xshm->shmInfo, video->width, video->height);
      xshm->fbptr = (uint8_t*)xshm->shmInfo.shmaddr;
   } else {
      size_t pitch = video->width * 4;
      void *data = malloc (pitch * video->height);
      if (!data) abort();/* seems like an out of memory situation... let's just blow up. */
      xshm->image = XCreateImage(g_x11_dpy, NULL, 24, ZPixmap, 0,
				 (char *) data, video->width,
				 video->height, 8, pitch);
      xshm->fbptr = (uint8_t*)data;
      XSync(g_x11_dpy, False);
   }

   xshm->gc = XCreateGC(g_x11_dpy, g_x11_win, 0, NULL);

   xshm->width = video->width;
   xshm->height = video->height;

   if (!x11_input_ctx_new(true))
      goto error;

   if (input && input_data)
   {
      settings_t *settings                   = config_get_ptr();
      void *xinput                           = input_driver_init_wrap(&input_x, 
            settings->arrays.input_joypad_driver);
      if (xinput)
      {
         *input = &input_x;
         *input_data = xinput;
      }
      else
         *input = NULL;
   }

   return xshm;
 error:
   free (xshm);
   return NULL;
}

static bool xshm_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned y;
   xshm_t      *xshm  = (xshm_t*)data;
#ifdef HAVE_MENU
   bool menu_is_alive = video_info->menu_is_alive;
#endif

   for (y = 0; y < height; y++)
      memcpy(xshm->fbptr + sizeof(uint32_t)*xshm->width*y,
            (uint8_t*)frame + pitch*y, pitch);

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

static void xshm_gfx_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool xshm_gfx_alive(void *data) { return true; }
static bool xshm_gfx_focus(void *data) { return true; }
static bool xshm_gfx_suppress_screensaver(void *data, bool enable) { return false; }
static void xshm_gfx_free(void *data) { }
static void xshm_poke_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling) { }
static void xshm_poke_set_aspect_ratio(void *data, unsigned aspect_ratio_idx) { }
static void xshm_poke_apply_state_changes(void *data) { }
static void xshm_poke_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha) { }
static void xshm_poke_texture_enable(void *data,
      bool enable, bool full_screen) { }
static void xshm_poke_set_osd_msg(void *data,
      const char *msg,
      const void *params, void *font) { }
static void xshm_show_mouse(void *data, bool state) { }
static void xshm_grab_mouse_toggle(void *data) { }

static video_poke_interface_t xshm_video_poke_interface = {
   NULL, /* get_flags */
   NULL,
   NULL,
   NULL,
   x11_get_refresh_rate,
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
   NULL,                   /* get_current_shader */
   NULL,                   /* get_current_software_framebuffer */
   NULL                    /* get_hw_render_interface */
};

static void xshm_gfx_poke_interface(void *data, const video_poke_interface_t **iface) { *iface = &xshm_video_poke_interface; }
static bool xshm_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

video_driver_t video_xshm = {
   xshm_gfx_init,
   xshm_gfx_frame,
   xshm_gfx_set_nonblock_state,
   xshm_gfx_alive,
   xshm_gfx_focus,
   xshm_gfx_suppress_screensaver,
   NULL, /* has_windowed */
   xshm_gfx_set_shader,
   xshm_gfx_free,
   "x11",

   NULL,
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
    NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
    xshm_gfx_poke_interface
};
