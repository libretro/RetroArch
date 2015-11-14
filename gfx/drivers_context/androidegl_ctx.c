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

#include <EGL/egl.h>
#include <sys/system_properties.h>

#include <formats/image.h>

#include "../../driver.h"
#include "../../general.h"
#include "../../runloop.h"
#include "../video_monitor.h"
#include "../drivers/gl_common.h"

#include "../../frontend/drivers/platform_linux.h"

/* forward declaration */
int system_property_get(const char *name, char *value);

typedef struct gfx_ctx_android_data
{
   bool g_use_hw_ctx;
   EGLContext g_egl_hw_ctx;
   EGLContext g_egl_ctx;
   EGLSurface g_egl_surf;
   EGLDisplay g_egl_dpy;
   EGLConfig g_egl_config;
} gfx_ctx_android_data_t;

static bool g_es3;

static void android_gfx_ctx_set_swap_interval(void *data, unsigned interval)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_android_data_t *android = NULL;
   
   android = (gfx_ctx_android_data_t*)driver->video_context_data;

   (void)data;
   if (!android)
      return;

   eglSwapInterval(android->g_egl_dpy, interval);
}

static void android_gfx_ctx_destroy_resources(gfx_ctx_android_data_t *android)
{
   if (!android)
      return;

   if (android->g_egl_dpy)
   {
      if (android->g_egl_ctx)
      {
         eglMakeCurrent(android->g_egl_dpy,
               EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(android->g_egl_dpy, android->g_egl_ctx);
      }

      if (android->g_egl_hw_ctx)
         eglDestroyContext(android->g_egl_dpy, android->g_egl_hw_ctx);

      if (android->g_egl_surf)
         eglDestroySurface(android->g_egl_dpy, android->g_egl_surf);
      eglTerminate(android->g_egl_dpy);
   }

   /* Be as careful as possible in deinit. */

   android->g_egl_ctx     = NULL;
   android->g_egl_hw_ctx  = NULL;
   android->g_egl_surf    = NULL;
   android->g_egl_dpy     = NULL;
   android->g_egl_config  = 0;
}

static void android_gfx_ctx_destroy(void *data)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_android_data_t *android = NULL;
   
   android = (gfx_ctx_android_data_t*)driver->video_context_data;
   (void)data;

   if (!android)
      return;

   android_gfx_ctx_destroy_resources(android);

   if (driver->video_context_data)
      free(driver->video_context_data);
   driver->video_context_data = NULL;
}

static void android_gfx_ctx_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   EGLint gl_width, gl_height;
   driver_t *driver = driver_get_ptr();
   gfx_ctx_android_data_t *android = NULL;
   
   android = (gfx_ctx_android_data_t*)
      driver->video_context_data;

   *width  = 0;
   *height = 0;

   if (!android)
      return;
   if (!android->g_egl_dpy)
      return;

   eglQuerySurface(android->g_egl_dpy,
         android->g_egl_surf, EGL_WIDTH, &gl_width);
   eglQuerySurface(android->g_egl_dpy,
         android->g_egl_surf, EGL_HEIGHT, &gl_height);
   *width  = gl_width;
   *height = gl_height;
}

