/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../../driver.h"
#include "../video_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"
#include "x11_common.h"

#include <signal.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>


static int (*g_pglSwapInterval)(int);
static void (*g_pglSwapIntervalEXT)(Display*, GLXDrawable, int);

typedef struct gfx_ctx_glx_data
{
   bool g_has_focus;
   bool g_true_full;
   bool g_use_hw_ctx;
   bool g_core;
   bool g_debug;
   bool g_should_reset_mode;
   bool g_is_double;

   Display *g_dpy;
   Window   g_win;
   GLXWindow g_glx_win;
   Colormap g_cmap;

   unsigned g_screen;
   unsigned g_interval;

   XIM g_xim;
   XIC g_xic;

   GLXContext g_ctx, g_hw_ctx;
   GLXFBConfig g_fbc;

   XF86VidModeModeInfo g_desktop_mode;

} gfx_ctx_glx_data_t;

static Atom g_quit_atom;
static volatile sig_atomic_t g_quit;
static unsigned g_major;
static unsigned g_minor;

static PFNGLXCREATECONTEXTATTRIBSARBPROC glx_create_context_attribs;

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static Bool glx_wait_notify(Display *d, XEvent *e, char *arg)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   (void)d;
   (void)e;

   if (!glx)
      return false;
   return (e->type == MapNotify) && (e->xmap.window == glx->g_win);
}

static int nul_handler(Display *dpy, XErrorEvent *event)
{
   (void)dpy;
   (void)event;
   return 0;
}

static void gfx_ctx_glx_get_video_size(void *data,
      unsigned *width, unsigned *height);

static void gfx_ctx_glx_destroy(void *data);

static void gfx_ctx_glx_swap_interval(void *data, unsigned interval)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   glx->g_interval = interval;

   if (g_pglSwapIntervalEXT)
   {
      RARCH_LOG("[GLX]: glXSwapIntervalEXT(%u)\n", glx->g_interval);
      g_pglSwapIntervalEXT(glx->g_dpy, glx->g_glx_win, glx->g_interval);
   }
   else if (g_pglSwapInterval)
   {
      RARCH_LOG("[GLX]: glXSwapInterval(%u)\n", glx->g_interval);
      if (g_pglSwapInterval(glx->g_interval) != 0)
         RARCH_WARN("[GLX]: glXSwapInterval() failed.\n");
   }
}

static void gfx_ctx_glx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   XEvent event;
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;
   unsigned new_width = *width, new_height = *height;

   (void)frame_count;

   gfx_ctx_glx_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *resize = true;
      *width  = new_width;
      *height = new_height;
   }

   while (XPending(glx->g_dpy))
   {
      bool filter;
      XNextEvent(glx->g_dpy, &event);
      filter = XFilterEvent(&event, glx->g_win);

      switch (event.type)
      {
         case ClientMessage:
            if (event.xclient.window == glx->g_win &&
                  (Atom)event.xclient.data.l[0] == g_quit_atom)
               g_quit = true;
            break;

         case DestroyNotify:
            if (event.xdestroywindow.window == glx->g_win)
               g_quit = true;
            break;

         case MapNotify:
            if (event.xmap.window == glx->g_win)
               glx->g_has_focus = true;
            break;

         case UnmapNotify:
            if (event.xunmap.window == glx->g_win)
               glx->g_has_focus = false;
            break;

         case KeyPress:
         case KeyRelease:
            x11_handle_key_event(&event, glx->g_xic, filter);
            break;
      }
   }

   *quit = g_quit;
}

static void gfx_ctx_glx_swap_buffers(void *data)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;
   (void)data;

   if (glx->g_is_double)
      glXSwapBuffers(glx->g_dpy, glx->g_glx_win);
}

