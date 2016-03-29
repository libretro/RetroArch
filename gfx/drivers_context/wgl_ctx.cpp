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

#include <math.h>

/* Win32/WGL context. */

/* necessary for mingw32 multimon defines: */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <string.h>
#include <math.h>

#include <windows.h>
#include <commdlg.h>

#include <dynamic/dylib.h>

#include "../../driver.h"
#include "../../dynamic.h"
#include "../../runloop.h"
#include "../video_context_driver.h"

#include "../common/gl_common.h"
#include "../common/win32_common.h"

#ifdef HAVE_VULKAN
#include "../common/vulkan_common.h"
#endif

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif

#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif

#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif

#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001
#endif

#ifndef WGL_CONTEXT_FLAGS_ARB
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#endif

#ifndef WGL_CONTEXT_DEBUG_BIT_ARB
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x0001
#endif

typedef HGLRC (APIENTRY *wglCreateContextAttribsProc)(HDC, HGLRC, const int*);
static BOOL (APIENTRY *p_swap_interval)(int);

static bool g_use_hw_ctx;
static HGLRC g_hrc;
static HGLRC g_hw_hrc;
static HDC g_hdc;

#ifdef HAVE_VULKAN
static gfx_ctx_vulkan_data_t g_vk;
#endif

static unsigned g_major;
static unsigned g_minor;
static enum gfx_ctx_api g_api;

static unsigned g_interval;

static dylib_t dll_handle = NULL; /* Handle to OpenGL32.dll */

static wglCreateContextAttribsProc pcreate_context;

static void setup_pixel_format(HDC hdc)
{
   PIXELFORMATDESCRIPTOR pfd = {0};
   pfd.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
   pfd.nVersion     = 1;
   pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
   pfd.iPixelType   = PFD_TYPE_RGBA;
   pfd.cColorBits   = 32;
   pfd.cDepthBits   = 0;
   pfd.cStencilBits = 0;
   pfd.iLayerType   = PFD_MAIN_PLANE;

   SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
}

static void create_gl_context(HWND hwnd, bool *quit)
{
   bool core_context;
   struct retro_hw_render_callback *hwr = NULL;
   bool debug                           = false;

   video_driver_ctl(RARCH_DISPLAY_CTL_HW_CONTEXT_GET, &hwr);

   debug            = hwr->debug_context;
#ifdef _WIN32
   dll_handle       = dylib_load("OpenGL32.dll");
#endif
   g_hdc            = GetDC(hwnd);
   setup_pixel_format(g_hdc);

#ifdef GL_DEBUG
   debug = true;
#endif
   core_context = (g_major * 1000 + g_minor) >= 3001;

   if (g_hrc)
   {
      RARCH_LOG("[WGL]: Using cached GL context.\n");
      video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT_ACK, NULL);
   }
   else
   {
      g_hrc = wglCreateContext(g_hdc);
      
      /* We'll create shared context later if not. */
      if (g_hrc && !core_context && !debug) 
      {
         g_hw_hrc = wglCreateContext(g_hdc);
         if (g_hw_hrc)
         {
            if (!wglShareLists(g_hrc, g_hw_hrc))
            {
               RARCH_LOG("[WGL]: Failed to share contexts.\n");
               *quit = true;
            }
         }
         else
            *quit = true;
      }
   }

   if (g_hrc)
   {
      if (wglMakeCurrent(g_hdc, g_hrc))
         g_inited = true;
      else
         *quit     = true;
   }
   else
   {
      *quit        = true;
      return;
   }

   if (core_context || debug)
   {
      int attribs[16];
      int *aptr = attribs;

      if (core_context)
      {
         *aptr++ = WGL_CONTEXT_MAJOR_VERSION_ARB;
         *aptr++ = g_major;
         *aptr++ = WGL_CONTEXT_MINOR_VERSION_ARB;
         *aptr++ = g_minor;

         /* Technically, we don't have core/compat until 3.2.
          * Version 3.1 is either compat or not depending 
          * on GL_ARB_compatibility.
          */
         if ((g_major * 1000 + g_minor) >= 3002)
         {
            *aptr++ = WGL_CONTEXT_PROFILE_MASK_ARB;
            *aptr++ = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
         }
      }

      if (debug)
      {
         *aptr++ = WGL_CONTEXT_FLAGS_ARB;
         *aptr++ = WGL_CONTEXT_DEBUG_BIT_ARB;
      }

      *aptr = 0;

      if (!pcreate_context)
         pcreate_context = (wglCreateContextAttribsProc)
            wglGetProcAddress("wglCreateContextAttribsARB");

      if (pcreate_context)
      {
         HGLRC context = pcreate_context(g_hdc, NULL, attribs);

         if (context)
         {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(g_hrc);
            g_hrc = context;
            if (!wglMakeCurrent(g_hdc, g_hrc))
               *quit = true;
         }
         else
            RARCH_ERR("[WGL]: Failed to create core context. Falling back to legacy context.\n");

         if (g_use_hw_ctx)
         {
            g_hw_hrc = pcreate_context(g_hdc, context, attribs);
            if (!g_hw_hrc)
            {
               RARCH_ERR("[WGL]: Failed to create shared context.\n");
               *quit = true;
            }
         }
      }
      else
         RARCH_ERR("[WGL]: wglCreateContextAttribsARB not supported.\n");
   }
}

