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

// X/EGL context. Mostly used for testing GLES code paths.
// Should be its own file as it has lots of X11 stuff baked into it as well.

#include "../../driver.h"
#include "../gfx_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"
#include "../../input/x11_input.h"

#include <signal.h>
#include <stdint.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *g_dpy;
static Window   g_win;
static Colormap g_cmap;
static Atom g_quit_atom;
static bool g_has_focus;

static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;

static volatile sig_atomic_t g_quit;
static bool g_inited;
static unsigned g_interval;

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static void hide_mouse(void)
{
   Cursor no_ptr;
   Pixmap bm_no;
   XColor black, dummy;
   Colormap colormap;

   static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};

   colormap = DefaultColormap(g_dpy, DefaultScreen(g_dpy));
   if (!XAllocNamedColor(g_dpy, colormap, "black", &black, &dummy))
      return;

   bm_no  = XCreateBitmapFromData(g_dpy, g_win, bm_no_data, 8, 8);
   no_ptr = XCreatePixmapCursor(g_dpy, bm_no, bm_no, &black, &black, 0, 0);

   XDefineCursor(g_dpy, g_win, no_ptr);
   XFreeCursor(g_dpy, no_ptr);

   if (bm_no != None)
      XFreePixmap(g_dpy, bm_no);

   XFreeColors(g_dpy, colormap, &black.pixel, 1, 0);
}

static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
#define XA_INIT(x) XA##x = XInternAtom(g_dpy, #x, False)
#define _NET_WM_STATE_ADD 1
static void set_windowed_fullscreen(void)
{
   XA_INIT(_NET_WM_STATE);
   XA_INIT(_NET_WM_STATE_FULLSCREEN);

   if (!XA_NET_WM_STATE || !XA_NET_WM_STATE_FULLSCREEN)
   {
      RARCH_ERR("[X/EGL]: Cannot set windowed fullscreen.\n");
      return;
   }

   XEvent xev;

   xev.xclient.type = ClientMessage;
   xev.xclient.serial = 0;
   xev.xclient.send_event = True;
   xev.xclient.message_type = XA_NET_WM_STATE;
   xev.xclient.window = g_win;
   xev.xclient.format = 32;
   xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
   xev.xclient.data.l[1] = XA_NET_WM_STATE_FULLSCREEN;
   xev.xclient.data.l[2] = 0;
   xev.xclient.data.l[3] = 0;
   xev.xclient.data.l[4] = 0;

   XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False,
         SubstructureRedirectMask | SubstructureNotifyMask,
         &xev);
}

void gfx_ctx_set_swap_interval(unsigned interval, bool inited)
{
   g_interval = interval;
   if (inited)
   {
      RARCH_LOG("[X/EGL]: eglSwapInterval(%u)\n", g_interval);
      if (!eglSwapInterval(g_egl_dpy, g_interval))
         RARCH_ERR("[X/EGL]: eglSwapInterval() failed.\n");
   }
}

