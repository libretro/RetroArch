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

#include <stdint.h>
#include <signal.h>

#include <GL/glx.h>

#include "../../driver.h"

#include "../common/gl_common.h"
#include "../common/x11_common.h"

static int (*g_pglSwapInterval)(int);
static int (*g_pglSwapIntervalSGI)(int);
static void (*g_pglSwapIntervalEXT)(Display*, GLXDrawable, int);

typedef struct gfx_ctx_glx_data
{
   bool g_use_hw_ctx;
   bool g_core_es;
   bool g_core_es_core;
   bool g_debug;
   bool g_should_reset_mode;
   bool g_is_double;

   GLXWindow g_glx_win;

   unsigned g_screen;
   unsigned g_interval;

   GLXContext g_ctx, g_hw_ctx;
   GLXFBConfig g_fbc;

   XF86VidModeModeInfo g_desktop_mode;

} gfx_ctx_glx_data_t;

static unsigned g_major;
static unsigned g_minor;

static PFNGLXCREATECONTEXTATTRIBSARBPROC glx_create_context_attribs;

static int glx_nul_handler(Display *dpy, XErrorEvent *event)
{
   (void)dpy;
   (void)event;
   return 0;
}

static void ctx_glx_destroy_resources(gfx_ctx_glx_data_t *glx)
{
   driver_t *driver = driver_get_ptr();

   if (!glx)
      return;

   x11_input_ctx_destroy();

   if (g_x11_dpy && glx->g_ctx)
   {

      glFinish();
      glXMakeContextCurrent(g_x11_dpy, None, None, NULL);

      if (!driver->video_cache_context)
      {
         if (glx->g_hw_ctx)
            glXDestroyContext(g_x11_dpy, glx->g_hw_ctx);
         glXDestroyContext(g_x11_dpy, glx->g_ctx);
         glx->g_ctx = NULL;
         glx->g_hw_ctx = NULL;
      }
   }

   if (g_x11_win)
   {
      glXDestroyWindow(g_x11_dpy, glx->g_glx_win);
      glx->g_glx_win = 0;

      /* Save last used monitor for later. */
#ifdef HAVE_XINERAMA
      {
         XWindowAttributes target;
         Window child;

         int x = 0, y = 0;
         XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);
         XTranslateCoordinates(g_x11_dpy, g_x11_win, DefaultRootWindow(g_x11_dpy),
               target.x, target.y, &x, &y, &child);

         glx->g_screen = x11_get_xinerama_monitor(g_x11_dpy, x, y,
               target.width, target.height);

         RARCH_LOG("[GLX]: Saved monitor #%u.\n", glx->g_screen);
      }
#endif

      x11_window_destroy(false);
   }

   x11_colormap_destroy();

   if (glx->g_should_reset_mode)
   {
      x11_exit_fullscreen(g_x11_dpy, &glx->g_desktop_mode);
      glx->g_should_reset_mode = false;
   }

   if (!driver->video_cache_context && g_x11_dpy)
   {
      XCloseDisplay(g_x11_dpy);
      g_x11_dpy = NULL;
   }

   g_pglSwapInterval = NULL;
   g_pglSwapIntervalSGI = NULL;
   g_pglSwapIntervalEXT = NULL;
   g_major = g_minor = 0;
   glx->g_core_es = false;
}

static void gfx_ctx_glx_destroy(void *data)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)
      gfx_ctx_data_get_ptr();

   if (!glx)
      return;
   
   (void)data;

   ctx_glx_destroy_resources(glx);
   gfx_ctx_free_data();
}

