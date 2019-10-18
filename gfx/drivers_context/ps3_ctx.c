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
#include "config.h"
#endif

#ifndef __PSL1GHT__
#include <sys/spu_initialize.h>
#endif

#include <compat/strl.h>

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../defines/ps3_defines.h"
#include "../../frontend/frontend_driver.h"
#include "../common/gl_common.h"

typedef struct gfx_ctx_ps3_data
{
#if defined(HAVE_PSGL)
   PSGLdevice* gl_device;
   PSGLcontext* gl_context;
#else
   void *empty;
#endif
} gfx_ctx_ps3_data_t;

static enum gfx_ctx_api ps3_api = GFX_CTX_NONE;

static void gfx_ctx_ps3_get_resolution(unsigned idx,
      unsigned *width, unsigned *height)
{
   CellVideoOutResolution resolution;
   cellVideoOutGetResolution(idx, &resolution);

   *width  = resolution.width;
   *height = resolution.height;
}

static float gfx_ctx_ps3_get_aspect_ratio(void *data)
{
   CellVideoOutState videoState;

   cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);

   switch (videoState.displayMode.aspect)
   {
      case CELL_VIDEO_OUT_ASPECT_4_3:
         return 4.0f/3.0f;
      case CELL_VIDEO_OUT_ASPECT_16_9:
         break;
   }

   return 16.0f/9.0f;
}

static void gfx_ctx_ps3_get_available_resolutions(void)
{
   unsigned i;
   uint32_t videomode[] = {
      CELL_VIDEO_OUT_RESOLUTION_480,
      CELL_VIDEO_OUT_RESOLUTION_576,
      CELL_VIDEO_OUT_RESOLUTION_960x1080,
      CELL_VIDEO_OUT_RESOLUTION_720,
      CELL_VIDEO_OUT_RESOLUTION_1280x1080,
      CELL_VIDEO_OUT_RESOLUTION_1440x1080,
      CELL_VIDEO_OUT_RESOLUTION_1600x1080,
      CELL_VIDEO_OUT_RESOLUTION_1080
   };
   uint32_t resolution_count = 0;
   bool defaultresolution    = true;
   uint16_t num_videomodes   = sizeof(videomode) / sizeof(uint32_t);
   global_t       *global    = global_get_ptr();

   if (global->console.screen.resolutions.check)
      return;

   for (i = 0; i < num_videomodes; i++)
   {
      if (cellVideoOutGetResolutionAvailability(
               CELL_VIDEO_OUT_PRIMARY, videomode[i],
               CELL_VIDEO_OUT_ASPECT_AUTO, 0))
         resolution_count++;
   }

   global->console.screen.resolutions.count = 0;
   global->console.screen.resolutions.list  =
      malloc(resolution_count * sizeof(uint32_t));

   for (i = 0; i < num_videomodes; i++)
   {
      if (cellVideoOutGetResolutionAvailability(
               CELL_VIDEO_OUT_PRIMARY,
               videomode[i],
               CELL_VIDEO_OUT_ASPECT_AUTO, 0))
      {
         global->console.screen.resolutions.list[
            global->console.screen.resolutions.count++] = videomode[i];
         global->console.screen.resolutions.initial.id = videomode[i];

         if (global->console.screen.resolutions.current.id == videomode[i])
         {
            defaultresolution = false;
            global->console.screen.resolutions.current.idx =
               global->console.screen.resolutions.count-1;
         }
      }
   }

   /* In case we didn't specify a resolution -
    * make the last resolution
      that was added to the list (the highest resolution)
      the default resolution */
   if (global->console.screen.resolutions.current.id > num_videomodes || defaultresolution)
    {
      global->console.screen.resolutions.current.idx = resolution_count - 1;
      global->console.screen.resolutions.current.id = global->console.screen.resolutions.list[global->console.screen.resolutions.current.idx];
    }

   global->console.screen.resolutions.check = true;
}

static void gfx_ctx_ps3_set_swap_interval(void *data, int interval)
{
#if defined(HAVE_PSGL)
   if (interval == 1)
      glEnable(GL_VSYNC_SCE);
   else
      glDisable(GL_VSYNC_SCE);
#endif
}

static void gfx_ctx_ps3_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      bool is_shutdown)
{
   gl_t *gl = data;

   *quit    = false;
   *resize  = false;

   if (gl->should_resize)
      *resize = true;
}

static bool gfx_ctx_ps3_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_ps3_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void gfx_ctx_ps3_swap_buffers(void *data, void *data2)
{
   (void)data;
#ifdef HAVE_PSGL
   psglSwap();
#endif
#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif
}

static void gfx_ctx_ps3_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)data;

#if defined(HAVE_PSGL)
   if (ps3)
      psglGetDeviceDimensions(ps3->gl_device, width, height);
#endif
}

static void *gfx_ctx_ps3_init(video_frame_info_t *video_info, void *video_driver)
{
#ifdef HAVE_PSGL
   PSGLdeviceParameters params;
   PSGLinitOptions options;
#endif
   global_t *global = global_get_ptr();
   gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)
      calloc(1, sizeof(gfx_ctx_ps3_data_t));

   (void)video_driver;
   (void)global;

   if (!ps3)
      return NULL;

