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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef __PSL1GHT__
#include <sys/spu_initialize.h>
#endif

#include <compat/strl.h>

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include <defines/ps3_defines.h>
#ifdef HAVE_GCM
#include <rsx/rsx.h>
#endif
#include "../../frontend/frontend_driver.h"
#include "../display_servers/dispserv_ps3.h"
#if defined(HAVE_PSGL)
#include "../common/gl_common.h"
#include "../common/gl2_common.h"
#endif

typedef struct gfx_ctx_ps3_data
{
#if defined(HAVE_PSGL)
   PSGLdevice* gl_device;
   PSGLcontext* gl_context;
#else
   void *empty;
#endif
} gfx_ctx_ps3_data_t;

/* TODO/FIXME - static global */
#ifdef HAVE_GCM
static enum gfx_ctx_api ps3_api = GFX_CTX_RSX_API;
#else
static enum gfx_ctx_api ps3_api = GFX_CTX_NONE;
#endif

static void gfx_ctx_ps3_set_swap_interval(void *data, int interval)
{
#if defined(HAVE_PSGL)
   if (ps3_api == GFX_CTX_OPENGL_API || ps3_api == GFX_CTX_OPENGL_ES_API)
   {
      if (interval == 1)
         gl_enable(GL_VSYNC_SCE);
      else
         gl_disable(GL_VSYNC_SCE);
   }
#endif
}

static void gfx_ctx_ps3_check_window(void *data, bool *quit,
      bool *resize, unsigned *dims)
{
   *quit    = false;
   *resize  = false;

#if defined(HAVE_PSGL)
   if (ps3_api == GFX_CTX_OPENGL_API || ps3_api == GFX_CTX_OPENGL_ES_API)
   {
      gl2_t *gl = data;
      if (gl->flags & GL2_FLAG_SHOULD_RESIZE)
         *resize = true;
   }
#endif
}

static bool gfx_ctx_ps3_has_focus(void *data) { return true; }
static bool gfx_ctx_ps3_suppress_screensaver(void *data, bool enable) { return false; }

static void gfx_ctx_ps3_swap_buffers(void *data)
{
#ifdef HAVE_PSGL
   if (ps3_api == GFX_CTX_OPENGL_API || ps3_api == GFX_CTX_OPENGL_ES_API)
      psglSwap();
#endif
#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif
}

static void gfx_ctx_ps3_get_video_size(void *data,
      unsigned *dims)
{
#if defined(HAVE_PSGL)
   if (ps3_api == GFX_CTX_OPENGL_API || ps3_api == GFX_CTX_OPENGL_ES_API)
   {
      gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)data;
      if (ps3)
         psglGetDeviceDimensions(ps3->gl_device, width, height);
   }
#endif
}

