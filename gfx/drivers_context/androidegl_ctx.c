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

#include <sys/system_properties.h>

#include <formats/image.h>

#include "../../driver.h"
#include "../../general.h"
#include "../../runloop.h"
#include "../common/egl_common.h"
#include "../common/gl_common.h"

#include "../../frontend/drivers/platform_linux.h"

/* forward declaration */
int system_property_get(const char *cmd, const char *args, char *value);

static bool g_es3;

static void *android_gfx_ctx_init(void *video_driver)
{
   EGLint n, major, minor;
   EGLint format;
   EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, g_es3 ? 3 : 2,
      EGL_NONE
   };
   const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE
   };
   struct android_app *android_app = (struct android_app*)g_android;
   egl_ctx_data_t *egl;

   if (!android_app)
      return false;

   egl = (egl_ctx_data_t*)calloc(1, sizeof(*egl));

   RARCH_LOG("Android EGL: GLES version = %d.\n", g_es3 ? 3 : 2);

   if (!egl_init_context(egl, EGL_DEFAULT_DISPLAY,
            &major, &minor, &n, attribs))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_get_native_visual_id(egl, &format))
      goto error;

   ANativeWindow_setBuffersGeometry(android_app->window, 0, 0, format);

   if (!egl_create_context(egl, context_attributes))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(egl, android_app->window))
      goto error;

   return egl;

error:
   if (egl)
   {
      egl_destroy(egl);
      free(egl);
   }

   return NULL;
}

static void android_gfx_ctx_destroy(void *data)
{
   if (data)
   {
      egl_destroy(data);
      free(data);
   }
}

static void android_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   unsigned new_width, new_height;

   (void)frame_count;

   *quit = false;

   egl_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   /* Check if we are exiting. */
   if (runloop_ctl(RUNLOOP_CTL_IS_SHUTDOWN, NULL))
      *quit = true;
}

static void android_gfx_ctx_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void android_gfx_ctx_update_window_title(void *data)
{
   char buf[128]        = {0};
   char buf_fps[128]    = {0};
   settings_t *settings = config_get_ptr();

   video_monitor_get_fps(buf, sizeof(buf),
         buf_fps, sizeof(buf_fps));
   if (settings->fps_show)
      runloop_msg_queue_push(buf_fps, 1, 1, false);
}

static bool android_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}

static void android_gfx_ctx_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   void *androidinput = input_android.init();

   (void)data;

   *input = androidinput ? &input_android : NULL;
   *input_data = androidinput;
}

static bool android_gfx_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   unsigned version = major * 100 + minor;

   (void)data;

   if (version > 300)
      return false;
   if (version < 300)
      g_es3 = false;
   else if (version == 300)
      g_es3 = true;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool android_gfx_ctx_has_focus(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   
   if (!android_app)
      return true;
   return (android_app->unfocused == true ) ? false : true;
}

static bool android_gfx_ctx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool android_gfx_ctx_has_windowed(void *data)
{
   (void)data;
   return false;
}

static void dpi_get_density(char *s, size_t len)
{
   system_property_get("getprop", "ro.sf.lcd_density", s);

   if (s[0] == '\0')
      system_property_get("wm", "density", s);
}

static bool android_gfx_ctx_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   static int dpi = -1;
   char density[PROP_VALUE_MAX] = {0};

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         return false;
      case DISPLAY_METRIC_MM_HEIGHT:
         return false;
      case DISPLAY_METRIC_DPI:
         if (dpi == -1)
         {
            dpi_get_density(density, sizeof(density));
            if (density[0] == '\0')
               return false;
            dpi    = atoi(density);
         }
         *value = (float)dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;
}

const gfx_ctx_driver_t gfx_ctx_android = {
   android_gfx_ctx_init,
   android_gfx_ctx_destroy,
   android_gfx_ctx_bind_api,
   egl_set_swap_interval,
   android_gfx_ctx_set_video_mode,
   egl_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   android_gfx_ctx_get_metrics,
   NULL,
   android_gfx_ctx_update_window_title,
   android_gfx_ctx_check_window,
   android_gfx_ctx_set_resize,
   android_gfx_ctx_has_focus,
   android_gfx_ctx_suppress_screensaver,
   android_gfx_ctx_has_windowed,
   egl_swap_buffers,
   android_gfx_ctx_input_driver,
   egl_get_proc_address,
   NULL,
   NULL,
   NULL,
   "android",
   egl_bind_hw_render,
};
