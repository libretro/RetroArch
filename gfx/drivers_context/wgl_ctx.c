/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <math.h>

/* Win32/WGL context. */

/* Necessary for mingw32 multimon defines: */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 /* _WIN32_WINNT_WIN2K */
#endif

#include <tchar.h>
#include <wchar.h>

#include <string.h>
#include <math.h>

#include <windows.h>
#include <commdlg.h>

#include <dynamic/dylib.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"

#include "../common/win32_common.h"

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#ifdef HAVE_ANGLE
#include "../common/angle_common.h"
#endif
#endif

#ifdef __WINRT__
#include "../common/uwpgdi.h"
#endif

#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)) && !defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
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
#endif

/* Forward declarations */
LRESULT CALLBACK wnd_proc_wgl_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_wgl_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_wgl_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);

static BOOL (APIENTRY *p_swap_interval)(int);

enum wgl_flags
{
   WGL_FLAG_USE_HW_CTX         = (1 << 0),
   WGL_FLAG_CORE_HW_CTX_ENABLE = (1 << 1),
   WGL_FLAG_ADAPTIVE_VSYNC     = (1 << 2)
};

/* TODO/FIXME - static globals */
static HGLRC win32_hrc;
static HGLRC win32_hw_hrc;
static HDC   win32_hdc;
static uint8_t wgl_flags;
#ifdef HAVE_EGL
static egl_ctx_data_t win32_egl;
#endif
static void             *dinput_wgl       = NULL;
static unsigned         win32_major       = 0;
static unsigned         win32_minor       = 0;
static int              win32_interval    = 0;
enum gfx_ctx_api win32_api                = GFX_CTX_NONE;
#ifdef HAVE_DYLIB
static dylib_t          dll_handle        = NULL; /* Handle to OpenGL32.dll/libGLESv2.dll */
#endif

typedef struct gfx_ctx_cgl_data
{
   void *empty;
} gfx_ctx_wgl_data_t;

/* FORWARD DECLARATIONS */
void win32_get_video_size(void *data, unsigned *width, unsigned *height);

static gfx_ctx_proc_t gfx_ctx_wgl_get_proc_address(const char *symbol)
{
#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)) && !defined(HAVE_OPENGLES)
   if (win32_api == GFX_CTX_OPENGL_API)
   {
      gfx_ctx_proc_t func = (gfx_ctx_proc_t)wglGetProcAddress(symbol);
      if (func)
         return func;
   }
#endif
#ifdef HAVE_DYLIB
   return (gfx_ctx_proc_t)GetProcAddress((HINSTANCE)dll_handle, symbol);
#else
   return NULL;
#endif
}

#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)) && !defined(HAVE_OPENGLES)
static bool wgl_has_extension(const char *ext, const char *exts)
{
   const char *where = strchr(ext, ' ');

   if (where || *ext == '\0')
      return false;

   if (exts)
   {
      const char *terminator = NULL;
      const char *start      = exts;

      for (;;)
      {
         if (!(where = strstr(start, ext)))
            break;

         terminator = where + strlen(ext);
         if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
               return true;

         start = terminator;
      }
   }
   return false;
}

