/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

// GLX context.

#include "../../driver.h"
#include "../gfx_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"
#include "x11_common.h"

#include <signal.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

static Display *g_dpy;
static Window   g_win;
static GLXWindow g_glx_win;
static Colormap g_cmap;
static Atom g_quit_atom;
static bool g_has_focus;
static bool g_true_full;
static unsigned g_screen;

static GLXContext g_ctx;
static GLXFBConfig g_fbc;
static unsigned g_major;
static unsigned g_minor;
static bool g_core;
static bool g_debug;

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*,
      GLXFBConfig, GLXContext, Bool, const int*);
static glXCreateContextAttribsARBProc glx_create_context_attribs;

static XF86VidModeModeInfo g_desktop_mode;
static bool g_should_reset_mode;

static volatile sig_atomic_t g_quit;
static bool g_inited;
static unsigned g_interval;
static bool g_is_double;

static int (*g_pglSwapInterval)(int);
static void (*g_pglSwapIntervalEXT)(Display*, GLXDrawable, int);

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static Bool glx_wait_notify(Display *d, XEvent *e, char *arg)
{
   (void)d;
   (void)e;
   return e->type == MapNotify && e->xmap.window == g_win;
}

static int nul_handler(Display *dpy, XErrorEvent *event)
{
   (void)dpy;
   (void)event;
   return 0;
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height);
static void gfx_ctx_destroy(void);

static void gfx_ctx_swap_interval(unsigned interval)
{
   g_interval = interval;

   if (g_pglSwapIntervalEXT)
   {
      RARCH_LOG("[GLX]: glXSwapIntervalEXT(%u)\n", g_interval);
      g_pglSwapIntervalEXT(g_dpy, g_glx_win, g_interval);
   }
   else if (g_pglSwapInterval)
   {
      RARCH_LOG("[GLX]: glXSwapInterval(%u)\n", g_interval);
      if (g_pglSwapInterval(g_interval) != 0)
         RARCH_WARN("[GLX]: glXSwapInterval() failed.\n");
   }
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   unsigned new_width = *width, new_height = *height;
   gfx_ctx_get_video_size(&new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *resize = true;
      *width  = new_width;
      *height = new_height;
   }

   XEvent event;
   while (XPending(g_dpy))
   {
      XNextEvent(g_dpy, &event);
      switch (event.type)
      {
         case ClientMessage:
            if (event.xclient.window == g_win &&
                  (Atom)event.xclient.data.l[0] == g_quit_atom)
               g_quit = true;
            break;

         case DestroyNotify:
            if (event.xdestroywindow.window == g_win)
               g_quit = true;
            break;

         case MapNotify:
            if (event.xmap.window == g_win)
               g_has_focus = true;
            break;

         case UnmapNotify:
            if (event.xunmap.window == g_win)
               g_has_focus = false;
            break;

         case KeyPress:
         case KeyRelease:
            x11_handle_key_event(&event);
            break;
      }
   }

   *quit = g_quit;
}

static void gfx_ctx_swap_buffers(void)
{
   if (g_is_double)
      glXSwapBuffers(g_dpy, g_glx_win);
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(void)
{
   char buf[128], buf_fps[128];
   if (gfx_get_fps(buf, sizeof(buf), true, buf_fps, sizeof(buf_fps)))
      XStoreName(g_dpy, g_win, buf);

   if ((g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)))
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   if (!g_dpy || g_win == None)
   {
      Display *dpy = XOpenDisplay(NULL);
      if (dpy)
      {
         int screen = DefaultScreen(dpy);
         *width  = DisplayWidth(dpy, screen);
         *height = DisplayHeight(dpy, screen);
         XCloseDisplay(dpy);
      }
      else
      {
         *width  = 0;
         *height = 0;
      }
   }
   else
   {
      XWindowAttributes target;
      XGetWindowAttributes(g_dpy, g_win, &target);

      *width  = target.width;
      *height = target.height;
   }
}