static void gfx_ctx_glx_swap_interval(void *data, unsigned interval)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)
      gfx_ctx_data_get_ptr();

   glx->g_interval = interval;

   if (g_pglSwapIntervalEXT)
   {
      RARCH_LOG("[GLX]: glXSwapIntervalEXT(%u)\n", glx->g_interval);
      g_pglSwapIntervalEXT(g_x11_dpy, glx->g_glx_win, glx->g_interval);
   }
   else if (g_pglSwapInterval)
   {
      RARCH_LOG("[GLX]: glXSwapInterval(%u)\n", glx->g_interval);
      if (g_pglSwapInterval(glx->g_interval) != 0)
         RARCH_WARN("[GLX]: glXSwapInterval() failed.\n");
   }
   else if (g_pglSwapIntervalSGI)
   {
      RARCH_LOG("[GLX]: glXSwapIntervalSGI(%u)\n", glx->g_interval);
      if (g_pglSwapIntervalSGI(glx->g_interval) != 0)
         RARCH_WARN("[GLX]: glXSwapIntervalSGI() failed.\n");
   }
}

static void gfx_ctx_glx_swap_buffers(void *data)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)
      gfx_ctx_data_get_ptr();

   if (glx->g_is_double)
      glXSwapBuffers(g_x11_dpy, glx->g_glx_win);
}

static void gfx_ctx_glx_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
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
   GLXFBConfig *fbcs       = NULL;
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)calloc(1, sizeof(gfx_ctx_glx_data_t));
   driver_t *driver        = driver_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();

   if (!glx)
      return false;

   XInitThreads();

   if (!x11_connect())
      goto error;

   glXQueryVersion(g_x11_dpy, &major, &minor);

   /* GLX 1.3+ minimum required. */
   if ((major * 1000 + minor) < 1003)
      goto error;

   glx_create_context_attribs = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

#ifdef GL_DEBUG
   glx->g_debug = true;
#else
   glx->g_debug = hw_render->debug_context;
#endif

   /* Have to use ContextAttribs */
#ifdef HAVE_OPENGLES2
   glx->g_core_es      = true;
   glx->g_core_es_core = true;
#else
   glx->g_core_es      = (g_major * 1000 + g_minor) >= 3001;
   glx->g_core_es_core = (g_major * 1000 + g_minor) >= 3002;