static void gfx_ctx_glx_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_glx_update_window_title(void *data)
{
   char buf[128], buf_fps[128];
   gfx_ctx_glx_data_t *glx = NULL;
   bool fps_draw = g_settings.fps_show || g_settings.fps_monitor_enable;

   (void)data;

   glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   if (gfx_get_fps(buf, sizeof(buf), g_settings.fps_show ? buf_fps : NULL, sizeof(buf_fps)))
      XStoreName(glx->g_dpy, glx->g_win, buf);
   if (g_settings.fps_show)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static void gfx_ctx_glx_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   if (!glx)
      return;

   (void)data;

   if (!glx->g_dpy || glx->g_win == None)
   {
      Display *dpy = (Display*)XOpenDisplay(NULL);
      *width  = 0;
      *height = 0;

      if (dpy)
      {
         int screen = DefaultScreen(dpy);
         *width  = DisplayWidth(dpy, screen);
         *height = DisplayHeight(dpy, screen);
         XCloseDisplay(dpy);
      }
   }
   else
   {
      XWindowAttributes target;
      XGetWindowAttributes(glx->g_dpy, glx->g_win, &target);

      *width  = target.width;
      *height = target.height;
   }
}

static void ctx_glx_destroy_resources(gfx_ctx_glx_data_t *glx)
{
   if (!glx)
      return;

   x11_destroy_input_context(&glx->g_xim, &glx->g_xic);

   if (glx->g_dpy && glx->g_ctx)
   {
      glFinish();
      glXMakeContextCurrent(glx->g_dpy, None, None, NULL);
      if (!driver.video_cache_context)
      {
         if (glx->g_hw_ctx)
            glXDestroyContext(glx->g_dpy, glx->g_hw_ctx);
         glXDestroyContext(glx->g_dpy, glx->g_ctx);
         glx->g_ctx = NULL;
         glx->g_hw_ctx = NULL;
      }
   }

   if (glx->g_win)
   {
      glXDestroyWindow(glx->g_dpy, glx->g_glx_win);
      glx->g_glx_win = 0;

      /* Save last used monitor for later. */
#ifdef HAVE_XINERAMA
      XWindowAttributes target;
      Window child;

      int x = 0, y = 0;
      XGetWindowAttributes(glx->g_dpy, glx->g_win, &target);
      XTranslateCoordinates(glx->g_dpy, glx->g_win, DefaultRootWindow(glx->g_dpy),
            target.x, target.y, &x, &y, &child);

      glx->g_screen = x11_get_xinerama_monitor(glx->g_dpy, x, y,
            target.width, target.height);

      RARCH_LOG("[GLX]: Saved monitor #%u.\n", glx->g_screen);
#endif

      XUnmapWindow(glx->g_dpy, glx->g_win);
      XDestroyWindow(glx->g_dpy, glx->g_win);
      glx->g_win = None;
   }

   if (glx->g_cmap)
   {
      XFreeColormap(glx->g_dpy, glx->g_cmap);
      glx->g_cmap = None;
   }

   if (glx->g_should_reset_mode)
   {
      x11_exit_fullscreen(glx->g_dpy, &glx->g_desktop_mode);
      glx->g_should_reset_mode = false;
   }

   if (!driver.video_cache_context && glx->g_dpy)
   {
      XCloseDisplay(glx->g_dpy);
      glx->g_dpy = NULL;
   }

   g_pglSwapInterval = NULL;
   g_pglSwapIntervalEXT = NULL;
   g_major = g_minor = 0;
   glx->g_core = false;
}

static bool gfx_ctx_glx_init(void *data)
{
   static const int visual_attribs[] = {
      GLX_X_RENDERABLE     , True,
      GLX_DRAWABLE_TYPE    , GLX_WINDOW_BIT,
      GLX_RENDER_TYPE      , GLX_RGBA_BIT,
      GLX_DOUBLEBUFFER     , True,
      GLX_RED_SIZE         , 8,
      GLX_GREEN_SIZE       , 8,
      GLX_BLUE_SIZE        , 8,
      GLX_ALPHA_SIZE       , 8,
      GLX_DEPTH_SIZE       , 0,
      GLX_STENCIL_SIZE     , 0,
      None
   };
   int nelements, major, minor;
   GLXFBConfig *fbcs = NULL;
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)calloc(1, sizeof(gfx_ctx_glx_data_t));

   if (!glx)
      return false;

   XInitThreads();

   g_quit = 0;

   if (!glx->g_dpy)
      glx->g_dpy = XOpenDisplay(NULL);

   if (!glx->g_dpy)
      goto error;

   glXQueryVersion(glx->g_dpy, &major, &minor);

   /* GLX 1.3+ minimum required. */
   if ((major * 1000 + minor) < 1003)
      goto error;

   glx_create_context_attribs = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