void gfx_ctx_check_window(bool *quit,
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

void gfx_ctx_swap_buffers(void)
{
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

void gfx_ctx_update_window_title(bool reset)
{
   if (reset)
      gfx_window_title_reset();

   char buf[128];
   if (gfx_window_title(buf, sizeof(buf)))
      XStoreName(g_dpy, g_win, buf);
}

void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
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

bool gfx_ctx_init(void)
{
   if (g_inited)
      return false;

   const EGLint egl_attribs[] = {
      EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
      EGL_RED_SIZE,        1,
      EGL_GREEN_SIZE,      1,
      EGL_BLUE_SIZE,       1,
      EGL_DEPTH_SIZE,      1,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };

   g_quit = 0;

   g_dpy = XOpenDisplay(NULL);
   if (!g_dpy)
      goto error;

   g_egl_dpy = eglGetDisplay(g_dpy);
   if (!g_egl_dpy)
      goto error;

   EGLint egl_major, egl_minor;
   if (!eglInitialize(g_egl_dpy, &egl_major, &egl_minor))
      goto error;

   RARCH_LOG("[X/EGL]: EGL version: %d.%d\n", egl_major, egl_minor);

   EGLint num_configs;
   if (!eglChooseConfig(g_egl_dpy, egl_attribs, &g_config, 1, &num_configs)
         || num_configs == 0 || !g_config)
      goto error;

   return true;

error:
   gfx_ctx_destroy();
   return false;
}

bool gfx_ctx_set_video_mode(
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

   XVisualInfo temp = {0};
   XSetWindowAttributes swa = {0};
   XVisualInfo *vi = NULL;

   const EGLint egl_ctx_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE,
   };

   EGLint vid;
   if (!eglGetConfigAttrib(g_egl_dpy, g_config, EGL_NATIVE_VISUAL_ID, &vid))
      goto error;

   temp.visualid = vid;

   EGLint num_visuals;
   vi = XGetVisualInfo(g_dpy, VisualIDMask, &temp, &num_visuals);
   if (!vi)
      goto error;

   swa.colormap = g_cmap = XCreateColormap(g_dpy, RootWindow(g_dpy, vi->screen),
         vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask;

   g_win = XCreateWindow(g_dpy, RootWindow(g_dpy, vi->screen),
         0, 0, width ? width : 200, height ? height : 200, 0,
         vi->depth, InputOutput, vi->visual, 
         CWBorderPixel | CWColormap | CWEventMask, &swa);
   XSetWindowBackground(g_dpy, g_win, 0);

   g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, EGL_NO_CONTEXT, egl_ctx_attribs);
   if (!g_egl_ctx)
      goto error;

   g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, g_win, NULL);
   if (!g_egl_surf)
      goto error;

   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
      goto error;

   gfx_ctx_update_window_title(true);
   hide_mouse();
   XMapWindow(g_dpy, g_win);

   if (fullscreen)
      set_windowed_fullscreen();

   g_quit_atom = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
   if (g_quit_atom)
      XSetWMProtocols(g_dpy, g_win, &g_quit_atom, 1);

   gfx_suspend_screensaver(g_win);

   XFree(vi);
   g_has_focus = true;
   g_inited    = true;

   return true;

error:
   if (vi)
      XFree(vi);

   gfx_ctx_destroy();
   return false;
}

void gfx_ctx_destroy(void)
{
   if (g_egl_dpy)
   {
      if (g_egl_ctx)
      {
         eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(g_egl_dpy, g_egl_ctx);
      }

      if (g_egl_surf)
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      eglTerminate(g_egl_dpy);
   }

   g_egl_ctx  = NULL;
   g_egl_surf = NULL;
   g_egl_dpy  = NULL;
   g_config   = 0;

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

   if (g_dpy)
   {
      XCloseDisplay(g_dpy);
      g_dpy = NULL;
   }

   g_inited = false;
}

void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   void *xinput = input_x.init();
   *input       = xinput ? &input_x : NULL;
   *input_data  = xinput;

   if (xinput)
      x_input_set_disp_win((x11_input_t*)xinput, g_dpy, g_win);
}

void gfx_ctx_set_projection(gl_t *gl, const struct gl_ortho *ortho, bool allow_rotate)
{
   // Calculate projection.
   math_matrix proj;
   matrix_ortho(&proj, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (allow_rotate)
   {
      math_matrix rot;
      matrix_rotate_z(&rot, M_PI * gl->rotation / 180.0f);
      matrix_multiply(&proj, &rot, &proj);
   }

   gl->mvp = proj;
}

bool gfx_ctx_window_has_focus(void)
{
   if (!g_inited)
      return false;

   Window win;
   int rev;
   XGetInputFocus(g_dpy, &win, &rev);

   return win == g_win && g_has_focus;
}

gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   return eglGetProcAddress(symbol);
}

