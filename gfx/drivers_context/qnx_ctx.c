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

#include <stdint.h>

#include <bps/screen.h>
#include <bps/navigator.h>
#include <bps/event.h>
#include <screen/screen.h>
#include <sys/platform.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_OPENGLES2
#include <GLES2/gl2.h>
#elif HAVE_OPENGLES3
#include <GLES3/gl3.h>
#endif

#ifdef HAVE_EGL
#include <EGL/egl.h>
#endif

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#ifdef HAVE_OPENGLES
#include "../common/gl_common.h"
#endif

#include "../../configuration.h"
#include "../../verbosity.h"

#define WINDOW_BUFFERS 2

screen_context_t screen_ctx;
screen_window_t screen_win;

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   screen_display_t screen_disp;
   bool resize;
} qnx_ctx_data_t;

static enum gfx_ctx_api qnx_api = GFX_CTX_NONE;

static void gfx_ctx_qnx_destroy(void *data)
{
   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_destroy(&qnx->egl);
#endif

   free(data);
}

static void *gfx_ctx_qnx_init(video_frame_info_t *video_info, void *video_driver)
{
   EGLint n;
   EGLint major, minor;
   EGLint context_attributes[] = {
#ifdef HAVE_OPENGLES2
           EGL_CONTEXT_CLIENT_VERSION, 2,
#elif HAVE_OPENGLES3
           EGL_CONTEXT_CLIENT_VERSION, 3,
#endif
      EGL_NONE
   };

   const EGLint attribs[] = {
#ifdef HAVE_OPENGLES2
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#elif HAVE_OPENGLES3
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
#endif
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_NONE
   };

   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)calloc(1, sizeof(*qnx));

   if (!qnx)
       goto screen_error;

   /* Create a screen context that will be used to
    * create an EGL surface to receive libscreen events */

   RARCH_LOG("Initializing screen context...\n");
   if (!screen_ctx)
   {
      screen_create_context(&screen_ctx, 0);

      if (screen_request_events(screen_ctx) != BPS_SUCCESS)
      {
         RARCH_ERR("screen_request_events failed.\n");
         goto screen_error;
      }

      if (navigator_request_events(0) != BPS_SUCCESS)
      {
         RARCH_ERR("navigator_request_events failed.\n");
         goto screen_error;
      }

      if (navigator_rotation_lock(false) != BPS_SUCCESS)
      {
         RARCH_ERR("navigator_location_lock failed.\n");
         goto screen_error;
      }
   }

#ifdef HAVE_EGL
   if (!egl_init_context(&qnx->egl, EGL_NONE, EGL_DEFAULT_DISPLAY, &major, &minor,
            &n, attribs, NULL))
      goto error;

   if (!egl_create_context(&qnx->egl, context_attributes))
      goto error;
#endif

   if(!screen_win)
   {
      if (screen_create_window(&screen_win, screen_ctx))
      {
             RARCH_ERR("screen_create_window failed:.\n");
	     goto error;
      }
   }

   int format = SCREEN_FORMAT_RGBX8888;
   if (screen_set_window_property_iv(screen_win,
            SCREEN_PROPERTY_FORMAT, &format))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_FORMAT] failed.\n");
      goto error;
   }

   int usage;
#ifdef HAVE_OPENGLES2
   usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_ROTATION;
#elif HAVE_OPENGLES3
   usage = SCREEN_USAGE_OPENGL_ES3 | SCREEN_USAGE_ROTATION;
#endif
   if (screen_set_window_property_iv(screen_win,
            SCREEN_PROPERTY_USAGE, &usage))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_USAGE] failed.\n");
      goto error;
   }

   if (screen_get_window_property_pv(screen_win,
            SCREEN_PROPERTY_DISPLAY, (void **)&qnx->screen_disp))
   {
      RARCH_ERR("screen_get_window_property_pv [SCREEN_PROPERTY_DISPLAY] failed.\n");
      goto error;
   }

   int screen_resolution[2];

   if (screen_get_display_property_iv(qnx->screen_disp,
            SCREEN_PROPERTY_SIZE, screen_resolution))
   {
      RARCH_ERR("screen_get_window_property_iv [SCREEN_PROPERTY_SIZE] failed.\n");
      goto error;
   }

#ifndef HAVE_BB10
   int angle, size[2];

   angle = atoi(getenv("ORIENTATION"));

   screen_display_mode_t screen_mode;
   if (screen_get_display_property_pv(qnx->screen_disp,
            SCREEN_PROPERTY_MODE, (void**)&screen_mode))
   {
      RARCH_ERR("screen_get_display_property_pv [SCREEN_PROPERTY_MODE] failed.\n");
      goto error;
   }

   if (screen_get_window_property_iv(screen_win,
            SCREEN_PROPERTY_BUFFER_SIZE, size))
   {
      RARCH_ERR("screen_get_window_property_iv [SCREEN_PROPERTY_BUFFER_SIZE] failed.\n");
      goto error;
   }

   int buffer_size[2] = {size[0], size[1]};

   if ((angle == 0) || (angle == 180))
   {
      if (((screen_mode.width > screen_mode.height) && (size[0] < size[1])) ||
            ((screen_mode.width < screen_mode.height) && (size[0] > size[1])))
      {
         buffer_size[1] = size[0];
         buffer_size[0] = size[1];
      }
   }
   else if ((angle == 90) || (angle == 270))
   {
      if (((screen_mode.width > screen_mode.height) && (size[0] > size[1])) ||
            ((screen_mode.width < screen_mode.height && size[0] < size[1])))
      {
         buffer_size[1] = size[0];
         buffer_size[0] = size[1];
      }
   }
   else
   {
      RARCH_ERR("Navigator returned an unexpected orientation angle.\n");
      goto error;
   }

   if (screen_set_window_property_iv(screen_win,
            SCREEN_PROPERTY_BUFFER_SIZE, buffer_size))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_BUFFER_SIZE] failed.\n");
      goto error;
   }

   if (screen_set_window_property_iv(screen_win,
            SCREEN_PROPERTY_ROTATION, &angle))
   {
      RARCH_ERR("screen_set_window_property_iv [SCREEN_PROPERTY_ROTATION] failed.\n");
      goto error;
   }