static bool android_gfx_ctx_init(void *data)
{
   int var;
   EGLint num_config, egl_version_major, egl_version_minor;
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
   driver_t *driver = driver_get_ptr();
   gfx_ctx_android_data_t *android = NULL;
   struct android_app *android_app = (struct android_app*)g_android;
   
   if (!android_app)
      return false;

   android = (gfx_ctx_android_data_t*)
      calloc(1, sizeof(gfx_ctx_android_data_t));

   if (!android)
      return false;

   RARCH_LOG("Android EGL: GLES version = %d.\n", g_es3 ? 3 : 2);

   android->g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

   if (!android->g_egl_dpy)
   {
      RARCH_ERR("[Android/EGL]: Couldn't get EGL display.\n");
      goto error;
   }

   if (!eglInitialize(android->g_egl_dpy,
            &egl_version_major, &egl_version_minor))
      goto error;

   RARCH_LOG("[ANDROID/EGL]: EGL version: %d.%d\n",
         egl_version_major, egl_version_minor);

   if (!eglChooseConfig(android->g_egl_dpy,
            attribs, &android->g_egl_config, 1, &num_config))
      goto error;

   var = eglGetConfigAttrib(android->g_egl_dpy, android->g_egl_config,
         EGL_NATIVE_VISUAL_ID, &format);

   if (!var)
   {
      RARCH_ERR("eglGetConfigAttrib failed: %d.\n", var);
      goto error;
   }

   ANativeWindow_setBuffersGeometry(android_app->window, 0, 0, format);

   android->g_egl_ctx = eglCreateContext(android->g_egl_dpy,
         android->g_egl_config, EGL_NO_CONTEXT, context_attributes);

   if (android->g_egl_ctx == EGL_NO_CONTEXT)
      goto error;

   if (android->g_use_hw_ctx)
   {
      android->g_egl_hw_ctx = eglCreateContext(android->g_egl_dpy,
           android->g_egl_config, android->g_egl_ctx,
            context_attributes);
      RARCH_LOG("[Android/EGL]: Created shared context: %p.\n",
            (void*)android->g_egl_hw_ctx);

      if (android->g_egl_hw_ctx == EGL_NO_CONTEXT)
         goto error;
   }

   android->g_egl_surf = eglCreateWindowSurface(android->g_egl_dpy,
         android->g_egl_config, android_app->window, 0);
   if (!android->g_egl_surf)
      goto error;

   if (!eglMakeCurrent(android->g_egl_dpy, android->g_egl_surf,
            android->g_egl_surf, android->g_egl_ctx))
      goto error;

   driver->video_context_data = android;

   return true;

error:
   android_gfx_ctx_destroy_resources(android);

   if (android)
      free(android);

   return false;
}

static void android_gfx_ctx_swap_buffers(void *data)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_android_data_t *android = NULL;
   
   android = (gfx_ctx_android_data_t*)driver->video_context_data;

   (void)data;

   if (android)
      eglSwapBuffers(android->g_egl_dpy, android->g_egl_surf);
}

static void android_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   unsigned new_width, new_height;
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   (void)frame_count;

   *quit = false;

   android_gfx_ctx_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   /* Check if we are exiting. */
   if (system->shutdown)
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
      rarch_main_msg_queue_push(buf_fps, 1, 1, false);
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

static gfx_ctx_proc_t android_gfx_ctx_get_proc_address(
      const char *symbol)
{
   gfx_ctx_proc_t ret;
   void *sym__ = NULL;

   retro_assert(sizeof(void*) == sizeof(void (*)(void)));

   sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
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
   (void)data;
   return true;
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

static void android_gfx_ctx_bind_hw_render(void *data, bool enable)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_android_data_t *android = NULL;
   
   android = (gfx_ctx_android_data_t*)driver->video_context_data;

   (void)data;

   if (!android)
      return;

   android->g_use_hw_ctx = enable;

   if (!android->g_egl_dpy)
      return;
   if (!android->g_egl_surf)
      return;

   eglMakeCurrent(android->g_egl_dpy, android->g_egl_surf,
         android->g_egl_surf, enable ? 
         android->g_egl_hw_ctx : android->g_egl_ctx);
}

static int system_property_get_density(char *value)
{
   FILE *pipe;
   int length = 0;
   char buffer[PATH_MAX_LENGTH] = {0};
   char cmd[PATH_MAX_LENGTH]    = {0};
   char *curpos                 = NULL;

   snprintf(cmd, sizeof(cmd), "wm density");

   pipe = popen(cmd, "r");

   if (!pipe)
   {
      RARCH_ERR("Could not create pipe.\n");
      return 0;
   }

   curpos = value;
   
   while (!feof(pipe))
   {
      if (fgets(buffer, 128, pipe) != NULL)
      {
         int curlen = strlen(buffer);

         memcpy(curpos, buffer, curlen);

         curpos    += curlen;
         length    += curlen;
      }
   }

   *curpos = '\0';

   pclose(pipe);

   return length;
}

static void dpi_get_density(char *s, size_t len)
{
   system_property_get("ro.sf.lcd_density", s);

   if (s[0] == '\0')
      system_property_get_density(s);
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
   android_gfx_ctx_set_swap_interval,
   android_gfx_ctx_set_video_mode,
   android_gfx_ctx_get_video_size,
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
   android_gfx_ctx_swap_buffers,
   android_gfx_ctx_input_driver,
   android_gfx_ctx_get_proc_address,
   NULL,
   NULL,
   NULL,
   "android",
   android_gfx_ctx_bind_hw_render,
};
