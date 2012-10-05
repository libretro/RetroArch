/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
static Colormap g_cmap;
static Atom g_quit_atom;
static bool g_has_focus;

static GLXContext g_ctx;
static GLXFBConfig g_fbc;

static XF86VidModeModeInfo g_desktop_mode;
static bool g_should_reset_mode;

static volatile sig_atomic_t g_quit;
static bool g_inited;
static unsigned g_interval;
static bool g_is_double;

static int (*g_pglSwapInterval)(int);

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

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height);
static void gfx_ctx_destroy(void);

static void gfx_ctx_swap_interval(unsigned interval)
{
   g_interval = interval;
   if (g_pglSwapInterval)
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
            if ((Atom)event.xclient.data.l[0] == g_quit_atom)
               g_quit = true;
            break;

         case DestroyNotify:
            g_quit = true;
            break;

         case MapNotify:
            g_has_focus = true;
            break;

         case UnmapNotify:
            g_has_focus = false;
            break;
      }
   }

   *quit = g_quit;
}

static void gfx_ctx_swap_buffers(void)
{
   if (g_is_double)
      glXSwapBuffers(g_dpy, g_win);
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(bool reset)
{
   if (reset)
      gfx_window_title_reset();

   char buf[128];
   if (gfx_window_title(buf, sizeof(buf)))
      XStoreName(g_dpy, g_win, buf);
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

   g_dpy = XOpenDisplay(NULL);
   if (!g_dpy)
      goto error;

   // GLX 1.3+ required.
   int major, minor;
   glXQueryVersion(g_dpy, &major, &minor);
   if (major < 1 || (major == 1 && minor < 3))
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
      unsigned bits, bool fullscreen)
{
   (void)bits;

   struct sigaction sa = {{0}};
   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   bool windowed_full = g_settings.video.windowed_fullscreen;
   bool true_full = false;

   XSetWindowAttributes swa = {0};

   XVisualInfo *vi = glXGetVisualFromFBConfig(g_dpy, g_fbc);
   if (!vi)
      goto error;

   swa.colormap = g_cmap = XCreateColormap(g_dpy, RootWindow(g_dpy, vi->screen),
         vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask;
   swa.override_redirect = fullscreen ? True : False;

   if (fullscreen && !windowed_full)
   {
      if (x11_enter_fullscreen(g_dpy, width, height, &g_desktop_mode))
      {
         g_should_reset_mode = true;
         true_full = true;
      }
   }

   g_win = XCreateWindow(g_dpy, RootWindow(g_dpy, vi->screen),
         0, 0, width ? width : 200, height ? height : 200, 0,
         vi->depth, InputOutput, vi->visual, 
         CWBorderPixel | CWColormap | CWEventMask | (true_full ? CWOverrideRedirect : 0), &swa);
   XSetWindowBackground(g_dpy, g_win, 0);

   gfx_ctx_update_window_title(true);
   x11_hide_mouse(g_dpy, g_win);

   if (true_full)
   {
      XMapRaised(g_dpy, g_win);
      XGrabKeyboard(g_dpy, g_win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
   }
   else if (fullscreen) // We attempted true fullscreen, but failed. Attempt using windowed fullscreen.
   {
      XMapRaised(g_dpy, g_win);
      RARCH_WARN("[GLX]: Using windowed fullscreen.\n");
      x11_windowed_fullscreen(g_dpy, g_win);
   }
   else
      XMapWindow(g_dpy, g_win);

   XEvent event;
   XIfEvent(g_dpy, &event, glx_wait_notify, NULL);

   XSetInputFocus(g_dpy, g_win, RevertToNone, CurrentTime);

   g_ctx = glXCreateNewContext(g_dpy, g_fbc, GLX_RGBA_TYPE, 0, True);
   if (!g_ctx)
   {
      RARCH_ERR("[GLX]: Failed to create new context.\n");
      goto error;
   }

   glXMakeCurrent(g_dpy, g_win, g_ctx);
   XSync(g_dpy, False);

   g_quit_atom = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
   if (g_quit_atom)
      XSetWMProtocols(g_dpy, g_win, &g_quit_atom, 1);

   int val;
   glXGetConfig(g_dpy, vi, GLX_DOUBLEBUFFER, &val);
   g_is_double = val;
   if (g_is_double)
   {
      if (!g_pglSwapInterval)
         g_pglSwapInterval = (int (*)(int))glXGetProcAddress((const GLubyte*)"glXSwapInterval");
      if (!g_pglSwapInterval)
         g_pglSwapInterval = (int (*)(int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");
      if (!g_pglSwapInterval)
         g_pglSwapInterval = (int (*)(int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalSGI");
      if (!g_pglSwapInterval)
         RARCH_WARN("[GLX]: Cannot find swap interval call.\n");
   }
   else
      RARCH_WARN("[GLX]: Context is not double buffered!.\n");

   gfx_ctx_swap_interval(g_interval);

   XFree(vi);
   g_has_focus = true;
   g_inited    = true;

   driver.display_type  = RARCH_DISPLAY_X11;
   driver.video_display = (uintptr_t)g_dpy;
   driver.video_window  = (uintptr_t)g_win;

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
      glXMakeCurrent(g_dpy, None, NULL);
      glXDestroyContext(g_dpy, g_ctx);
      g_ctx = NULL;
   }

   if (g_win)
   {
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

   if (g_dpy)
   {
      XCloseDisplay(g_dpy);
      g_dpy = NULL;
   }

   g_inited = false;
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

   return win == g_win && g_has_focus;
}

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   return glXGetProcAddress((const GLubyte*)symbol);
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   return api == GFX_CTX_OPENGL_API;
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
   "glx",
};