void create_gl_context(HWND hwnd, bool *quit)
{
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   bool core_context                    = (win32_major * 1000 + win32_minor) >= 3001;
#ifdef GL_DEBUG
   bool debug                           = true;
#else
   bool debug                           = hwr->debug_context;
#endif

#ifdef __WINRT__
   win32_hdc                            = (HDC)(hwnd);
#else
   win32_hdc                            = GetDC(hwnd);
#endif

   win32_setup_pixel_format(win32_hdc, true);

   if (win32_hrc)
   {
      video_state_get_ptr()->flags |= VIDEO_FLAG_CACHE_CONTEXT_ACK;
      RARCH_LOG("[WGL]: Using cached GL context.\n");
   }
   else
   {
      win32_hrc         = wglCreateContext(win32_hdc);
      /* We'll create shared context later if not. */
      if (win32_hrc && !core_context && !debug)
      {
         win32_hw_hrc   = wglCreateContext(win32_hdc);
         if (win32_hw_hrc)
         {
            if (!wglShareLists(win32_hrc, win32_hw_hrc))
            {
               RARCH_LOG("[WGL]: Failed to share contexts.\n");
               *quit    = true;
            }
         }
         else
            *quit       = true;
      }
   }

   if (win32_hrc)
   {
      if (wglMakeCurrent(win32_hdc, win32_hrc))
         g_win32_flags |= WIN32_CMN_FLAG_INITED;
      else
         *quit          = true;
   }
   else
   {
      *quit             = true;
      return;
   }

   if (core_context || debug)
   {
      unsigned i;
      int attribs[16];
      int *aptr = attribs;
      typedef HGLRC (APIENTRY *wglCreateContextAttribsProc)
         (HDC, HGLRC, const int*);
      static wglCreateContextAttribsProc pcreate_context;

      for (i = 0; i < 16; i++)
         attribs[i] = 0;

      if (core_context)
      {
         *aptr++ = WGL_CONTEXT_MAJOR_VERSION_ARB;
         *aptr++ = win32_major;
         *aptr++ = WGL_CONTEXT_MINOR_VERSION_ARB;
         *aptr++ = win32_minor;

         /* Technically, we don't have core/compat until 3.2.
          * Version 3.1 is either compat or not depending
          * on GL_ARB_compatibility.
          */
         if ((win32_major * 1000 + win32_minor) >= 3002)
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
            gfx_ctx_wgl_get_proc_address("wglCreateContextAttribsARB");

      /* In order to support the core info "required_hw_api"
       * field correctly, we should try to init the highest available
       * version GL context possible. This means trying successively
       * lower versions until it works, because GL has
       * no facility for determining the highest possible
       * supported version.
       */
      if (pcreate_context)
      {
         int i;
         int gl_versions[][2] = {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2}, {3, 1}, {3, 0}};
         int gl_version_rows  = ARRAY_SIZE(gl_versions);
         HGLRC context        = NULL;
         int version_rows     = gl_version_rows;
         int (*versions)[2]   = gl_versions;

         /* Only try higher versions when core_context is true */
         if (!core_context)
            version_rows = 1;

         /* Try versions from highest down to requested version */
         for (i = 0; i < version_rows; i++)
         {
            if (core_context)
            {
               attribs[1] = versions[i][0];
               attribs[3] = versions[i][1];
            }

            context = pcreate_context(win32_hdc, NULL, attribs);

            if (context)
            {
               wglMakeCurrent(NULL, NULL);
               wglDeleteContext(win32_hrc);
               win32_hrc = context;

               if (!wglMakeCurrent(win32_hdc, win32_hrc))
               {
                  *quit = true;
                  break;
               }

               if (wgl_flags & WGL_FLAG_USE_HW_CTX)
               {
                  win32_hw_hrc = pcreate_context(win32_hdc, context, attribs);

                  if (!win32_hw_hrc)
                  {
                     RARCH_ERR("[WGL]: Failed to create shared context.\n");
                     *quit = true;
                     break;
                  }
               }

               /* Found a suitable version that is high enough, we can stop now */
               break;
            }
            else if (
                     (versions[i][0] == (int)win32_major)
                  && (versions[i][1] == (int)win32_minor))
            {
               /* The requested version was tried and
                * is not supported, go ahead and fail
                * since everything else will be lower than that. */
               break;
            }
         }

         if (!context)
         {
            RARCH_ERR("[WGL]: Failed to create core context. Falling back to legacy context.\n");
            *quit = true;
         }
      }
      else
         RARCH_ERR("[WGL]: wglCreateContextAttribsARB not supported.\n");
   }

   {
      const char *(WINAPI * wglGetExtensionsStringARB) (HDC) = 0;
      const char *exts                                       = NULL;
      wglGetExtensionsStringARB                              =
	      (const char *(WINAPI *) (HDC))
	      gfx_ctx_wgl_get_proc_address("wglGetExtensionsStringARB");

      if (wglGetExtensionsStringARB)
      {
         exts = wglGetExtensionsStringARB(win32_hdc);
         RARCH_LOG("[WGL]: Extensions: %s\n", exts);
         if (wgl_has_extension("WGL_EXT_swap_control_tear", exts))
         {
            RARCH_LOG("[WGL]: Adaptive VSync supported.\n");
            wgl_flags |= WGL_FLAG_ADAPTIVE_VSYNC;
         }
      }
   }
}
#endif

