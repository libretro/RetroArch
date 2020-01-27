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

/* UWP/ANGLE EGL context. */

/* necessary for mingw32 multimon defines: */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <tchar.h>
#include <wchar.h>

#include <string.h>
#include <math.h>

#include <dynamic/dylib.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../uwp/uwp_func.h"
#include "gfx/common/win32_common.h"

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"

#include "../common/egl_common.h"
#include "../common/gl_common.h"

#ifdef HAVE_ANGLE
#include "../common/angle_common.h"
#endif

#ifdef HAVE_DYNAMIC
static dylib_t          dll_handle = NULL; /* Handle to libGLESv2.dll */
#endif

static void gfx_ctx_uwp_destroy(void *data);

static egl_ctx_data_t uwp_egl;
static int uwp_interval         = 0;
static enum gfx_ctx_api uwp_api = GFX_CTX_OPENGL_ES_API;

typedef struct gfx_ctx_cgl_data
{
   void *empty;
} gfx_ctx_uwp_data_t;

bool create_gles_context(void* corewindow)
{
   EGLint n, major, minor;
   EGLint format;
   EGLint attribs[] = {
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
   if (!angle_init_context(&uwp_egl, EGL_DEFAULT_DISPLAY,
      &major, &minor, &n, attribs, NULL))
#else
   if (!egl_init_context(&uwp_egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
      &major, &minor, &n, attribs, NULL))
#endif
   {
      egl_report_error();
      goto error;
   }

   if (!egl_get_native_visual_id(&uwp_egl, &format))
      goto error;

   if (!egl_create_context(&uwp_egl, context_attributes))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(&uwp_egl, uwp_get_corewindow()))
      goto error;

   return true;

error:
   return false;
}

static void gfx_ctx_uwp_swap_interval(void *data, int interval)
{
   (void)data;

   switch (uwp_api)
   {
      case GFX_CTX_OPENGL_ES_API:
         if (uwp_interval != interval)
         {
            uwp_interval = interval;
            egl_set_swap_interval(&uwp_egl, uwp_interval);
         }
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_uwp_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      bool is_shutdown)
{
   win32_check_window(quit, resize, width, height);
}


static gfx_ctx_proc_t gfx_ctx_uwp_get_proc_address(const char* symbol)
{
#ifdef HAVE_DYNAMIC
   return (gfx_ctx_proc_t)GetProcAddress((HINSTANCE)dll_handle, symbol);
#else
   return NULL;
#endif
}


static void gfx_ctx_uwp_swap_buffers(void *data, void *data2)
{
   (void)data;

   switch (uwp_api)
   {
   case GFX_CTX_OPENGL_ES_API:
      egl_swap_buffers(&uwp_egl);
      break;
   case GFX_CTX_NONE:
   default:
      break;
   }
}

static bool gfx_ctx_uwp_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   return false;
}


static void gfx_ctx_uwp_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   bool quit;
   bool resize;
   win32_check_window(&quit, &resize, width, height);
}

static void *gfx_ctx_uwp_init(video_frame_info_t *video_info, void *video_driver)
{
   gfx_ctx_uwp_data_t *uwp = (gfx_ctx_uwp_data_t*)calloc(1, sizeof(*uwp));

   if (!uwp)
      return NULL;


#ifdef HAVE_DYNAMIC
   dll_handle = dylib_load("libGLESv2.dll");
#endif

   return uwp;
}

static void gfx_ctx_uwp_destroy(void *data)
{
   gfx_ctx_uwp_data_t *wgl = (gfx_ctx_uwp_data_t*)data;

   switch (uwp_api)
   {
   case GFX_CTX_OPENGL_ES_API:
      egl_destroy(&uwp_egl);
      break;

   case GFX_CTX_NONE:
   default:
      break;
   }

#ifdef HAVE_DYNAMIC
   dylib_close(dll_handle);
#endif

}

static bool gfx_ctx_uwp_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      RARCH_ERR("[UWP EGL]: win32_set_video_mode failed.\n");
   }

   if (!create_gles_context(uwp_get_corewindow())) {
      RARCH_ERR("[UWP EGL]: create_gles_context failed.\n");
      goto error;
   }

   gfx_ctx_uwp_swap_interval(data, uwp_interval);
   return true;

error:
   gfx_ctx_uwp_destroy(data);
   return false;
}

static void gfx_ctx_uwp_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   settings_t *settings = config_get_ptr();

   /* Plain xinput is supported on UWP, but it
    * supports joypad only (uwp driver was added later) */
   if (string_is_equal(settings->arrays.input_driver, "xinput"))
   {
      void* xinput = input_xinput.init(joypad_name);
      *input = xinput ? (input_driver_t*)&input_xinput : NULL;
      *input_data = xinput;
   }
   else
   {
      void* uwp = input_uwp.init(joypad_name);
      *input = uwp ? (input_driver_t*)&input_uwp : NULL;
      *input_data = uwp;
   }
}

static enum gfx_ctx_api gfx_ctx_uwp_get_api(void *data)
{
   return uwp_api;
}

static bool gfx_ctx_uwp_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   if (api == GFX_CTX_OPENGL_ES_API)
      return true;
   else
      return false;
}

static void gfx_ctx_uwp_bind_hw_render(void *data, bool enable)
{
   switch (uwp_api)
   {
      case GFX_CTX_OPENGL_ES_API:
         egl_bind_hw_render(&uwp_egl, enable);
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static uint32_t gfx_ctx_uwp_get_flags(void *data)
{
   uint32_t flags = 0;

   switch (uwp_api)
   {
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

const gfx_ctx_driver_t gfx_ctx_uwp = {
   gfx_ctx_uwp_init,
   gfx_ctx_uwp_destroy,
   gfx_ctx_uwp_get_api,
   gfx_ctx_uwp_bind_api,
   gfx_ctx_uwp_swap_interval,
   gfx_ctx_uwp_set_video_mode,
   gfx_ctx_uwp_get_video_size,
   NULL, /* get refresh rate */
   NULL, /* get video output size */
   NULL, /* get video output prev */
   NULL, /* get video output next */
   win32_get_metrics,
   NULL,
   NULL, /* update title */
   gfx_ctx_uwp_check_window,
   gfx_ctx_uwp_set_resize,
   win32_has_focus,
   NULL, /* suppress screensaver */
   true, /* has_windowed */
   gfx_ctx_uwp_swap_buffers,
   gfx_ctx_uwp_input_driver,
   gfx_ctx_uwp_get_proc_address,
   NULL,
   NULL,
   win32_show_cursor,
   "uwp",
   gfx_ctx_uwp_get_flags, /* get flags */
   NULL, /* set flags */
   gfx_ctx_uwp_bind_hw_render,
   NULL,
   NULL
};
