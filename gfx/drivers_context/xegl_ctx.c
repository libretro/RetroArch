/*  RetroArch - A frontend for libretro.
*  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
*  Copyright (C) 2011-2016 - Daniel De Matteis
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

/* X/EGL context. Mostly used for testing GLES code paths. */

#include <stdint.h>

#include "../../driver.h"
#include "../common/egl_common.h"
#include "../common/gl_common.h"
#include "../common/x11_common.h"

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   XF86VidModeModeInfo desktop_mode;
   bool should_reset_mode;
} xegl_ctx_data_t;

static enum gfx_ctx_api x_api;

static int x_nul_handler(Display *dpy, XErrorEvent *event)
{
   (void)dpy;
   (void)event;
   return 0;
}

static void gfx_ctx_xegl_destroy(void *data)
{
   xegl_ctx_data_t *xegl = (xegl_ctx_data_t*)data;

   x11_input_ctx_destroy();
#ifdef HAVE_EGL
   egl_destroy(&xegl->egl);
#endif

   if (g_x11_win)
   {
      /* Save last used monitor for later. */
      x11_save_last_used_monitor(RootWindow(
               g_x11_dpy, DefaultScreen(g_x11_dpy)));
      x11_window_destroy(false);
   }

   x11_colormap_destroy();

   if (xegl->should_reset_mode)
   {
      x11_exit_fullscreen(g_x11_dpy, &xegl->desktop_mode);
      xegl->should_reset_mode = false;
   }

   free(data);

   /* Do not close g_x11_dpy. We'll keep one for the entire application 
    * lifecycle to work-around nVidia EGL limitations.
    */
}

static bool gfx_ctx_xegl_set_resize(void *data,
   unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
   return false;
}

#define XEGL_ATTRIBS_BASE \
EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
EGL_RED_SIZE,        1, \
EGL_GREEN_SIZE,      1, \
EGL_BLUE_SIZE,       1, \
EGL_ALPHA_SIZE,      0, \
EGL_DEPTH_SIZE,      0

static void *gfx_ctx_xegl_init(void *video_driver)
{
#ifdef HAVE_EGL
   static const EGLint egl_attribs_gl[] = {
      XEGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };

   static const EGLint egl_attribs_gles[] = {
      XEGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };

#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      XEGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif
#ifdef HAVE_VG
   static const EGLint egl_attribs_vg[] = {
      XEGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };
#endif
   const EGLint *attrib_ptr = NULL;
   EGLint major, minor;
   EGLint n;
#endif
   xegl_ctx_data_t *xegl;

   if (g_egl_inited)
      return NULL;

   XInitThreads();

   xegl = (xegl_ctx_data_t*)calloc(1, sizeof(xegl_ctx_data_t));
   if (!xegl)
      return NULL;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
         attrib_ptr = egl_attribs_gl;
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef EGL_KHR_create_context
         if (xegl->egl.major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
            attrib_ptr = egl_attribs_gles;
         break;
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
         attrib_ptr = egl_attribs_vg;
#endif
         break;
      default:
         break;
   }

   if (!x11_connect())
      goto error;

#ifdef HAVE_EGL
   if (!egl_init_context(&xegl->egl, (EGLNativeDisplayType)g_x11_dpy,
            &major, &minor, &n, attrib_ptr))
   {
      egl_report_error();
      goto error;
   }

   if (n == 0 || !egl_has_config(&xegl->egl))
      goto error;
#endif

   return xegl;

error:
   gfx_ctx_xegl_destroy(xegl);
   return NULL;
}

static EGLint *xegl_fill_attribs(xegl_ctx_data_t *xegl, EGLint *attr)
{
   switch (x_api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
         {
            bool debug       = false;
            unsigned version = xegl->egl.major * 1000 + xegl->egl.minor;
            bool core        = version >= 3001;
            struct retro_hw_render_callback *hwr = NULL;

            video_driver_ctl(RARCH_DISPLAY_CTL_HW_CONTEXT_GET, &hwr);
#ifdef GL_DEBUG
            debug = true;
#else
            debug = hwr->debug_context;
#endif

            if (core)
            {
               *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
               *attr++ = xegl->egl.major;
               *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
               *attr++ = xegl->egl.minor;

               /* Technically, we don't have core/compat until 3.2.
                * Version 3.1 is either compat or not depending 
                * on GL_ARB_compatibility.
                */
               if (version >= 3002)
               {
                  *attr++ = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
                  *attr++ = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
               }
            }

            if (debug)
            {
               *attr++ = EGL_CONTEXT_FLAGS_KHR;
               *attr++ = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
            }

            break;
         }
#endif

      case GFX_CTX_OPENGL_ES_API:
         /* Same as EGL_CONTEXT_MAJOR_VERSION. */
         *attr++ = EGL_CONTEXT_CLIENT_VERSION;
         *attr++ = xegl->egl.major ? (EGLint)xegl->egl.major : 2;
#ifdef EGL_KHR_create_context
         if (xegl->egl.minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = xegl->egl.minor;
         }
#endif
         break;

      default:
         break;
   }

   *attr = EGL_NONE;
   return attr;
}