#ifdef GL_DEBUG
   glx->g_debug = true;
#else
   glx->g_debug = g_extern.system.hw_render_callback.debug_context;
#endif

   glx->g_core = (g_major * 1000 + g_minor) >= 3001; /* Have to use ContextAttribs */
   if ((glx->g_core || glx->g_debug) && !glx_create_context_attribs)
      goto error;

   fbcs = glXChooseFBConfig(glx->g_dpy, DefaultScreen(glx->g_dpy),
         visual_attribs, &nelements);

   if (!fbcs)
      goto error;

   if (!nelements)
   {
      XFree(fbcs);
      goto error;
   }

   glx->g_fbc = fbcs[0];
   XFree(fbcs);

   driver.video_context_data = glx;

   return true;

error:
   ctx_glx_destroy_resources(glx);

   if (glx)
      free(glx);

   return false;
}


static bool gfx_ctx_glx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   XEvent event;
   bool true_full = false, windowed_full;
   int val, x_off = 0, y_off = 0;
   XVisualInfo *vi = NULL;
   XSetWindowAttributes swa = {0};
   int (*old_handler)(Display*, XErrorEvent*) = NULL;
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;
   struct sigaction sa = {{0}};

   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   if (!glx)
      return false;

   windowed_full = g_settings.video.windowed_fullscreen;
   true_full = false;

   vi = glXGetVisualFromFBConfig(glx->g_dpy, glx->g_fbc);
   if (!vi)
      goto error;

   swa.colormap = glx->g_cmap = XCreateColormap(glx->g_dpy,
         RootWindow(glx->g_dpy, vi->screen), vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask;
   swa.override_redirect = fullscreen ? True : False;

   if (fullscreen && !windowed_full)
   {
      if (x11_enter_fullscreen(glx->g_dpy, width, height, &glx->g_desktop_mode))
      {
         glx->g_should_reset_mode = true;
         true_full = true;
      }
      else
         RARCH_ERR("[GLX]: Entering true fullscreen failed. Will attempt windowed mode.\n");
   }

   if (g_settings.video.monitor_index)
      glx->g_screen = g_settings.video.monitor_index - 1;

#ifdef HAVE_XINERAMA
   if (fullscreen || glx->g_screen != 0)
   {
      unsigned new_width  = width;
      unsigned new_height = height;

      if (x11_get_xinerama_coord(glx->g_dpy, glx->g_screen,
               &x_off, &y_off, &new_width, &new_height))
         RARCH_LOG("[GLX]: Using Xinerama on screen #%u.\n", glx->g_screen);
      else
         RARCH_LOG("[GLX]: Xinerama is not active on screen.\n");

      if (fullscreen)
      {
         width  = new_width;
         height = new_height;
      }
   }
#endif

   RARCH_LOG("[GLX]: X = %d, Y = %d, W = %u, H = %u.\n",
         x_off, y_off, width, height);

   glx->g_win = XCreateWindow(glx->g_dpy, RootWindow(glx->g_dpy, vi->screen),
         x_off, y_off, width, height, 0,
         vi->depth, InputOutput, vi->visual, 
         CWBorderPixel | CWColormap | CWEventMask | (true_full ? CWOverrideRedirect : 0), &swa);
   XSetWindowBackground(glx->g_dpy, glx->g_win, 0);

   glx->g_glx_win = glXCreateWindow(glx->g_dpy, glx->g_fbc, glx->g_win, 0);

   x11_set_window_attr(glx->g_dpy, glx->g_win);

   if (fullscreen)
      x11_show_mouse(glx->g_dpy, glx->g_win, false);

   if (true_full)
   {
      RARCH_LOG("[GLX]: Using true fullscreen.\n");
      XMapRaised(glx->g_dpy, glx->g_win);
   }
   else if (fullscreen) /* We attempted true fullscreen, but failed. Attempt using windowed fullscreen. */
   {
      XMapRaised(glx->g_dpy, glx->g_win);
      RARCH_LOG("[GLX]: Using windowed fullscreen.\n");
      /* We have to move the window to the screen we want to go fullscreen on first.
       * x_off and y_off usually get ignored in XCreateWindow().
       */
      x11_move_window(glx->g_dpy, glx->g_win, x_off, y_off, width, height);
      x11_windowed_fullscreen(glx->g_dpy, glx->g_win);
   }
   else
   {
      XMapWindow(glx->g_dpy, glx->g_win);
      // If we want to map the window on a different screen, we'll have to do it by force.
      // Otherwise, we should try to let the window manager sort it out.
      // x_off and y_off usually get ignored in XCreateWindow().
      if (glx->g_screen)
         x11_move_window(glx->g_dpy, glx->g_win, x_off, y_off, width, height);
   }

   XIfEvent(glx->g_dpy, &event, glx_wait_notify, NULL);

   if (!glx->g_ctx)
   {
      if (glx->g_core || glx->g_debug)
      {
         int attribs[16];
         int *aptr = attribs;

         if (glx->g_core)
         {
            *aptr++ = GLX_CONTEXT_MAJOR_VERSION_ARB;
            *aptr++ = g_major;
            *aptr++ = GLX_CONTEXT_MINOR_VERSION_ARB;
            *aptr++ = g_minor;

            /* Technically, we don't have core/compat until 3.2.
             * Version 3.1 is either compat or not depending on GL_ARB_compatibility.
             */
            if ((g_major * 1000 + g_minor) >= 3002)
            {
               *aptr++ = GLX_CONTEXT_PROFILE_MASK_ARB;
               *aptr++ = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
            }
         }

         if (glx->g_debug)
         {
            *aptr++ = GLX_CONTEXT_FLAGS_ARB;
            *aptr++ = GLX_CONTEXT_DEBUG_BIT_ARB;
         }

         *aptr = None;
         glx->g_ctx = glx_create_context_attribs(glx->g_dpy, glx->g_fbc, NULL, True, attribs);
         if (glx->g_use_hw_ctx)
         {
            RARCH_LOG("[GLX]: Creating shared HW context.\n");
            glx->g_hw_ctx = glx_create_context_attribs(glx->g_dpy, glx->g_fbc, glx->g_ctx, True, attribs);
            if (!glx->g_hw_ctx)
               RARCH_ERR("[GLX]: Failed to create new shared context.\n");
         }
      }
      else
      {
         glx->g_ctx = glXCreateNewContext(glx->g_dpy, glx->g_fbc, GLX_RGBA_TYPE, 0, True);
         if (glx->g_use_hw_ctx)
         {
            glx->g_hw_ctx = glXCreateNewContext(glx->g_dpy, glx->g_fbc, GLX_RGBA_TYPE, glx->g_ctx, True);
            if (!glx->g_hw_ctx)
               RARCH_ERR("[GLX]: Failed to create new shared context.\n");
         }
      }

      if (!glx->g_ctx)
      {
         RARCH_ERR("[GLX]: Failed to create new context.\n");
         goto error;
      }
   }
   else
   {
      driver.video_cache_context_ack = true;
      RARCH_LOG("[GLX]: Using cached GL context.\n");
   }

   glXMakeContextCurrent(glx->g_dpy, glx->g_glx_win, glx->g_glx_win, glx->g_ctx);
   XSync(glx->g_dpy, False);

   g_quit_atom = XInternAtom(glx->g_dpy, "WM_DELETE_WINDOW", False);
   if (g_quit_atom)
      XSetWMProtocols(glx->g_dpy, glx->g_win, &g_quit_atom, 1);

   glXGetConfig(glx->g_dpy, vi, GLX_DOUBLEBUFFER, &val);
   glx->g_is_double = val;

   if (glx->g_is_double)
   {
      const char *swap_func = NULL;

      g_pglSwapIntervalEXT = (void (*)(Display*, GLXDrawable, int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
      g_pglSwapInterval = (int (*)(int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");

      if (g_pglSwapIntervalEXT)
         swap_func = "glXSwapIntervalEXT";
      else if (g_pglSwapInterval)
         swap_func = "glXSwapIntervalMESA";

      if (!g_pglSwapInterval && !g_pglSwapIntervalEXT)
         RARCH_WARN("[GLX]: Cannot find swap interval call.\n");
      else
         RARCH_LOG("[GLX]: Found swap function: %s.\n", swap_func);
   }
   else
      RARCH_WARN("[GLX]: Context is not double buffered!.\n");

   gfx_ctx_glx_swap_interval(data, glx->g_interval);

   /* This can blow up on some drivers. It's not fatal, so override errors for this call. */
   old_handler = XSetErrorHandler(nul_handler);
   XSetInputFocus(glx->g_dpy, glx->g_win, RevertToNone, CurrentTime);
   XSync(glx->g_dpy, False);
   XSetErrorHandler(old_handler);

   XFree(vi);
   glx->g_has_focus = true;

   if (!x11_create_input_context(glx->g_dpy, glx->g_win, &glx->g_xim, &glx->g_xic))
      goto error;

   driver.display_type  = RARCH_DISPLAY_X11;
   driver.video_display = (uintptr_t)glx->g_dpy;
   driver.video_window  = (uintptr_t)glx->g_win;
   glx->g_true_full = true_full;

   return true;

error:
   if (vi)
      XFree(vi);

   ctx_glx_destroy_resources(glx);

   if (glx)
      free(glx);

   return false;
}

static void gfx_ctx_glx_destroy(void *data)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   if (!glx)
      return;
   
   (void)data;

   ctx_glx_destroy_resources(glx);

   if (driver.video_context_data)
      free(driver.video_context_data);
   driver.video_context_data = NULL;
}

static void gfx_ctx_glx_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   void *xinput = input_x.init();

   (void)data;

   *input       = xinput ? &input_x : NULL;
   *input_data  = xinput;
}

static bool gfx_ctx_glx_has_focus(void *data)
{
   Window win;
   int rev;
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   (void)data;

   XGetInputFocus(glx->g_dpy, &win, &rev);

   return (win == glx->g_win && glx->g_has_focus) || glx->g_true_full;
}

static bool gfx_ctx_glx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static gfx_ctx_proc_t gfx_ctx_glx_get_proc_address(const char *symbol)
{
   return glXGetProcAddress((const GLubyte*)symbol);
}

static bool gfx_ctx_glx_bind_api(void *data, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   (void)data;

   g_major = major;
   g_minor = minor;

   return api == GFX_CTX_OPENGL_API;
}

static void gfx_ctx_glx_show_mouse(void *data, bool state)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   (void)data;

   x11_show_mouse(glx->g_dpy, glx->g_win, state);
}

static void gfx_ctx_glx_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)driver.video_context_data;

   if (!glx)
      return;

   (void)data;

   glx->g_use_hw_ctx = enable;

   if (!glx->g_dpy)
      return;
   if (!glx->g_glx_win)
      return;

   glXMakeContextCurrent(glx->g_dpy, glx->g_glx_win,
         glx->g_glx_win, enable ? glx->g_hw_ctx : glx->g_ctx);
}

const gfx_ctx_driver_t gfx_ctx_glx = {
   gfx_ctx_glx_init,
   gfx_ctx_glx_destroy,
   gfx_ctx_glx_bind_api,
   gfx_ctx_glx_swap_interval,
   gfx_ctx_glx_set_video_mode,
   gfx_ctx_glx_get_video_size,
   NULL,
   gfx_ctx_glx_update_window_title,
   gfx_ctx_glx_check_window,
   gfx_ctx_glx_set_resize,
   gfx_ctx_glx_has_focus,
   gfx_ctx_glx_has_windowed,
   gfx_ctx_glx_swap_buffers,
   gfx_ctx_glx_input_driver,
   gfx_ctx_glx_get_proc_address,
#ifdef HAVE_EGL
   NULL,
   NULL,
#endif
   gfx_ctx_glx_show_mouse,
   "glx",

   gfx_ctx_glx_bind_hw_render,
};

