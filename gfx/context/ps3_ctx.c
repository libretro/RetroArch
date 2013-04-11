/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "../../console/rarch_console.h"

#ifdef HAVE_LIBDBGFONT
#ifndef __PSL1GHT__
#include <cell/dbgfont.h>
#endif
#endif

#ifndef __PSL1GHT__
#include <sys/spu_initialize.h>
#endif

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../gl_common.h"
#include "../image.h"

#include "../gfx_context.h"
#include "../fonts/gl_font.h"

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

#ifdef HAVE_CG
#include "../shader_cg.h"
#endif

#if defined(HAVE_PSGL)
static PSGLdevice* gl_device;
static PSGLcontext* gl_context;
#endif

static unsigned gfx_ctx_get_resolution_width(unsigned resolution_id)
{
   CellVideoOutResolution resolution;
   cellVideoOutGetResolution(resolution_id, &resolution);

   return resolution.width;
}

static unsigned gfx_ctx_get_resolution_height(unsigned resolution_id)
{
   CellVideoOutResolution resolution;
   cellVideoOutGetResolution(resolution_id, &resolution);

   return resolution.height;
}

static float gfx_ctx_get_aspect_ratio(void)
{
   CellVideoOutState videoState;
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

static void rmenu_ctx_ps3_screenshot_enable(bool enable)
{
#if(CELL_SDK_VERSION > 0x340000)
   if(enable)
   {
      cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
      CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

      screenshot_param.photo_title = "RetroArch PS3";
      screenshot_param.game_title = "RetroArch PS3";
      cellScreenShotSetParameter (&screenshot_param);
      cellScreenShotEnable();
   }
   else
   {
      cellScreenShotDisable();
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
   }
#endif
}

static void gfx_ctx_get_available_resolutions (void)
{
   bool defaultresolution;
   uint32_t resolution_count;
   uint16_t num_videomodes;

   if (g_extern.console.screen.resolutions.check)
      return;

   defaultresolution = true;

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

   num_videomodes = sizeof(videomode) / sizeof(uint32_t);

   resolution_count = 0;
   for (unsigned i = 0; i < num_videomodes; i++)
   {
      if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, videomode[i],
               CELL_VIDEO_OUT_ASPECT_AUTO, 0))
         resolution_count++;
   }

   g_extern.console.screen.resolutions.list = malloc(resolution_count * sizeof(uint32_t));
   g_extern.console.screen.resolutions.count = 0;

   for (unsigned i = 0; i < num_videomodes; i++)
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

static void gfx_ctx_set_swap_interval(unsigned interval)
{
#if defined(HAVE_PSGL)
   if (gl_context)
   {
      if (interval)
         glEnable(GL_VSYNC_SCE);
      else
         glDisable(GL_VSYNC_SCE);
   }
#endif
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   gl_t *gl = driver.video_data;
   *quit = false;
   *resize = false;


   if (gl->quitting)
      *quit = true;

   if (gl->should_resize)
      *resize = true;
}

static bool gfx_ctx_has_focus(void)
{
   return true;
}

static void gfx_ctx_swap_buffers(void)
{
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

static void gfx_ctx_set_resize(unsigned width, unsigned height) { }

static void gfx_ctx_update_window_title(bool reset) { }

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
#if defined(HAVE_PSGL)
   psglGetDeviceDimensions(gl_device, width, height); 
#endif
}

static bool gfx_ctx_init(void)
{
#if defined(HAVE_PSGL)
   PSGLinitOptions options = {
      .enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS,
      .maxSPUs = 1,
      .initializeSPUs = GL_FALSE,
   };
#if CELL_SDK_VERSION < 0x340000
   options.enable |=	PSGL_INIT_HOST_MEMORY_SIZE;
#endif

   // Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL
   sys_spu_initialize(6, 1);
   psglInit(&options);

   PSGLdeviceParameters params;

   params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT |
      PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT |
      PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
   params.colorFormat = GL_ARGB_SCE;
   params.depthFormat = GL_NONE;
   params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE))
   {
      RARCH_LOG("[PSGL Context]: Setting triple buffering.\n");
      params.enable |= PSGL_DEVICE_PARAMETERS_BUFFERING_MODE;
      params.bufferingMode = PSGL_BUFFERING_MODE_TRIPLE;
   }

   if (g_extern.console.screen.resolutions.current.id)
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
      params.width = gfx_ctx_get_resolution_width(g_extern.console.screen.resolutions.current.id);
      params.height = gfx_ctx_get_resolution_height(g_extern.console.screen.resolutions.current.id);

      if (params.width == 720 && params.height == 576)
      {
         RARCH_LOG("[PSGL Context]: 720x576 resolution detected, setting MODE_VIDEO_PAL_ENABLE.\n");
         g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_PAL_ENABLE);
      }
      else
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_ENABLE);
   }

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
   {
      RARCH_LOG("[PSGL Context]: Setting temporal PAL60 mode.\n");
      params.enable |= PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE;
      params.enable |= PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE;
      params.rescPalTemporalMode = RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE;
      params.rescRatioMode = RESC_RATIO_MODE_FULLSCREEN;
   }

   gl_device = psglCreateDeviceExtended(&params);
   gl_context = psglCreateContext();

   psglMakeCurrent(gl_context, gl_device);
   psglResetCurrentContext();
#endif

   g_extern.console.screen.pal_enable = cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, CELL_VIDEO_OUT_RESOLUTION_576, CELL_VIDEO_OUT_ASPECT_AUTO, 0);

   gfx_ctx_get_available_resolutions();

   return true;
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   return true;
}

static void gfx_ctx_destroy(void)
{
#if defined(HAVE_PSGL)
   psglDestroyContext(gl_context);
   psglDestroyDevice(gl_device);

   psglExit();
#endif
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data) { }

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   return api == GFX_CTX_OPENGL_API || GFX_CTX_OPENGL_ES_API;
}

const gfx_ctx_driver_t gfx_ctx_ps3 = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_set_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   NULL,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   NULL,
   NULL,
   "ps3",
#ifdef HAVE_RMENU
   rmenu_ctx_ps3_screenshot_enable,
#endif
};

