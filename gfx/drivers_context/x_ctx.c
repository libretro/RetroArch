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

#include <stdint.h>
#include <signal.h>

#ifdef HAVE_OPENGL
#include <GL/glx.h>
#endif

#include "../../driver.h"

#include "../common/gl_common.h"
#include "../common/x11_common.h"

#ifdef HAVE_VULKAN
#include "../common/vulkan_common.h"
#endif

static int (*g_pglSwapInterval)(int);
static int (*g_pglSwapIntervalSGI)(int);
#ifdef HAVE_OPENGL
static void (*g_pglSwapIntervalEXT)(Display*, GLXDrawable, int);
#endif

typedef struct gfx_ctx_x_data
{
   bool g_use_hw_ctx;
   bool g_core_es;
   bool g_core_es_core;
   bool g_debug;
   bool g_should_reset_mode;
   bool g_is_double;

#ifdef HAVE_OPENGL
   GLXWindow g_glx_win;
   GLXContext g_ctx, g_hw_ctx;
   GLXFBConfig g_fbc;
#endif

   unsigned g_interval;
   XF86VidModeModeInfo g_desktop_mode;

#ifdef HAVE_VULKAN
   gfx_ctx_vulkan_data_t vk;
#endif
} gfx_ctx_x_data_t;

static unsigned g_major;
static unsigned g_minor;
static enum gfx_ctx_api x_api;

#ifdef HAVE_OPENGL
static PFNGLXCREATECONTEXTATTRIBSARBPROC glx_create_context_attribs;
#endif

static int x_nul_handler(Display *dpy, XErrorEvent *event)
{
   (void)dpy;
   (void)event;
   return 0;
}

static void gfx_ctx_x_destroy_resources(gfx_ctx_x_data_t *x)
{
   x11_input_ctx_destroy();

   if (g_x11_dpy)
   {
      switch (x_api)
      {
         case GFX_CTX_OPENGL_API:
         case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
            if (x->g_ctx)
            {
               glFinish();
               glXMakeContextCurrent(g_x11_dpy, None, None, NULL);

               if (!video_driver_ctl(RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT, NULL))
               {
                  if (x->g_hw_ctx)
                     glXDestroyContext(g_x11_dpy, x->g_hw_ctx);
                  if (x->g_ctx)
                     glXDestroyContext(g_x11_dpy, x->g_ctx);

                  x->g_ctx    = NULL;
                  x->g_hw_ctx = NULL;
               }
            }
            
            if (g_x11_win)
            {
               if (x->g_glx_win)
                  glXDestroyWindow(g_x11_dpy, x->g_glx_win);
               x->g_glx_win = 0;
            }
#endif
            break;

         case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
            vulkan_context_destroy(&x->vk, g_x11_win != 0);
#endif
            break;

         case GFX_CTX_NONE:
         default:
            break;
      }
   }

   if (g_x11_win)
   {
      /* Save last used monitor for later. */
      x11_save_last_used_monitor(DefaultRootWindow(g_x11_dpy));
      x11_window_destroy(false);
   }

   x11_colormap_destroy();

   if (x->g_should_reset_mode)
   {
      x11_exit_fullscreen(g_x11_dpy, &x->g_desktop_mode);
      x->g_should_reset_mode = false;
   }

   if (!video_driver_ctl(RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT, NULL) 
         && g_x11_dpy)
   {
      XCloseDisplay(g_x11_dpy);
      g_x11_dpy = NULL;
   }

   g_pglSwapInterval    = NULL;
   g_pglSwapIntervalSGI = NULL;
#ifdef HAVE_OPENGL
   g_pglSwapIntervalEXT = NULL;
#endif
   g_major              = 0;
   g_minor              = 0;
   x->g_core_es         = false;
}