#if defined(HAVE_OPENGLES) && defined(HAVE_EGL)
void create_gles_context(HWND hwnd, bool *quit)
{
   EGLint n, major, minor;
   EGLint format;
   EGLint attribs[]            = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      EGL_NONE
   };
   EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

#ifdef HAVE_ANGLE
   if (!angle_init_context(&win32_egl, EGL_DEFAULT_DISPLAY,
      &major, &minor, &n, attribs, NULL))
#else
   if (!egl_init_context(&win32_egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
      &major, &minor, &n, attribs, NULL))
#endif
   {
      egl_report_error();
      goto error;
   }

   if (!egl_get_native_visual_id(&win32_egl, &format))
      goto error;

   if (!egl_create_context(&win32_egl, context_attributes))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(&win32_egl, hwnd))
      goto error;

   g_win32_flags   |= WIN32_CMN_FLAG_INITED;
   return;

error:
   *quit = true;
   return;
}
#endif

static void gfx_ctx_wgl_swap_interval(void *data, int interval)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)) && !defined(HAVE_OPENGLES)
         win32_interval = interval;
         if (!win32_hrc || !p_swap_interval)
            return;

         if (!p_swap_interval(win32_interval))
            RARCH_WARN("[WGL]: wglSwapInterval(%i) failed.\n", win32_interval);
#endif
         break;

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_EGL
         if (win32_interval != interval)
         {
            win32_interval = interval;
            egl_set_swap_interval(&win32_egl, win32_interval);
         }
#endif
         break;

      case GFX_CTX_NONE:
      default:
         win32_interval = interval;
         break;
   }
}

static void gfx_ctx_wgl_swap_buffers(void *data)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef __WINRT__
         wglSwapBuffers(win32_hdc);
#else
         SwapBuffers(win32_hdc);
#endif
         break;
      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_EGL)
         egl_swap_buffers(&win32_egl);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static bool gfx_ctx_wgl_set_resize(void *data,
      unsigned width, unsigned height) { return false; }

static void gfx_ctx_wgl_destroy(void *data)
{
   HWND            window  = win32_get_window();
   gfx_ctx_wgl_data_t *wgl = (gfx_ctx_wgl_data_t*)data;

   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)) && !defined(HAVE_OPENGLES)
         if (win32_hrc)
         {
            uint32_t video_st_flags;
            video_driver_state_t *video_st = video_state_get_ptr();
            gl_finish();
            wglMakeCurrent(NULL, NULL);

            video_st_flags = video_st->flags;
            if (!(video_st_flags & VIDEO_FLAG_CACHE_CONTEXT))
            {
               if (win32_hw_hrc)
                  wglDeleteContext(win32_hw_hrc);
               wglDeleteContext(win32_hrc);
               win32_hrc    = NULL;
               win32_hw_hrc = NULL;
            }
         }
#endif
         break;

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_EGL
         egl_destroy(&win32_egl);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   if (window && win32_hdc)
   {
#ifndef __WINRT__
      ReleaseDC(window, win32_hdc);
#endif
      win32_hdc = NULL;
   }

#ifndef __WINRT__
   if (window)
   {
      win32_monitor_from_window();
      win32_destroy_window();
   }

#endif
   if (g_win32_flags & WIN32_CMN_FLAG_RESTORE_DESKTOP)
   {
#ifndef __WINRT__
      win32_monitor_get_info();
#endif
      g_win32_flags &= ~WIN32_CMN_FLAG_RESTORE_DESKTOP;
   }

#ifdef HAVE_DYLIB
   dylib_close(dll_handle);
#endif

   if (wgl)
      free(wgl);

   win32_major                  = 0;
   win32_minor                  = 0;
   p_swap_interval              = NULL;
   wgl_flags                   &= ~(WGL_FLAG_CORE_HW_CTX_ENABLE
                                |   WGL_FLAG_ADAPTIVE_VSYNC
                                  );
   g_win32_flags               &= ~WIN32_CMN_FLAG_INITED;
}


