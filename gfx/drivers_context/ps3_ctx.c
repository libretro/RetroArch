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

#include "../../driver.h"
#include "../../ps3/sdk_defines.h"

#ifdef HAVE_LIBDBGFONT
#ifndef __PSL1GHT__
#include <cell/dbgfont.h>
#endif
#endif

#include "../video_monitor.h"

#ifndef __PSL1GHT__
#include <sys/spu_initialize.h>
#endif

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gl_common.h"

#include "../video_context_driver.h"

typedef struct gfx_ctx_ps3_data
{
#if defined(HAVE_PSGL)
   PSGLdevice* gl_device;
   PSGLcontext* gl_context;
#endif
} gfx_ctx_ps3_data_t;

static float gfx_ctx_ps3_get_aspect_ratio(void *data)
{
   CellVideoOutState videoState;

   (void)data;

   cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);

   switch (videoState.displayMode.aspect)
   {
      case CELL_VIDEO_OUT_ASPECT_4_3:
         return 4.0f/3.0f;
      case CELL_VIDEO_OUT_ASPECT_16_9:
         return 16.0f/9.0f;
   }

   return 16.0f/9.0f;
}

static void gfx_ctx_ps3_get_available_resolutions(void)
{
   unsigned i;
   bool defaultresolution;
   uint32_t resolution_count;
   uint16_t num_videomodes;
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

   if (g_extern.console.screen.resolutions.check)
      return;

   defaultresolution = true;

   num_videomodes = sizeof(videomode) / sizeof(uint32_t);

   resolution_count = 0;

   for (i = 0; i < num_videomodes; i++)
   {
      if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, videomode[i],
               CELL_VIDEO_OUT_ASPECT_AUTO, 0))
         resolution_count++;
   }

   g_extern.console.screen.resolutions.list = malloc(resolution_count * sizeof(uint32_t));
   g_extern.console.screen.resolutions.count = 0;

   for (i = 0; i < num_videomodes; i++)
   {
      if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, videomode[i],
               CELL_VIDEO_OUT_ASPECT_AUTO, 0))
      {
         g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.count++] = videomode[i];
         g_extern.console.screen.resolutions.initial.id = videomode[i];

         if (g_extern.console.screen.resolutions.current.id == videomode[i])
         {
            defaultresolution = false;
            g_extern.console.screen.resolutions.current.idx = g_extern.console.screen.resolutions.count-1;
         }
      }
   }

   /* In case we didn't specify a resolution - make the last resolution
      that was added to the list (the highest resolution) the default resolution */
   if (g_extern.console.screen.resolutions.current.id > num_videomodes || defaultresolution)
      g_extern.console.screen.resolutions.current.idx = g_extern.console.screen.resolutions.count - 1;

   g_extern.console.screen.resolutions.check = true;
}

static void gfx_ctx_ps3_set_swap_interval(void *data, unsigned interval)
{
   gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)driver.video_context_data;

   (void)data;

#if defined(HAVE_PSGL)
   if (!ps3)
      return;
   if (!ps3->gl_context)
      return;

   if (interval)
      glEnable(GL_VSYNC_SCE);
   else
      glDisable(GL_VSYNC_SCE);
#endif
}

static void gfx_ctx_ps3_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   gl_t *gl = data;

   *quit = false;
   *resize = false;

   if (gl->quitting)
      *quit = true;

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

static bool gfx_ctx_ps3_has_windowed(void *data)
{
   (void)data;
   return false;
}

static void gfx_ctx_ps3_swap_buffers(void *data)
{
   (void)data;
#ifdef HAVE_LIBDBGFONT
   cellDbgFontDraw();
#endif
#ifdef HAVE_PSGL
   psglSwap();
#endif
#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif
}

static void gfx_ctx_ps3_set_resize(void *data,
      unsigned width, unsigned height) { }

static void gfx_ctx_ps3_update_window_title(void *data)
{
   (void)data;
   char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show || g_settings.fps_monitor_enable;

   if (!fps_draw)
      return;

   video_monitor_get_fps(buf, sizeof(buf),
         g_settings.fps_show ? buf_fps : NULL, sizeof(buf_fps));
   if (g_settings.fps_show)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static void gfx_ctx_ps3_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)driver.video_context_data;

   (void)data;