/* forward declaration */
static void gfx_ctx_xegl_set_swap_interval(void *data, unsigned swap_interval);

static bool gfx_ctx_xegl_set_video_mode(void *data,
   unsigned width, unsigned height,
   bool fullscreen)
{
   XEvent event;
   EGLint egl_attribs[16];
   EGLint *attr;
   EGLint vid, num_visuals;
   bool windowed_full;
   bool true_full = false;
   int x_off = 0;
   int y_off = 0;
   XVisualInfo temp = {0};
   XSetWindowAttributes swa = {0};
   XVisualInfo *vi = NULL;
   settings_t *settings = config_get_ptr();
   xegl_ctx_data_t *xegl = (xegl_ctx_data_t*)data;

   int (*old_handler)(Display*, XErrorEvent*) = NULL;

   x11_install_sighandlers();

   windowed_full = settings->video.windowed_fullscreen;

   attr = egl_attribs;
   attr = xegl_fill_attribs(xegl, attr);

#ifdef HAVE_EGL
   if (!egl_get_native_visual_id(&xegl->egl, &vid))
      goto error;
#endif

   temp.visualid = vid;

   vi = XGetVisualInfo(g_x11_dpy, VisualIDMask, &temp, &num_visuals);
   if (!vi)
      goto error;

   swa.colormap = g_x11_cmap = XCreateColormap(
         g_x11_dpy, RootWindow(g_x11_dpy, vi->screen),
         vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask | KeyPressMask | 
      ButtonPressMask | ButtonReleaseMask | KeyReleaseMask;
   swa.override_redirect = fullscreen ? True : False;

   if (fullscreen && !windowed_full)
   {
      if (x11_enter_fullscreen(g_x11_dpy, width, height, &xegl->desktop_mode))
      {
         xegl->should_reset_mode = true;
         true_full = true;
      }
      else
         RARCH_ERR("[X/EGL]: Entering true fullscreen failed. Will attempt windowed mode.\n");
   }

   if (settings->video.monitor_index)
      g_x11_screen = settings->video.monitor_index - 1;

#ifdef HAVE_XINERAMA
   if (fullscreen || g_x11_screen != 0)
   {
      unsigned new_width  = width;
      unsigned new_height = height;

      if (x11_get_xinerama_coord(g_x11_dpy, g_x11_screen,
               &x_off, &y_off, &new_width, &new_height))
         RARCH_LOG("[X/EGL]: Using Xinerama on screen #%u.\n", g_x11_screen);
      else
         RARCH_LOG("[X/EGL]: Xinerama is not active on screen.\n");

      if (fullscreen)
      {
         width  = new_width;
         height = new_height;
      }
   }
#endif

   RARCH_LOG("[X/EGL]: X = %d, Y = %d, W = %u, H = %u.\n",
         x_off, y_off, width, height);

   g_x11_win = XCreateWindow(g_x11_dpy, RootWindow(g_x11_dpy, vi->screen),
         x_off, y_off, width, height, 0,
         vi->depth, InputOutput, vi->visual, 
         CWBorderPixel | CWColormap | CWEventMask | 
         (true_full ? CWOverrideRedirect : 0), &swa);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);

   if (!egl_create_context(&xegl->egl, (attr != egl_attribs) ? egl_attribs : NULL))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(&xegl->egl, (EGLNativeWindowType)g_x11_win))
      goto error;

   x11_set_window_attr(g_x11_dpy, g_x11_win);

   if (fullscreen)
      x11_show_mouse(g_x11_dpy, g_x11_win, false);

   if (true_full)
   {
      RARCH_LOG("[X/EGL]: Using true fullscreen.\n");
      XMapRaised(g_x11_dpy, g_x11_win);
   }
   else if (fullscreen) 
   {
      /* We attempted true fullscreen, but failed.
       * Attempt using windowed fullscreen. */
      XMapRaised(g_x11_dpy, g_x11_win);
      RARCH_LOG("[X/EGL]: Using windowed fullscreen.\n");

      /* We have to move the window to the screen we 
       * want to go fullscreen on first.
       * x_off and y_off usually get ignored in XCreateWindow().
       */
      x11_move_window(g_x11_dpy, g_x11_win, x_off, y_off, width, height);
      x11_windowed_fullscreen(g_x11_dpy, g_x11_win);
   }
   else
   {
      XMapWindow(g_x11_dpy, g_x11_win);

      /* If we want to map the window on a different screen, 
       * we'll have to do it by force.
       *
       * Otherwise, we should try to let the window manager sort it out.
       * x_off and y_off usually get ignored in XCreateWindow().
       */
      if (g_x11_screen)
         x11_move_window(g_x11_dpy, g_x11_win, x_off, y_off, width, height);
   }

   x11_event_queue_check(&event);
   x11_install_quit_atom();