static void *gfx_ctx_wgl_init(void *video_driver)
{
#ifndef __WINRT__
   WNDCLASSEX wndclass     = {0};
#endif
   gfx_ctx_wgl_data_t *wgl = (gfx_ctx_wgl_data_t*)calloc(1, sizeof(*wgl));
   uint8_t win32_flags     = win32_get_flags();
   settings_t *settings    = config_get_ptr();

   if (!wgl)
      return NULL;

   if (win32_flags & WIN32_CMN_FLAG_INITED)
      gfx_ctx_wgl_destroy(NULL);

#ifdef HAVE_DYLIB
#ifdef HAVE_OPENGL
   dll_handle = dylib_load("OpenGL32.dll");
#else
   dll_handle = dylib_load("libGLESv2.dll");
#endif
#endif

#ifndef __WINRT__
   win32_window_reset();
   win32_monitor_init();


   wndclass.lpfnWndProc    = wnd_proc_wgl_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
	   wndclass.lpfnWndProc = wnd_proc_wgl_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
	   wndclass.lpfnWndProc = wnd_proc_wgl_winraw;
#endif

   if (!win32_window_init(&wndclass, true, NULL))
   {
      free(wgl);
      return NULL;
   }
#else
   bool quit = false;
   create_gl_context(uwp_get_corewindow(), &quit);
   if (quit)
   {
      RARCH_ERR("[UWP WGL]: create_gl_context failed.\n");
      free(wgl);
      return NULL;
   }
#endif
   return wgl;
}

static bool gfx_ctx_wgl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      RARCH_ERR("[WGL]: win32_set_video_mode failed.\n");
      gfx_ctx_wgl_destroy(data);
      return false;
   }


   if (win32_api == GFX_CTX_OPENGL_API)
      p_swap_interval = (BOOL (APIENTRY *)(int))gfx_ctx_wgl_get_proc_address("wglSwapIntervalEXT");

   gfx_ctx_wgl_swap_interval(data, win32_interval);
   return true;
}

static void gfx_ctx_wgl_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   settings_t *settings     = config_get_ptr();

#if _WIN32_WINNT >= 0x0501
#ifdef HAVE_WINRAWINPUT
   const char *input_driver = settings->arrays.input_driver;

   /* winraw only available since XP */
   if (string_is_equal(input_driver, "raw"))
   {
      *input_data = input_driver_init_wrap(&input_winraw, joypad_name);
      if (*input_data)
      {
         *input     = &input_winraw;
         dinput_wgl = NULL;
         return;
      }
   }
#endif
#endif

#ifdef HAVE_DINPUT
   dinput_wgl  = input_driver_init_wrap(&input_dinput, joypad_name);
   *input      = dinput_wgl ? &input_dinput : NULL;
   *input_data = dinput_wgl;
#elif defined(__WINRT__)
   /* Plain xinput is supported on UWP, but it
    * supports joypad only (uwp driver was added later) */
   if (string_is_equal(settings->arrays.input_driver, "xinput"))
   {
      void* xinput = input_driver_init_wrap(&input_xinput, joypad_name);
      *input = xinput ? (input_driver_t*)&input_xinput : NULL;
      *input_data = xinput;
   }
   else
   {
      void* uwp = input_driver_init_wrap(&input_uwp, joypad_name);
      *input = uwp ? (input_driver_t*)&input_uwp : NULL;
      *input_data = uwp;
   }
#elif defined(_XBOX)
   void* xinput = input_driver_init_wrap(&input_xinput, joypad_name);
   *input = xinput ? (input_driver_t*)&input_xinput : NULL;
   *input_data = xinput;
#endif
}

static enum gfx_ctx_api gfx_ctx_wgl_get_api(void *data) { return win32_api; }

static bool gfx_ctx_wgl_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   win32_major = major;
   win32_minor = minor;
   win32_api   = api;

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
   if (api == GFX_CTX_OPENGL_API)
      return true;
#endif
#if defined(HAVE_OPENGLES)
   if (api == GFX_CTX_OPENGL_ES_API)
      return true;