#if defined(HAVE_PSGL)
   if (ps3)
      psglGetDeviceDimensions(ps3->gl_device, width, height); 
#endif
}

static void gfx_ctx_ps3_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   unsigned ident = g_extern.console.screen.resolutions.current.id;
   if (!width || !height)
      return;

   CellVideoOutResolution resolution;
   cellVideoOutGetResolution(ident, &resolution);

   *width  = resolution.width;
   *height = resolution.height;
}

static bool gfx_ctx_ps3_init(void *data)
{
   gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)
      calloc(1, sizeof(gfx_ctx_ps3_data_t));

   (void)data;

   if (!ps3)
      return false;

#if defined(HAVE_PSGL)
   PSGLinitOptions options = {
      .enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS,
      .maxSPUs = 1,
      .initializeSPUs = GL_FALSE,
   };

   /* Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL. */
   sys_spu_initialize(6, 1);
   psglInit(&options);

   PSGLdeviceParameters params;

   params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT |
      PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT |
      PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
   params.colorFormat = GL_ARGB_SCE;
   params.depthFormat = GL_NONE;
   params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

   if (g_extern.console.screen.resolutions.current.id)
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
      
      gfx_ctx_ps3_get_video_output_size(data, &params.width, &params.height);
      g_extern.console.screen.pal_enable = false;

      if (params.width == 720 && params.height == 576)
      {
         RARCH_LOG("[PSGL Context]: 720x576 resolution detected, setting MODE_VIDEO_PAL_ENABLE.\n");
         g_extern.console.screen.pal_enable = true;
      }
   }

   if (g_extern.console.screen.pal60_enable)
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

   g_extern.console.screen.pal_enable = 
      cellVideoOutGetResolutionAvailability(
            CELL_VIDEO_OUT_PRIMARY, CELL_VIDEO_OUT_RESOLUTION_576,
            CELL_VIDEO_OUT_ASPECT_AUTO, 0);

   gfx_ctx_ps3_get_available_resolutions();

   driver.video_context_data = ps3;

   return true;
}

static bool gfx_ctx_ps3_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;

   if (g_extern.console.screen.resolutions.list[
         g_extern.console.screen.resolutions.current.idx] == 
         CELL_VIDEO_OUT_RESOLUTION_576)
   {
      if (g_extern.console.screen.pal_enable)
         g_extern.console.screen.pal60_enable = true;
   }
   else
   {
      g_extern.console.screen.pal_enable = false;
      g_extern.console.screen.pal60_enable = false;
   }

   rarch_main_command(RARCH_CMD_REINIT);
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
   (void)data;

   gfx_ctx_ps3_data_t *ps3 = (gfx_ctx_ps3_data_t*)driver.video_context_data;

   if (!ps3)
      return;

   gfx_ctx_ps3_destroy_resources(ps3);

   if (driver.video_context_data)
      free(driver.video_context_data);
   driver.video_context_data = NULL;
}

static void gfx_ctx_ps3_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   void *ps3input = NULL;

   (void)data;

   ps3input = input_ps3.init();

   *input = ps3input ? &input_ps3 : NULL;
   *input_data = ps3input;
}

static bool gfx_ctx_ps3_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;

   return api == GFX_CTX_OPENGL_API || GFX_CTX_OPENGL_ES_API;
}

const gfx_ctx_driver_t gfx_ctx_ps3 = {
   gfx_ctx_ps3_init,
   gfx_ctx_ps3_destroy,
   gfx_ctx_ps3_bind_api,
   gfx_ctx_ps3_set_swap_interval,
   gfx_ctx_ps3_set_video_mode,
   gfx_ctx_ps3_get_video_size,
   gfx_ctx_ps3_get_video_output_size,
   gfx_ctx_ps3_get_video_output_prev,
   gfx_ctx_ps3_get_video_output_next,
   NULL,
   gfx_ctx_ps3_update_window_title,
   gfx_ctx_ps3_check_window,
   gfx_ctx_ps3_set_resize,
   gfx_ctx_ps3_has_focus,
   gfx_ctx_ps3_suppress_screensaver,
   gfx_ctx_ps3_has_windowed,
   gfx_ctx_ps3_swap_buffers,
   gfx_ctx_ps3_input_driver,
   NULL,
   NULL,
   "ps3",
};