void create_graphics_context(HWND hwnd, bool *quit)
{
   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
         create_gl_context(hwnd, quit);
         break;

      case GFX_CTX_VULKAN_API:
      {
#ifdef HAVE_VULKAN
         RECT rect;
         unsigned width = rect.right - rect.left;
         unsigned height = rect.bottom - rect.top;
         GetClientRect(hwnd, &rect);
         HINSTANCE instance = GetModuleHandle(NULL);
         if (!vulkan_surface_create(&g_vk, VULKAN_WSI_WIN32,
                  &instance, &hwnd, 
                  width, height, g_interval))
            *quit = true;
         g_inited = true;
         break;
#endif
      }

      case GFX_CTX_NONE:
      default:
         break;
   }
}

void *dinput_wgl;

static void gfx_ctx_wgl_swap_interval(void *data, unsigned interval)
{
   (void)data;
   g_interval = interval;

   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         if (!g_hrc)
            return;
         if (!p_swap_interval)
            return;

         RARCH_LOG("[WGL]: wglSwapInterval(%u)\n", g_interval);
         if (!p_swap_interval(g_interval))
            RARCH_WARN("[WGL]: wglSwapInterval() failed.\n");
#endif
         break;

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (g_interval != interval)
         {
            g_interval = interval;
            if (g_vk.swapchain)
               g_vk.need_new_swapchain = true;
         }
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_wgl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   win32_check_window(quit, resize, width, height);

   switch (g_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (g_vk.need_new_swapchain)
            *resize = true;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_wgl_swap_buffers(void *data)
{
   (void)data;

   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         SwapBuffers(g_hdc);
         break;
#endif

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_present(&g_vk, g_vk.context.current_swapchain_index);
         vulkan_acquire_next_image(&g_vk);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static bool gfx_ctx_wgl_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   switch (g_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (!vulkan_create_swapchain(&g_vk, width, height, g_interval))
         {
            RARCH_ERR("[Win32/Vulkan]: Failed to update swapchain.\n");
            return false;
         }

         g_vk.context.invalid_swapchain = true;
         g_vk.need_new_swapchain        = false;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_wgl_update_window_title(void *data)
{
   char buf[128]        = {0};
   char buf_fps[128]    = {0};
   settings_t *settings = config_get_ptr();
   HWND         window  = win32_get_window();

   (void)data;

   if (video_monitor_get_fps(buf, sizeof(buf),
            buf_fps, sizeof(buf_fps)))
      SetWindowText(window, buf);
   if (settings->fps_show)
      runloop_msg_queue_push(buf_fps, 1, 1, false);
}

static void gfx_ctx_wgl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   (void)data;
   HWND         window  = win32_get_window();

   if (!window)
   {
      unsigned mon_id;
      RECT mon_rect;
      MONITORINFOEX current_mon;
      HMONITOR hm_to_use = NULL;

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      mon_rect = current_mon.rcMonitor;
      *width  = mon_rect.right - mon_rect.left;
      *height = mon_rect.bottom - mon_rect.top;
   }
   else
   {
      *width  = g_resize_width;
      *height = g_resize_height;
   }
}

static void *gfx_ctx_wgl_init(void *video_driver)
{
   WNDCLASSEX wndclass = {0};

   (void)video_driver;

   if (g_inited)
      return NULL;

   win32_window_reset();
   win32_monitor_init();

   wndclass.lpfnWndProc   = WndProcGL;
   if (!win32_window_init(&wndclass, true, NULL))
           return NULL;

   switch (g_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (!vulkan_context_init(&g_vk, VULKAN_WSI_WIN32))
            return NULL;
#endif
         break;
   }

   return (void*)"wgl";
}

static void gfx_ctx_wgl_destroy(void *data)
{
   HWND     window  = win32_get_window();

   (void)data;

   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         if (g_hrc)
         {
            glFinish();
            wglMakeCurrent(NULL, NULL);

            if (!video_driver_ctl(RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT, NULL))
            {
               if (g_hw_hrc)
                  wglDeleteContext(g_hw_hrc);
               wglDeleteContext(g_hrc);
               g_hrc = NULL;
               g_hw_hrc = NULL;
            }
         }
#endif
         break;

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_context_destroy(&g_vk, g_vk.vk_surface != VK_NULL_HANDLE);
         if (g_vk.context.queue_lock)
            slock_free(g_vk.context.queue_lock);
         memset(&g_vk, 0, sizeof(g_vk));
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   if (window && g_hdc)
   {
      ReleaseDC(window, g_hdc);
      g_hdc = NULL;
   }

   if (window)
   {
      win32_monitor_from_window(window, true);
      win32_destroy_window();
   }

   if (g_restore_desktop)
   {
      win32_monitor_get_info();
      g_restore_desktop = false;
   }

   g_inited = false;
   g_major = g_minor = 0;
   p_swap_interval = NULL;
}

static bool gfx_ctx_wgl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      RARCH_ERR("[WGL]: win32_set_video_mode failed.\n");
      goto error;
   }

   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         p_swap_interval = (BOOL (APIENTRY *)(int))
            wglGetProcAddress("wglSwapIntervalEXT");
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   gfx_ctx_wgl_swap_interval(data, g_interval);
   return true;

error:
   gfx_ctx_wgl_destroy(data);
   return false;
}


static void gfx_ctx_wgl_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;

   dinput_wgl   = input_dinput.init();

   *input       = dinput_wgl ? &input_dinput : NULL;
   *input_data  = dinput_wgl;
}