#endif

   if ((glx->g_core_es || glx->g_debug) && !glx_create_context_attribs)
      goto error;

   fbcs = glXChooseFBConfig(g_x11_dpy, DefaultScreen(g_x11_dpy),
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

   gfx_ctx_data_set(glx);

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
   driver_t *driver = driver_get_ptr();
   settings_t *settings    = config_get_ptr();
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)
      gfx_ctx_data_get_ptr();

   x11_install_sighandlers();

   if (!glx)
      return false;

   windowed_full = settings->video.windowed_fullscreen;
   true_full = false;

   vi = glXGetVisualFromFBConfig(g_x11_dpy, glx->g_fbc);
   if (!vi)
      goto error;

   swa.colormap = g_x11_cmap = XCreateColormap(g_x11_dpy,
         RootWindow(g_x11_dpy, vi->screen), vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
      ButtonReleaseMask | ButtonPressMask;
   swa.override_redirect = fullscreen ? True : False;

   if (fullscreen && !windowed_full)
   {
      if (x11_enter_fullscreen(g_x11_dpy, width, height, &glx->g_desktop_mode))
      {
         glx->g_should_reset_mode = true;
         true_full = true;
      }
      else
         RARCH_ERR("[GLX]: Entering true fullscreen failed. Will attempt windowed mode.\n");
   }

   if (settings->video.monitor_index)
      glx->g_screen = settings->video.monitor_index - 1;

#ifdef HAVE_XINERAMA
   if (fullscreen || glx->g_screen != 0)
   {
      unsigned new_width  = width;
      unsigned new_height = height;

      if (x11_get_xinerama_coord(g_x11_dpy, glx->g_screen,
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

   g_x11_win = XCreateWindow(g_x11_dpy, RootWindow(g_x11_dpy, vi->screen),
         x_off, y_off, width, height, 0,
         vi->depth, InputOutput, vi->visual, 
         CWBorderPixel | CWColormap | CWEventMask | (true_full ? CWOverrideRedirect : 0), &swa);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);

   glx->g_glx_win = glXCreateWindow(g_x11_dpy, glx->g_fbc, g_x11_win, 0);

   x11_set_window_attr(g_x11_dpy, g_x11_win);

   if (fullscreen)
      x11_show_mouse(g_x11_dpy, g_x11_win, false);

   if (true_full)
   {
      RARCH_LOG("[GLX]: Using true fullscreen.\n");
      XMapRaised(g_x11_dpy, g_x11_win);
   }
   else if (fullscreen) /* We attempted true fullscreen, but failed. Attempt using windowed fullscreen. */
   {
      XMapRaised(g_x11_dpy, g_x11_win);
      RARCH_LOG("[GLX]: Using windowed fullscreen.\n");
      /* We have to move the window to the screen we want to go fullscreen on first.
       * x_off and y_off usually get ignored in XCreateWindow().
       */
      x11_move_window(g_x11_dpy, g_x11_win, x_off, y_off, width, height);
      x11_windowed_fullscreen(g_x11_dpy, g_x11_win);
   }
   else
   {
      XMapWindow(g_x11_dpy, g_x11_win);
      /* If we want to map the window on a different screen, we'll have to do it by force.
       * Otherwise, we should try to let the window manager sort it out.
       * x_off and y_off usually get ignored in XCreateWindow(). */
      if (glx->g_screen)
         x11_move_window(g_x11_dpy, g_x11_win, x_off, y_off, width, height);
   }

   x11_event_queue_check(&event);

   if (!glx->g_ctx)
   {
      if (glx->g_core_es || glx->g_debug)
      {
         int attribs[16];
         int *aptr = attribs;

         if (glx->g_core_es)
         {
            *aptr++ = GLX_CONTEXT_MAJOR_VERSION_ARB;
            *aptr++ = g_major;
            *aptr++ = GLX_CONTEXT_MINOR_VERSION_ARB;
            *aptr++ = g_minor;

            if (glx->g_core_es_core)
            {
               /* Technically, we don't have core/compat until 3.2.
                * Version 3.1 is either compat or not depending on GL_ARB_compatibility.
                */
               *aptr++ = GLX_CONTEXT_PROFILE_MASK_ARB;
#ifdef HAVE_OPENGLES2
               *aptr++ = GLX_CONTEXT_ES_PROFILE_BIT_EXT;
#else
               *aptr++ = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
#endif
            }
         }

         if (glx->g_debug)
         {
            *aptr++ = GLX_CONTEXT_FLAGS_ARB;
            *aptr++ = GLX_CONTEXT_DEBUG_BIT_ARB;
         }

         *aptr = None;
         glx->g_ctx = glx_create_context_attribs(g_x11_dpy, glx->g_fbc, NULL, True, attribs);
         if (glx->g_use_hw_ctx)
         {
            RARCH_LOG("[GLX]: Creating shared HW context.\n");
            glx->g_hw_ctx = glx_create_context_attribs(g_x11_dpy, glx->g_fbc, glx->g_ctx, True, attribs);
            if (!glx->g_hw_ctx)
               RARCH_ERR("[GLX]: Failed to create new shared context.\n");
         }
      }
      else
      {
         glx->g_ctx = glXCreateNewContext(g_x11_dpy, glx->g_fbc, GLX_RGBA_TYPE, 0, True);
         if (glx->g_use_hw_ctx)
         {
            glx->g_hw_ctx = glXCreateNewContext(g_x11_dpy, glx->g_fbc, GLX_RGBA_TYPE, glx->g_ctx, True);
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
      driver->video_cache_context_ack = true;
      RARCH_LOG("[GLX]: Using cached GL context.\n");
   }

   glXMakeContextCurrent(g_x11_dpy, glx->g_glx_win, glx->g_glx_win, glx->g_ctx);
   XSync(g_x11_dpy, False);

   x11_install_quit_atom();

   glXGetConfig(g_x11_dpy, vi, GLX_DOUBLEBUFFER, &val);
   glx->g_is_double = val;

   if (glx->g_is_double)
   {
      const char *swap_func = NULL;

      g_pglSwapIntervalEXT = (void (*)(Display*, GLXDrawable, int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
      g_pglSwapIntervalSGI = (int (*)(int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalSGI");
      g_pglSwapInterval    = (int (*)(int))glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");

      if (g_pglSwapIntervalEXT)
         swap_func = "glXSwapIntervalEXT";
      else if (g_pglSwapInterval)
         swap_func = "glXSwapIntervalMESA";
      else if (g_pglSwapIntervalSGI)
         swap_func = "glXSwapIntervalSGI";

      if (!g_pglSwapInterval && !g_pglSwapIntervalEXT && !g_pglSwapIntervalSGI)
         RARCH_WARN("[GLX]: Cannot find swap interval call.\n");
      else
         RARCH_LOG("[GLX]: Found swap function: %s.\n", swap_func);
   }
   else
      RARCH_WARN("[GLX]: Context is not double buffered!.\n");

   gfx_ctx_glx_swap_interval(data, glx->g_interval);

   /* This can blow up on some drivers. It's not fatal, so override errors for this call. */
   old_handler = XSetErrorHandler(glx_nul_handler);
   XSetInputFocus(g_x11_dpy, g_x11_win, RevertToNone, CurrentTime);
   XSync(g_x11_dpy, False);
   XSetErrorHandler(old_handler);

   XFree(vi);

   if (!x11_input_ctx_new(true_full))
      goto error;

   return true;

error:
   if (vi)
      XFree(vi);

   ctx_glx_destroy_resources(glx);

   if (glx)
      free(glx);

   return false;
}


static void gfx_ctx_glx_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   void *xinput = input_x.init();

   (void)data;

   *input       = xinput ? &input_x : NULL;
   *input_data  = xinput;
}

static bool gfx_ctx_glx_suppress_screensaver(void *data, bool enable)
{
   driver_t *driver = driver_get_ptr();
   if (driver->display_type != RARCH_DISPLAY_X11)
      return false;

   x11_suspend_screensaver(driver->video_window);

   return true;
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

#ifdef HAVE_OPENGLES2
   Display *dpy = XOpenDisplay(NULL);
   const char *exts = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
   bool ret = api == GFX_CTX_OPENGL_ES_API &&
      exts && strstr(exts, "GLX_EXT_create_context_es2_profile");
   XCloseDisplay(dpy);
   if (ret && g_major < 3)
   {
      g_major = 2; /* ES 2.0. */
      g_minor = 0;
   }
   return ret;
#else
   return api == GFX_CTX_OPENGL_API;
#endif
}

static void gfx_ctx_glx_show_mouse(void *data, bool state)
{
   x11_show_mouse(g_x11_dpy, g_x11_win, state);
}

static void gfx_ctx_glx_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_glx_data_t *glx = (gfx_ctx_glx_data_t*)
      gfx_ctx_data_get_ptr();

   if (!glx)
      return;

   (void)data;

   glx->g_use_hw_ctx = enable;

   if (!g_x11_dpy || !glx->g_glx_win)
      return;

   glXMakeContextCurrent(g_x11_dpy, glx->g_glx_win,
         glx->g_glx_win, enable ? glx->g_hw_ctx : glx->g_ctx);
}

const gfx_ctx_driver_t gfx_ctx_glx = {
   gfx_ctx_glx_init,
   gfx_ctx_glx_destroy,
   gfx_ctx_glx_bind_api,
   gfx_ctx_glx_swap_interval,
   gfx_ctx_glx_set_video_mode,
   x11_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   x11_get_metrics,
   NULL,
   x11_update_window_title,
   x11_check_window,
   gfx_ctx_glx_set_resize,
   x11_has_focus,
   gfx_ctx_glx_suppress_screensaver,
   gfx_ctx_glx_has_windowed,
   gfx_ctx_glx_swap_buffers,
   gfx_ctx_glx_input_driver,
   gfx_ctx_glx_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_glx_show_mouse,
   "glx",

   gfx_ctx_glx_bind_hw_render,
};