static bool gfx_ctx_init(void)
{
   if (g_inited)
      return false;

   XInitThreads();

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

   GLXFBConfig *fbcs = NULL;
   g_quit = 0;

   if (!g_dpy)
      g_dpy = XOpenDisplay(NULL);

   if (!g_dpy)
      goto error;

   int major, minor;
   glXQueryVersion(g_dpy, &major, &minor);

   // GLX 1.3+ minimum required.
   if ((major * 1000 + minor) < 1003)
      goto error;

   glx_create_context_attribs = (glXCreateContextAttribsARBProc)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

#ifdef GL_DEBUG
   g_debug = true;
#else
   g_debug = g_extern.system.hw_render_callback.debug_context;
#endif

   g_core = (g_major * 1000 + g_minor) >= 3001; // Have to use ContextAttribs
   if ((g_core || g_debug) && !glx_create_context_attribs)
      goto error;

   int nelements;
   fbcs = glXChooseFBConfig(g_dpy, DefaultScreen(g_dpy),
         visual_attribs, &nelements);

   if (!fbcs)
      goto error;

   if (!nelements)
   {
      XFree(fbcs);
      goto error;
   }

   g_fbc = fbcs[0];
   XFree(fbcs);

   return true;

error:
   gfx_ctx_destroy();
   return false;
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   struct sigaction sa = {{0}};
   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
   int x_off = 0;
   int y_off = 0;

   bool windowed_full = g_settings.video.windowed_fullscreen;
   bool true_full = false;
   int (*old_handler)(Display*, XErrorEvent*) = NULL;

   XSetWindowAttributes swa = {0};

   XVisualInfo *vi = glXGetVisualFromFBConfig(g_dpy, g_fbc);
   if (!vi)
      goto error;

   swa.colormap = g_cmap = XCreateColormap(g_dpy, RootWindow(g_dpy, vi->screen),
         vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask;
   swa.override_redirect = fullscreen ? True : False;

   if (fullscreen && !windowed_full)
   {
      if (x11_enter_fullscreen(g_dpy, width, height, &g_desktop_mode))
      {
         g_should_reset_mode = true;
         true_full = true;
      }
      else
         RARCH_ERR("[GLX]: Entering true fullscreen failed. Will attempt windowed mode.\n");
   }

   if (g_settings.video.monitor_index)
      g_screen = g_settings.video.monitor_index - 1;

#ifdef HAVE_XINERAMA
   if (fullscreen || g_screen != 0)
   {
      unsigned new_width  = width;
      unsigned new_height = height;
      if (x11_get_xinerama_coord(g_dpy, g_screen, &x_off, &y_off, &new_width, &new_height))
         RARCH_LOG("[GLX]: Using Xinerama on screen #%u.\n", g_screen);
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

   g_win = XCreateWindow(g_dpy, RootWindow(g_dpy, vi->screen),
         x_off, y_off, width, height, 0,
         vi->depth, InputOutput, vi->visual, 
         CWBorderPixel | CWColormap | CWEventMask | (true_full ? CWOverrideRedirect : 0), &swa);
   XSetWindowBackground(g_dpy, g_win, 0);

   g_glx_win = glXCreateWindow(g_dpy, g_fbc, g_win, 0);

   x11_set_window_attr(g_dpy, g_win);

   if (fullscreen)
      x11_show_mouse(g_dpy, g_win, false);

   if (true_full)
   {
      RARCH_LOG("[GLX]: Using true fullscreen.\n");
      XMapRaised(g_dpy, g_win);
   }
   else if (fullscreen) // We attempted true fullscreen, but failed. Attempt using windowed fullscreen.
   {
      XMapRaised(g_dpy, g_win);
      RARCH_LOG("[GLX]: Using windowed fullscreen.\n");
      // We have to move the window to the screen we want to go fullscreen on first.
      // x_off and y_off usually get ignored in XCreateWindow().
      x11_move_window(g_dpy, g_win, x_off, y_off, width, height);
      x11_windowed_fullscreen(g_dpy, g_win);
   }
   else
   {
      XMapWindow(g_dpy, g_win);
      // If we want to map the window on a different screen, we'll have to do it by force.
      // Otherwise, we should try to let the window manager sort it out.
      // x_off and y_off usually get ignored in XCreateWindow().
      if (g_screen)
         x11_move_window(g_dpy, g_win, x_off, y_off, width, height);
   }

   XEvent event;
   XIfEvent(g_dpy, &event, glx_wait_notify, NULL);

   if (!g_ctx)
   {
      if (g_core || g_debug)
      {
         int attribs[16];
         int *aptr = attribs;

         if (g_core)
         {
            *aptr++ = GLX_CONTEXT_MAJOR_VERSION_ARB;
            *aptr++ = g_major;
            *aptr++ = GLX_CONTEXT_MINOR_VERSION_ARB;
            *aptr++ = g_minor;
            *aptr++ = GLX_CONTEXT_PROFILE_MASK_ARB;
            *aptr++ = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
         }

         if (g_debug)
         {
            *aptr++ = GLX_CONTEXT_FLAGS_ARB;
            *aptr++ = GLX_CONTEXT_DEBUG_BIT_ARB;
         }

         *aptr = None;
         g_ctx = glx_create_context_attribs(g_dpy, g_fbc, NULL, True, attribs);
      }
      else
         g_ctx = glXCreateNewContext(g_dpy, g_fbc, GLX_RGBA_TYPE, 0, True);

      if (!g_ctx)
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

   glXMakeContextCurrent(g_dpy, g_glx_win, g_glx_win, g_ctx);
   XSync(g_dpy, False);

   g_quit_atom = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
   if (g_quit_atom)
      XSetWMProtocols(g_dpy, g_win, &g_quit_atom, 1);

   int val;
   glXGetConfig(g_dpy, vi, GLX_DOUBLEBUFFER, &val);
   g_is_double = val;
   if (g_is_double)
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

   gfx_ctx_swap_interval(g_interval);

   // This can blow up on some drivers. It's not fatal, so override errors for this call.
   old_handler = XSetErrorHandler(nul_handler);
   XSetInputFocus(g_dpy, g_win, RevertToNone, CurrentTime);
   XSync(g_dpy, False);
   XSetErrorHandler(old_handler);

   XFree(vi);
   g_has_focus = true;
   g_inited    = true;

   driver.display_type  = RARCH_DISPLAY_X11;
   driver.video_display = (uintptr_t)g_dpy;
   driver.video_window  = (uintptr_t)g_win;
   g_true_full = true_full;

   return true;

error:
   if (vi)
      XFree(vi);

   gfx_ctx_destroy();
   return false;
}

static void gfx_ctx_destroy(void)
{
   if (g_dpy && g_ctx)
   {
      glFinish();
      glXMakeContextCurrent(g_dpy, None, None, NULL);
      if (!driver.video_cache_context)
      {
         glXDestroyContext(g_dpy, g_ctx);
         g_ctx = NULL;
      }
   }

   if (g_win)
   {
      glXDestroyWindow(g_dpy, g_glx_win);
      g_glx_win = 0;

      // Save last used monitor for later.
#ifdef HAVE_XINERAMA
      XWindowAttributes target;
      Window child;

      int x = 0, y = 0;
      XGetWindowAttributes(g_dpy, g_win, &target);
      XTranslateCoordinates(g_dpy, g_win, DefaultRootWindow(g_dpy),
            target.x, target.y, &x, &y, &child);

      g_screen = x11_get_xinerama_monitor(g_dpy, x, y,
            target.width, target.height);

      RARCH_LOG("[GLX]: Saved monitor #%u.\n", g_screen);
#endif

      XUnmapWindow(g_dpy, g_win);
      XDestroyWindow(g_dpy, g_win);
      g_win = None;
   }

   if (g_cmap)
   {
      XFreeColormap(g_dpy, g_cmap);
      g_cmap = None;
   }

   if (g_should_reset_mode)
   {
      x11_exit_fullscreen(g_dpy, &g_desktop_mode);
      g_should_reset_mode = false;
   }

   if (!driver.video_cache_context && g_dpy)
   {
      XCloseDisplay(g_dpy);
      g_dpy = NULL;
   }

   g_inited = false;
   g_pglSwapInterval = NULL;
   g_pglSwapIntervalEXT = NULL;
   g_major = g_minor = 0;
   g_core = false;
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   void *xinput = input_x.init();
   *input       = xinput ? &input_x : NULL;
   *input_data  = xinput;
}

static bool gfx_ctx_has_focus(void)
{
   if (!g_inited)
      return false;

   Window win;
   int rev;
   XGetInputFocus(g_dpy, &win, &rev);

   return (win == g_win && g_has_focus) || g_true_full;
}

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   return glXGetProcAddress((const GLubyte*)symbol);
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   g_major = major;
   g_minor = minor;
   return api == GFX_CTX_OPENGL_API;
}

#ifdef HAVE_EGL
static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}
#endif

static void gfx_ctx_show_mouse(bool state)
{
   x11_show_mouse(g_dpy, g_win, state);
}

const gfx_ctx_driver_t gfx_ctx_glx = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   NULL,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
#ifdef HAVE_EGL
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
#endif
   gfx_ctx_show_mouse,
   "glx",
};