static bool gfx_ctx_wgl_has_focus(void *data)
{
   return win32_has_focus();
}

static bool gfx_ctx_wgl_suppress_screensaver(void *data, bool enable)
{
   return win32_suppress_screensaver(data, enable);
}

static bool gfx_ctx_wgl_has_windowed(void *data)
{
   (void)data;

   return true;
}

static gfx_ctx_proc_t gfx_ctx_wgl_get_proc_address(const char *symbol)
{
   void *func = (void *)wglGetProcAddress(symbol);
   if (func)
      return (gfx_ctx_proc_t)wglGetProcAddress(symbol);
   return (gfx_ctx_proc_t)GetProcAddress((HINSTANCE)dll_handle, symbol);
}

static bool gfx_ctx_wgl_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   return win32_get_metrics(data, type, value);
}

static bool gfx_ctx_wgl_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   g_major = major;
   g_minor = minor;
   g_api = api;

   return api == GFX_CTX_OPENGL_API || api == GFX_CTX_VULKAN_API;
}

static void gfx_ctx_wgl_show_mouse(void *data, bool state)
{
   (void)data;
   win32_show_cursor(state);
}

static void gfx_ctx_wgl_bind_hw_render(void *data, bool enable)
{
   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         g_use_hw_ctx = enable;

         if (g_hdc)
            wglMakeCurrent(g_hdc, enable ? g_hw_hrc : g_hrc);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

#ifdef HAVE_VULKAN
static void *gfx_ctx_wgl_get_context_data(void *data)
{
   (void)data;
   return &g_vk.context;
}
#endif

const gfx_ctx_driver_t gfx_ctx_wgl = {
   gfx_ctx_wgl_init,
   gfx_ctx_wgl_destroy,
   gfx_ctx_wgl_bind_api,
   gfx_ctx_wgl_swap_interval,
   gfx_ctx_wgl_set_video_mode,
   gfx_ctx_wgl_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_wgl_get_metrics,
   NULL,
   gfx_ctx_wgl_update_window_title,
   gfx_ctx_wgl_check_window,
   gfx_ctx_wgl_set_resize,
   gfx_ctx_wgl_has_focus,
   gfx_ctx_wgl_suppress_screensaver,
   gfx_ctx_wgl_has_windowed,
   gfx_ctx_wgl_swap_buffers,
   gfx_ctx_wgl_input_driver,
   gfx_ctx_wgl_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_wgl_show_mouse,
   "wgl",
   gfx_ctx_wgl_bind_hw_render,
#ifdef HAVE_VULKAN
   gfx_ctx_wgl_get_context_data,
#else
   NULL,
#endif
};