#if defined(HAVE_PSGL)
   options.enable         = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS;
   options.maxSPUs        = 1;
   options.initializeSPUs = GL_FALSE;

   /* Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL. */
   sys_spu_initialize(6, 1);
   psglInit(&options);

   params.enable            =
      PSGL_DEVICE_PARAMETERS_COLOR_FORMAT |
      PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT |
      PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
   params.colorFormat       = GL_ARGB_SCE;
   params.depthFormat       = GL_NONE;
   params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

   if (global->console.screen.resolutions.current.id)
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;

      gfx_ctx_ps3_get_resolution(
            global->console.screen.resolutions.current.id,
            &params.width, &params.height);

      global->console.screen.pal_enable = false;

      if (params.width == 720 && params.height == 576)
      {
         RARCH_LOG("[PSGL Context]: 720x576 resolution detected, setting MODE_VIDEO_PAL_ENABLE.\n");
         global->console.screen.pal_enable = true;
      }
   }

   if (global->console.screen.pal60_enable)
   {
      RARCH_LOG("[PSGL Context]: Setting temporal PAL60 mode.\n");
      params.enable |= PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE;
      params.enable |= PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE;
      params.rescPalTemporalMode = RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE;
      params.rescRatioMode = RESC_RATIO_MODE_FULLSCREEN;
   }

   ps3->gl_device = psglCreateDeviceExtended(&params);
   ps3->gl_context = psglCreateContext();

   psglMakeCurrent(ps3->gl_context, ps3->gl_device);
   psglResetCurrentContext();
#endif

   global->console.screen.pal_enable =
      cellVideoOutGetResolutionAvailability(
            CELL_VIDEO_OUT_PRIMARY, CELL_VIDEO_OUT_RESOLUTION_576,
            CELL_VIDEO_OUT_ASPECT_AUTO, 0);

   gfx_ctx_ps3_get_available_resolutions();

   return ps3;
}

static bool gfx_ctx_ps3_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   return true;
}

static void gfx_ctx_ps3_destroy_resources(gfx_ctx_ps3_data_t *ps3)
{
   if (!ps3)
      return;

#if defined(HAVE_PSGL)
   psglDestroyContext(ps3->gl_context);
   psglDestroyDevice(ps3->gl_device);

   psglExit();
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
   void *ps3input       = input_ps3.init(joypad_name);

   *input               = ps3input ? &input_ps3 : NULL;
   *input_data          = ps3input;
}

static enum gfx_ctx_api gfx_ctx_ps3_get_api(void *data)
{
   return ps3_api;
}

static bool gfx_ctx_ps3_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;

   ps3_api = api;

   if (
         api == GFX_CTX_OPENGL_API ||
         api == GFX_CTX_OPENGL_ES_API
      )
      return true;

   return false;
}

static void gfx_ctx_ps3_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   global_t *global = global_get_ptr();

   if (!global)
      return;

   gfx_ctx_ps3_get_resolution(global->console.screen.resolutions.current.id,
         width, height);

   if (*width == 720 && *height == 576)
   {
      if (global->console.screen.pal_enable)
         global->console.screen.pal60_enable = true;
   }
   else
   {
      global->console.screen.pal_enable = false;
      global->console.screen.pal60_enable = false;
   }
}

static void gfx_ctx_ps3_get_video_output_prev(void *data)
{
   global_t *global = global_get_ptr();

   if (!global)
      return;

   if (global->console.screen.resolutions.current.idx)
   {
      global->console.screen.resolutions.current.idx--;
      global->console.screen.resolutions.current.id =
         global->console.screen.resolutions.list
         [global->console.screen.resolutions.current.idx];
   }
}

static void gfx_ctx_ps3_get_video_output_next(void *data)
{
   global_t *global = global_get_ptr();

   if (!global)
      return;

   if (global->console.screen.resolutions.current.idx + 1 <
         global->console.screen.resolutions.count)
   {
      global->console.screen.resolutions.current.idx++;
      global->console.screen.resolutions.current.id =
         global->console.screen.resolutions.list
         [global->console.screen.resolutions.current.idx];
   }
}

static uint32_t gfx_ctx_ps3_get_flags(void *data)
{
   uint32_t flags = 0;

#ifdef HAVE_CG
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_CG);
#endif

   return flags;
}

static void gfx_ctx_ps3_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_ps3 = {
   gfx_ctx_ps3_init,
   gfx_ctx_ps3_destroy,
   gfx_ctx_ps3_get_api,
   gfx_ctx_ps3_bind_api,
   gfx_ctx_ps3_set_swap_interval,
   gfx_ctx_ps3_set_video_mode,
   gfx_ctx_ps3_get_video_size,
   NULL, /* get_refresh_rate */
   gfx_ctx_ps3_get_video_output_size,
   gfx_ctx_ps3_get_video_output_prev,
   gfx_ctx_ps3_get_video_output_next,
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
   NULL
};