#ifdef HAVE_EGL
   gfx_ctx_xegl_set_swap_interval(&xegl->egl, xegl->egl.interval);
#endif

   /* This can blow up on some drivers. It's not fatal, 
    * so override errors for this call.
    */
   old_handler = XSetErrorHandler(x_nul_handler);
   XSetInputFocus(g_x11_dpy, g_x11_win, RevertToNone, CurrentTime);
   XSync(g_x11_dpy, False);
   XSetErrorHandler(old_handler);

   XFree(vi);
   g_egl_inited = true;

   if (!x11_input_ctx_new(true_full))
      goto error;

   return true;

error:
   if (vi)
      XFree(vi);

   gfx_ctx_xegl_destroy(data);
   return false;
}


static void gfx_ctx_xegl_input_driver(void *data,
   const input_driver_t **input, void **input_data)
{
   void *xinput = input_x.init();

   (void)data;

   *input       = xinput ? &input_x : NULL;
   *input_data  = xinput;
}

static bool gfx_ctx_xegl_has_focus(void *data)
{
   if (!g_egl_inited)
      return false;

   return x11_has_focus(data);
}

static bool gfx_ctx_xegl_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
      return false;

   x11_suspend_screensaver(video_driver_window_get());

   return true;
}

static bool gfx_ctx_xegl_has_windowed(void *data)
{
   (void)data;

   /* TODO - verify if this has windowed mode or not. */
   return true;
}

static bool gfx_ctx_xegl_bind_api(void *video_driver,
   enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   g_egl_major  = major;
   g_egl_minor  = minor;
   x_api        = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            break;
#endif
         return eglBindAPI(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
#ifndef EGL_KHR_create_context
         if (major >= 3)
            break;
#endif
         return eglBindAPI(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
         return eglBindAPI(EGL_OPENVG_API);
#endif
      default:
         break;
   }

   return false;
}

static void gfx_ctx_xegl_show_mouse(void *data, bool state)
{
   (void)data;
   x11_show_mouse(g_x11_dpy, g_x11_win, state);
}

static void gfx_ctx_xegl_swap_buffers(void *data)
{
   xegl_ctx_data_t *xegl = (xegl_ctx_data_t*)data;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_EGL
         egl_swap_buffers(&xegl->egl);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_xegl_bind_hw_render(void *data, bool enable)
{
   xegl_ctx_data_t *xegl = (xegl_ctx_data_t*)data;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_bind_hw_render(&xegl->egl, enable);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_xegl_set_swap_interval(void *data, unsigned swap_interval)
{
   xegl_ctx_data_t *xegl = (xegl_ctx_data_t*)data;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_EGL
         egl_set_swap_interval(&xegl->egl, swap_interval);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static gfx_ctx_proc_t gfx_ctx_xegl_get_proc_address(const char *symbol)
{
   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         return egl_get_proc_address(symbol);
#else
         break;
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return NULL;
}

static uint32_t gfx_ctx_xegl_get_flags(void *data)
{
   xegl_ctx_data_t *xegl = (xegl_ctx_data_t*)data;

   if ((xegl->egl.major * 1000 + xegl->egl.minor) >= 3001)
      return (1UL << GFX_CTX_FLAGS_GL_CORE_CONTEXT);
   return 1UL << GFX_CTX_FLAGS_NONE;
}

const gfx_ctx_driver_t gfx_ctx_x_egl =
{
   gfx_ctx_xegl_init,
   gfx_ctx_xegl_destroy,
   gfx_ctx_xegl_bind_api,
   gfx_ctx_xegl_set_swap_interval, 
   gfx_ctx_xegl_set_video_mode,
   x11_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   x11_get_metrics,
   NULL,
   x11_update_window_title,
   x11_check_window,
   gfx_ctx_xegl_set_resize,
   gfx_ctx_xegl_has_focus,
   gfx_ctx_xegl_suppress_screensaver,
   gfx_ctx_xegl_has_windowed,
   gfx_ctx_xegl_swap_buffers,
   gfx_ctx_xegl_input_driver,
   gfx_ctx_xegl_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_xegl_show_mouse,
   "x-egl",
   gfx_ctx_xegl_get_flags,
   gfx_ctx_xegl_bind_hw_render
};