static void gfx_ctx_x_destroy(void *data)
{
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;
   if (!x)
      return;
   
   gfx_ctx_x_destroy_resources(x);

   switch (x_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (x->vk.context.queue_lock)
            slock_free(x->vk.context.queue_lock);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   free(data);
}

static void gfx_ctx_x_swap_interval(void *data, unsigned interval)
{
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         x->g_interval = interval;

         if (g_pglSwapIntervalEXT)
         {
            RARCH_LOG("[GLX]: glXSwapIntervalEXT(%u)\n", x->g_interval);
            g_pglSwapIntervalEXT(g_x11_dpy, x->g_glx_win, x->g_interval);
         }
         else if (g_pglSwapInterval)
         {
            RARCH_LOG("[GLX]: glXSwapInterval(%u)\n", x->g_interval);
            if (g_pglSwapInterval(x->g_interval) != 0)
               RARCH_WARN("[GLX]: glXSwapInterval() failed.\n");
         }
         else if (g_pglSwapIntervalSGI)
         {
            RARCH_LOG("[GLX]: glXSwapIntervalSGI(%u)\n", x->g_interval);
            if (g_pglSwapIntervalSGI(x->g_interval) != 0)
               RARCH_WARN("[GLX]: glXSwapIntervalSGI() failed.\n");
         }
#endif
         break;

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (x->g_interval != interval)
         {
            x->g_interval = interval;
            if (x->vk.swapchain)
               x->vk.need_new_swapchain = true;
         }
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_x_swap_buffers(void *data)
{
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         if (x->g_is_double)
            glXSwapBuffers(g_x11_dpy, x->g_glx_win);
#endif
         break;

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_present(&x->vk, x->vk.context.current_swapchain_index);
         vulkan_acquire_next_image(&x->vk);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_x_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
#ifdef HAVE_VULKAN
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;
#endif

   x11_check_window(data, quit, resize, width, height, frame_count);

   switch (x_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (x->vk.need_new_swapchain)
            *resize = true;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static bool gfx_ctx_x_set_resize(void *data,
      unsigned width, unsigned height)
{
#ifdef HAVE_VULKAN
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;
#endif
   (void)data;
   (void)width;
   (void)height;

   switch (x_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (!vulkan_create_swapchain(&x->vk, width, height, x->g_interval))
         {
            RARCH_ERR("[X/Vulkan]: Failed to update swapchain.\n");
            return false;
         }

         x->vk.context.invalid_swapchain = true;
         x->vk.need_new_swapchain        = false;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
   return false;
}

static void *gfx_ctx_x_init(void *data)
{
   int nelements, major, minor;
#ifdef HAVE_OPENGL
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
   GLXFBConfig *fbcs       = NULL;
#endif
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)
      calloc(1, sizeof(gfx_ctx_x_data_t));
#ifndef GL_DEBUG
   struct retro_hw_render_callback *hwr = NULL;
   video_driver_ctl(RARCH_DISPLAY_CTL_HW_CONTEXT_GET, &hwr);
#endif

   if (!x)
      return NULL;

   XInitThreads();

   if (!x11_connect())
      goto error;


   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         glXQueryVersion(g_x11_dpy, &major, &minor);

         /* GLX 1.3+ minimum required. */
         if ((major * 1000 + minor) < 1003)
            goto error;

         glx_create_context_attribs = (PFNGLXCREATECONTEXTATTRIBSARBPROC)
            glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

#ifdef GL_DEBUG
         x->g_debug = true;
#else
         x->g_debug = hwr->debug_context;
#endif

         /* Have to use ContextAttribs */
#ifdef HAVE_OPENGLES2
         x->g_core_es      = true;
         x->g_core_es_core = true;
#else
         x->g_core_es      = (g_major * 1000 + g_minor) >= 3001;
         x->g_core_es_core = (g_major * 1000 + g_minor) >= 3002;
#endif

         if ((x->g_core_es || x->g_debug) && !glx_create_context_attribs)
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

         x->g_fbc = fbcs[0];
         XFree(fbcs);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         /* Use XCB WSI since it's the most supported WSI over legacy Xlib. */
         if (!vulkan_context_init(&x->vk, VULKAN_WSI_XCB))
            goto error;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   return x;

error:
   if (x)
   {
      gfx_ctx_x_destroy_resources(x);
      free(x);
   }
   g_x11_screen = 0;

   return NULL;
}

static bool gfx_ctx_x_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   XEvent event;
   bool true_full = false, windowed_full;
   int val, x_off = 0, y_off = 0;
   XVisualInfo *vi = NULL;
   XSetWindowAttributes swa = {0};
   int (*old_handler)(Display*, XErrorEvent*) = NULL;
   settings_t *settings    = config_get_ptr();
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;

   x11_install_sighandlers();

   if (!x)
      return false;

   windowed_full = settings->video.windowed_fullscreen;
   true_full = false;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         vi = glXGetVisualFromFBConfig(g_x11_dpy, x->g_fbc);
         if (!vi)
            goto error;
#endif
         break;

      case GFX_CTX_NONE:
      default:
      {
         XVisualInfo vi_template;
         /* For default case, just try to obtain a visual from template. */
         int nvisuals = 0;

         memset(&vi_template, 0, sizeof(vi_template));
         vi_template.screen = DefaultScreen(g_x11_dpy);
         vi = XGetVisualInfo(g_x11_dpy, VisualScreenMask, &vi_template, &nvisuals);
         if (!vi || nvisuals < 1)
            goto error;
      }
      break;
   }

   swa.colormap = g_x11_cmap = XCreateColormap(g_x11_dpy,
         RootWindow(g_x11_dpy, vi->screen), vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
      ButtonReleaseMask | ButtonPressMask;
   swa.override_redirect = fullscreen ? True : False;

   if (fullscreen && !windowed_full)
   {
      if (x11_enter_fullscreen(g_x11_dpy, width, height, &x->g_desktop_mode))
      {
         x->g_should_reset_mode = true;
         true_full = true;
      }
      else
         RARCH_ERR("[GLX]: Entering true fullscreen failed. Will attempt windowed mode.\n");
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
         RARCH_LOG("[GLX]: Using Xinerama on screen #%u.\n", g_x11_screen);
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
         CWBorderPixel | CWColormap | CWEventMask | 
         (true_full ? CWOverrideRedirect : 0), &swa);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         x->g_glx_win = glXCreateWindow(g_x11_dpy, x->g_fbc, g_x11_win, 0);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   x11_set_window_attr(g_x11_dpy, g_x11_win);

   if (fullscreen)
      x11_show_mouse(g_x11_dpy, g_x11_win, false);

   if (true_full)
   {
      RARCH_LOG("[GLX]: Using true fullscreen.\n");
      XMapRaised(g_x11_dpy, g_x11_win);
   }
   else if (fullscreen)
   {
      /* We attempted true fullscreen, but failed. 
       * Attempt using windowed fullscreen. */

      XMapRaised(g_x11_dpy, g_x11_win);
      RARCH_LOG("[GLX]: Using windowed fullscreen.\n");

      /* We have to move the window to the screen we want 
       * to go fullscreen on first.
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
       * Otherwise, we should try to let the window manager sort it out.
       * x_off and y_off usually get ignored in XCreateWindow(). */
      if (g_x11_screen)
         x11_move_window(g_x11_dpy, g_x11_win, x_off, y_off, width, height);
   }

   x11_event_queue_check(&event);

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         if (!x->g_ctx)
         {
            if (x->g_core_es || x->g_debug)
            {
               int attribs[16];
               int *aptr = attribs;

               if (x->g_core_es)
               {
                  *aptr++ = GLX_CONTEXT_MAJOR_VERSION_ARB;
                  *aptr++ = g_major;
                  *aptr++ = GLX_CONTEXT_MINOR_VERSION_ARB;
                  *aptr++ = g_minor;

                  if (x->g_core_es_core)
                  {
                     /* Technically, we don't have core/compat until 3.2.
                      * Version 3.1 is either compat or not depending on 
                      * GL_ARB_compatibility.
                      */
                     *aptr++ = GLX_CONTEXT_PROFILE_MASK_ARB;
#ifdef HAVE_OPENGLES2
                     *aptr++ = GLX_CONTEXT_ES_PROFILE_BIT_EXT;
#else
                     *aptr++ = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
#endif
                  }
               }

               if (x->g_debug)
               {
                  *aptr++ = GLX_CONTEXT_FLAGS_ARB;
                  *aptr++ = GLX_CONTEXT_DEBUG_BIT_ARB;
               }

               *aptr = None;
               x->g_ctx = glx_create_context_attribs(g_x11_dpy,
                     x->g_fbc, NULL, True, attribs);

               if (x->g_use_hw_ctx)
               {
                  RARCH_LOG("[GLX]: Creating shared HW context.\n");
                  x->g_hw_ctx = glx_create_context_attribs(g_x11_dpy,
                        x->g_fbc, x->g_ctx, True, attribs);

                  if (!x->g_hw_ctx)
                     RARCH_ERR("[GLX]: Failed to create new shared context.\n");
               }
            }
            else
            {
               x->g_ctx = glXCreateNewContext(g_x11_dpy, x->g_fbc,
                     GLX_RGBA_TYPE, 0, True);
               if (x->g_use_hw_ctx)
               {
                  x->g_hw_ctx = glXCreateNewContext(g_x11_dpy, x->g_fbc,
                        GLX_RGBA_TYPE, x->g_ctx, True);
                  if (!x->g_hw_ctx)
                     RARCH_ERR("[GLX]: Failed to create new shared context.\n");
               }
            }

            if (!x->g_ctx)
            {
               RARCH_ERR("[GLX]: Failed to create new context.\n");
               goto error;
            }
         }
         else
         {
            video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT_ACK, NULL);
            RARCH_LOG("[GLX]: Using cached GL context.\n");
         }

         glXMakeContextCurrent(g_x11_dpy,
               x->g_glx_win, x->g_glx_win, x->g_ctx);
#endif
         break;

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         {
            bool quit, resize;
            unsigned width, height;
            x11_check_window(x, &quit, &resize, &width, &height, 0);

            /* Use XCB surface since it's the most supported WSI.
             * We can obtain the XCB connection directly from X11. */
            if (!vulkan_surface_create(&x->vk, VULKAN_WSI_XCB,
                     g_x11_dpy, &g_x11_win, 
                     width, height, x->g_interval))
               goto error;
         }
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   XSync(g_x11_dpy, False);

   x11_install_quit_atom();

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         glXGetConfig(g_x11_dpy, vi, GLX_DOUBLEBUFFER, &val);
         x->g_is_double = val;

         if (x->g_is_double)
         {
            const char *swap_func = NULL;

            g_pglSwapIntervalEXT = (void (*)(Display*, GLXDrawable, int))
               glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
            g_pglSwapIntervalSGI = (int (*)(int))
               glXGetProcAddress((const GLubyte*)"glXSwapIntervalSGI");
            g_pglSwapInterval    = (int (*)(int))
               glXGetProcAddress((const GLubyte*)"glXSwapIntervalMESA");

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
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   gfx_ctx_x_swap_interval(data, x->g_interval);

   /* This can blow up on some drivers. 
    * It's not fatal, so override errors for this call. */
   old_handler = XSetErrorHandler(x_nul_handler);
   XSetInputFocus(g_x11_dpy, g_x11_win, RevertToNone, CurrentTime);
   XSync(g_x11_dpy, False);
   XSetErrorHandler(old_handler);

   XFree(vi);
   vi = NULL;

   if (!x11_input_ctx_new(true_full))
      goto error;

   return true;

error:
   if (vi)
      XFree(vi);

   gfx_ctx_x_destroy_resources(x);

   if (x)
      free(x);
   g_x11_screen = 0;

   return false;
}

static void gfx_ctx_x_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   void *xinput = input_x.init();

   (void)data;

   *input       = xinput ? &input_x : NULL;
   *input_data  = xinput;
}

static bool gfx_ctx_x_suppress_screensaver(void *data, bool enable)
{
   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
      return false;

   x11_suspend_screensaver(video_driver_window_get());

   return true;
}

static bool gfx_ctx_x_has_windowed(void *data)
{
   (void)data;
   return true;
}

static gfx_ctx_proc_t gfx_ctx_x_get_proc_address(const char *symbol)
{
   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         return glXGetProcAddress((const GLubyte*)symbol);
#else
         break;
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return NULL;
}

static bool gfx_ctx_x_bind_api(void *data, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   (void)data;

   g_major = major;
   g_minor = minor;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         x_api = GFX_CTX_OPENGL_API;
         return true;
#else
         break;
#endif
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES2
         {
            Display *dpy = XOpenDisplay(NULL);
            const char *exts = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
            bool ret         = exts && strstr(exts,
                  "GLX_EXT_create_context_es2_profile");
            XCloseDisplay(dpy);
            if (ret && g_major < 3)
            {
               g_major = 2; /* ES 2.0. */
               g_minor = 0;
            }
            x_api = GFX_CTX_OPENGL_ES_API;
            return ret;
         }
#else
         break;
#endif
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         x_api = api;
         return true;
#else
         break;
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_x_show_mouse(void *data, bool state)
{
   x11_show_mouse(g_x11_dpy, g_x11_win, state);
}

static void gfx_ctx_x_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;

   if (!x)
      return;

   switch (x_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGL
         x->g_use_hw_ctx = enable;
         if (!g_x11_dpy || !x->g_glx_win)
            return;
         glXMakeContextCurrent(g_x11_dpy, x->g_glx_win,
               x->g_glx_win, enable ? x->g_hw_ctx : x->g_ctx);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

#ifdef HAVE_VULKAN
static void *gfx_ctx_x_get_context_data(void *data)
{
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;
   return &x->vk.context;
}
#endif

static uint32_t gfx_ctx_x_get_flags(void *data)
{
   gfx_ctx_x_data_t *x = (gfx_ctx_x_data_t*)data;
   if (x->g_core_es_core)
      return (1UL << GFX_CTX_FLAGS_GL_CORE_CONTEXT);
   return 1UL << GFX_CTX_FLAGS_NONE;
}

const gfx_ctx_driver_t gfx_ctx_x = {
   gfx_ctx_x_init,
   gfx_ctx_x_destroy,
   gfx_ctx_x_bind_api,
   gfx_ctx_x_swap_interval,
   gfx_ctx_x_set_video_mode,
   x11_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   x11_get_metrics,
   NULL,
   x11_update_window_title,
   gfx_ctx_x_check_window,
   gfx_ctx_x_set_resize,
   x11_has_focus,
   gfx_ctx_x_suppress_screensaver,
   gfx_ctx_x_has_windowed,
   gfx_ctx_x_swap_buffers,
   gfx_ctx_x_input_driver,
   gfx_ctx_x_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_x_show_mouse,
   "x",
   gfx_ctx_x_get_flags,

   gfx_ctx_x_bind_hw_render,
#ifdef HAVE_VULKAN
   gfx_ctx_x_get_context_data,
#else
   NULL
#endif
};