static void *gfx_ctx_ps3_init(void *video_driver)
{
#ifdef HAVE_PSGL
   PSGLdeviceParameters params;
   PSGLinitOptions options;
   unsigned dims;
#endif
   global_t        *global  = global_get_ptr();
   gfx_ctx_ps3_data_t *ps3  = (gfx_ctx_ps3_data_t*)
      calloc(1, sizeof(gfx_ctx_ps3_data_t));

   if (!ps3)
      return NULL;

#if defined(HAVE_PSGL)
   options.enable           = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS;
   options.maxSPUs          = 1;
   options.initializeSPUs   = GL_FALSE;

   /* Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL. */
   sys_spu_initialize(6, 1);
   psglInit(&options);

   params.enable            =
        PSGL_DEVICE_PARAMETERS_COLOR_FORMAT
      | PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT
      | PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
   params.colorFormat       = GL_ARGB_SCE;
   params.depthFormat       = GL_NONE;
   params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

   /* The mode the display server has chosen, if the display takes
    * it; without one PSGL keeps the system menu's mode */
   if ((dims = ps3_modes_dims(ps3_display_server_resolution(0))))
   {
      params.enable        |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
      params.width          = VIDEO_SCALE_W(dims);
      params.height         = VIDEO_SCALE_H(dims);

      global->console.screen.pal_enable = false;

      if (params.width == 720 && params.height == 576)
      {
         RARCH_LOG("[PSGL Context] 720x576 resolution detected, setting MODE_VIDEO_PAL_ENABLE.\n");
         global->console.screen.pal_enable = true;
      }
   }

   if (global->console.screen.pal60_enable)
   {
      RARCH_LOG("[PSGL Context] Setting temporal PAL60 mode.\n");
      params.enable             |= PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE;
      params.enable             |= PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE;
      params.rescPalTemporalMode = RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE;
      params.rescRatioMode       = RESC_RATIO_MODE_FULLSCREEN;
   }

   ps3->gl_device           = psglCreateDeviceExtended(&params);
   ps3->gl_context          = psglCreateContext();
   psglMakeCurrent(ps3->gl_context, ps3->gl_device);
   psglResetCurrentContext();
#endif

   global->console.screen.pal_enable =
      cellVideoOutGetResolutionAvailability(
            CELL_VIDEO_OUT_PRIMARY, CELL_VIDEO_OUT_RESOLUTION_576,
            CELL_VIDEO_OUT_ASPECT_AUTO, 0);

   return ps3;
}

static bool gfx_ctx_ps3_set_video_mode(void *data,
      unsigned dims, bool fullscreen) { return true; }

static void gfx_ctx_ps3_destroy_resources(gfx_ctx_ps3_data_t *ps3)
{
#if defined(HAVE_PSGL)
   if (!ps3)
      return;
   if (ps3_api == GFX_CTX_OPENGL_API || ps3_api == GFX_CTX_OPENGL_ES_API)
   {
      psglDestroyContext(ps3->gl_context);
      psglDestroyDevice(ps3->gl_device);
      psglExit();
   }
#endif
}

static void gfx_ctx_ps3_destroy(void *data)
{
   gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)data;

   if (!ps3)
      return;

   gfx_ctx_ps3_destroy_resources(ps3);
   free(data);
}

static void gfx_ctx_ps3_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   void *ps3input       = input_driver_init_wrap(&input_ps3, joypad_name);

   *input               = ps3input ? &input_ps3 : NULL;
   *input_data          = ps3input;
}

static enum gfx_ctx_api gfx_ctx_ps3_get_api(void *data) { return ps3_api; }

static bool gfx_ctx_ps3_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   ps3_api = api;
#ifdef HAVE_PSGL
   if (ps3_api == GFX_CTX_OPENGL_API || ps3_api == GFX_CTX_OPENGL_ES_API)
      return true;
#endif
#ifdef HAVE_GCM
   if (ps3_api == GFX_CTX_RSX_API)
      return true;
#endif
   return false;
}

static uint32_t gfx_ctx_ps3_get_flags(void *data)
{
   uint32_t flags = 0;

#ifdef HAVE_CG
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_CG);
#endif

   return flags;
}

static void gfx_ctx_ps3_set_flags(void *data, uint32_t flags) { }

const gfx_ctx_driver_t gfx_ctx_ps3 = {
   gfx_ctx_ps3_init,
   gfx_ctx_ps3_destroy,
   gfx_ctx_ps3_get_api,
   gfx_ctx_ps3_bind_api,
   gfx_ctx_ps3_set_swap_interval,
   gfx_ctx_ps3_set_video_mode,
   gfx_ctx_ps3_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size: dispserv_ps3 */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_ps3_check_window,
   NULL, /* set_resize */
   gfx_ctx_ps3_has_focus,
   gfx_ctx_ps3_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_ps3_swap_buffers,
   gfx_ctx_ps3_input_driver,
   NULL,
   NULL,
   NULL,
   NULL,
   "ps3",
   gfx_ctx_ps3_get_flags,
   gfx_ctx_ps3_set_flags,
   NULL,
   NULL,
   NULL, /* create_surface */
   NULL  /* destroy_surface */
};