#endif

   if (screen_create_window_buffers(screen_win, WINDOW_BUFFERS))
   {
      RARCH_ERR("screen_create_window_buffers failed.\n");
      goto error;
   }

   if (!egl_create_surface(&qnx->egl, screen_win))
      goto error;

   return qnx;

error:
   egl_report_error();
   gfx_ctx_qnx_destroy(video_driver);
screen_error:
   screen_stop_events(screen_ctx);
   return NULL;
}

static void gfx_ctx_qnx_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_get_video_size(&qnx->egl, width, height);
#endif
}

static void gfx_ctx_qnx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      bool is_shutdown)
{
   unsigned new_width, new_height;
   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)data;

   *quit = false;

#ifdef HAVE_EGL
   egl_get_video_size(&qnx->egl, &new_width, &new_height);
#endif

   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   /* Check if we are exiting. */
   if (is_shutdown)
      *quit = true;
}

static bool gfx_ctx_qnx_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}

static void gfx_ctx_qnx_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   void *qnxinput       = input_qnx.init(joypad_name);

   *input               = qnxinput ? &input_qnx : NULL;
   *input_data          = qnxinput;
}

static enum gfx_ctx_api gfx_ctx_qnx_get_api(void *data)
{
   return qnx_api;
}

static bool gfx_ctx_qnx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   qnx_api = api;

   if (api == GFX_CTX_OPENGL_ES_API)
      return true;

   return false;
}

static bool gfx_ctx_qnx_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_qnx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static int dpi_get_density(qnx_ctx_data_t *qnx)
{
    int screen_dpi[2];

    if(!qnx)
        return -1;

    if (screen_get_display_property_iv(qnx->screen_disp,
             SCREEN_PROPERTY_DPI, screen_dpi))
    {
       RARCH_ERR("screen_get_display_property_iv [SCREEN_PROPERTY_DPI] failed.\n");
       return -1;
    }

    return min(screen_dpi[0], screen_dpi[1]);
}

static bool gfx_ctx_qnx__get_metrics(void *data,
    enum display_metric_types type, float *value)
{
   static int dpi = -1;
   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)data;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         return false;
      case DISPLAY_METRIC_MM_HEIGHT:
         return false;
      case DISPLAY_METRIC_DPI:
         if (dpi == -1)
         {
            dpi = dpi_get_density(qnx);
            if (dpi <= 0)
               goto dpi_fallback;
         }
         *value = (float)dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;

dpi_fallback:
   /* Add a fallback in case the device doesn't report DPI.
    * Calculated as an average of all BB10 device DPIs circa 2016. */
   dpi    = 345;
   *value = (float)dpi;
   return true;
}

static void gfx_ctx_qnx_set_swap_interval(void *data, int swap_interval)
{
   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_set_swap_interval(&qnx->egl, swap_interval);
#endif
}

static void gfx_ctx_qnx_swap_buffers(void *data, void *data2)
{
   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_swap_buffers(&qnx->egl);
#endif
}

static void gfx_ctx_qnx_bind_hw_render(void *data, bool enable)
{
   qnx_ctx_data_t *qnx = (qnx_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_bind_hw_render(&qnx->egl, enable);
#endif
}

static gfx_ctx_proc_t gfx_ctx_qnx_get_proc_address(const char *symbol)
{
#ifdef HAVE_EGL
   return egl_get_proc_address(symbol);
#endif
}

static uint32_t gfx_ctx_qnx_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static void gfx_ctx_qnx_set_flags(void *data, uint32_t flags)
{
   (void)flags;
}

const gfx_ctx_driver_t gfx_ctx_qnx = {
   gfx_ctx_qnx_init,
   gfx_ctx_qnx_destroy,
   gfx_ctx_qnx_get_api,
   gfx_ctx_qnx_bind_api,
   gfx_ctx_qnx_set_swap_interval,
   gfx_ctx_qnx_set_video_mode,
   gfx_ctx_qnx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_qnx__get_metrics,
   NULL,
   NULL, /* update_title */
   gfx_ctx_qnx_check_window,
   NULL, /* set_resize */
   gfx_ctx_qnx_has_focus,
   gfx_ctx_qnx_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_qnx_swap_buffers,
   gfx_ctx_qnx_input_driver,
   gfx_ctx_qnx_get_proc_address,
   NULL,
   NULL,
   NULL,
   "qnx",
   gfx_ctx_qnx_get_flags,
   gfx_ctx_qnx_set_flags,
   gfx_ctx_qnx_bind_hw_render,
   NULL,
   NULL
};