#endif

   return false;
}

static void gfx_ctx_wgl_bind_hw_render(void *data, bool enable)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)) && !defined(HAVE_OPENGLES)
         wgl_flags |= WGL_FLAG_USE_HW_CTX;

         if (win32_hdc)
         {
            if (enable)
               wglMakeCurrent(win32_hdc, win32_hw_hrc);
            else
               wglMakeCurrent(win32_hdc, win32_hrc);
         }
#endif
         break;

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_EGL
         egl_bind_hw_render(&win32_egl, enable);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static uint32_t gfx_ctx_wgl_get_flags(void *data)
{
   uint32_t flags = 0;

   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
         if (wgl_flags & WGL_FLAG_ADAPTIVE_VSYNC)
            BIT32_SET(flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC);

         if (wgl_flags & WGL_FLAG_CORE_HW_CTX_ENABLE)
            BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

         if (string_is_equal(video_driver_get_ident(), "gl1")) { }
         else if (string_is_equal(video_driver_get_ident(), "glcore"))
         {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
            BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
         }
         else
         {
#ifdef HAVE_CG
            if (!(wgl_flags & WGL_FLAG_CORE_HW_CTX_ENABLE))
               BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_CG);
#endif
#ifdef HAVE_GLSL
            BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
         }

         break;

      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
         BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
#ifdef HAVE_GLSL
            BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   return flags;
}

static void gfx_ctx_wgl_set_flags(void *data, uint32_t flags)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         if (BIT32_GET(flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC))
            wgl_flags |= WGL_FLAG_ADAPTIVE_VSYNC;
         if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
            wgl_flags |= WGL_FLAG_CORE_HW_CTX_ENABLE;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

}

static void gfx_ctx_wgl_get_video_output_prev(void *data) { }
static void gfx_ctx_wgl_get_video_output_next(void *data) { }

/* TODO: maybe create an uwp_mesa_common.c? */
#ifdef __WINRT__

static void win32_get_video_size(void* data,
   unsigned* width, unsigned* height)
{
   bool quit = false;
   bool resize = false;
   win32_check_window(NULL, &quit, &resize, width, height);
   width = uwp_get_width();
   height = uwp_get_height();
}

void win32_get_video_output_size(void* data, unsigned* width, unsigned* height, char* desc, size_t desc_len)
{
   win32_get_video_size(data, width, height);
}

bool win32_suspend_screensaver(void* data, bool enable)
{
   return true;
}

float win32_get_refresh_rate(void* data)
{
   return 60.0;
}

#define win32_get_refresh_rate NULL

HWND win32_get_window(void)
{
   return (HWND)uwp_get_corewindow();
}

/* TODO/FIXME - static globals */
uint8_t g_win32_flags = 0;

uint8_t win32_get_flags(void) { return g_win32_flags; }
/* NTD, already done by mesa */
void win32_setup_pixel_format(HDC hdc, bool supports_gl) { }
#endif

const gfx_ctx_driver_t gfx_ctx_wgl = {
   gfx_ctx_wgl_init,
   gfx_ctx_wgl_destroy,
   gfx_ctx_wgl_get_api,
   gfx_ctx_wgl_bind_api,
   gfx_ctx_wgl_swap_interval,
   gfx_ctx_wgl_set_video_mode,
   win32_get_video_size,
   win32_get_refresh_rate,
   win32_get_video_output_size,
   gfx_ctx_wgl_get_video_output_prev,
   gfx_ctx_wgl_get_video_output_next,
   win32_get_metrics,
   NULL,
   video_driver_update_title,
   win32_check_window,
   gfx_ctx_wgl_set_resize,
   win32_has_focus,
   win32_suspend_screensaver,
   true, /* has_windowed */
   gfx_ctx_wgl_swap_buffers,
   gfx_ctx_wgl_input_driver,
   gfx_ctx_wgl_get_proc_address,
   NULL,
   NULL,
   win32_show_cursor,
   "wgl",
   gfx_ctx_wgl_get_flags,
   gfx_ctx_wgl_set_flags,
   gfx_ctx_wgl_bind_hw_render,
   NULL,
   NULL
};
