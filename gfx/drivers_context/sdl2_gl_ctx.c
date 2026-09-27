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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

#include "../../configuration.h"
#include "../../gfx/video_defines.h"
#include "../../gfx/video_driver.h"
#include "../../verbosity.h"

#include "SDL.h"

#include "../common/sdl2_common.h"

#ifdef WEBOS
#include <SDL_webOS.h>
#endif

typedef struct gfx_ctx_sdl2_data
{
   SDL_Window    *win;
   SDL_GLContext  ctx;
   SDL_GLContext  shared_ctx;

   int  width;
   int  height;
   int  new_width;
   int  new_height;

   bool full;
   bool resized;
   bool subsystem_inited;
} gfx_ctx_sdl2_data_t;

static void sdl2_ctx_destroy_resources(gfx_ctx_sdl2_data_t *sdl)
{
   if (!sdl)
      return;

   if (sdl->ctx)
      SDL_GL_DeleteContext(sdl->ctx);

   if (sdl->shared_ctx)
      SDL_GL_DeleteContext(sdl->shared_ctx);

   if (sdl->win)
      SDL_DestroyWindow(sdl->win);

   sdl->ctx = NULL;
   sdl->shared_ctx = NULL;
   sdl->win = NULL;
}

static void sdl2_ctx_destroy(void *data)
{
   gfx_ctx_sdl2_data_t *sdl = (gfx_ctx_sdl2_data_t*)data;

   if (!sdl)
      return;

   sdl2_ctx_destroy_resources(sdl);
#ifndef WEBOS
   if (sdl->subsystem_inited)
#endif
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
   free(sdl);
}

static void *sdl2_ctx_init(void *video_driver)
{
   gfx_ctx_sdl2_data_t *sdl     = (gfx_ctx_sdl2_data_t*)
      calloc(1, sizeof(gfx_ctx_sdl2_data_t));
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   if (!sdl)
      return NULL;

#ifdef HAVE_X11
   XInitThreads();
#endif

#ifdef WEBOS
   SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK, "true");
   SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT, "true");
   SDL_SetHint(SDL_HINT_WEBOS_CURSOR_SLEEP_TIME, "5000");
#endif

   /* Initialise graphics subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         goto error;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_VIDEO) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
         goto error;
      sdl->subsystem_inited = true;
   }

   RARCH_LOG("[SDL GL] SDL %i.%i.%i gfx context driver initialized.\n",
         SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);

   return sdl;

error:
   RARCH_WARN("[SDL GL] Failed to initialize SDL gfx context driver: %s.\n",
         SDL_GetError());

   sdl2_ctx_destroy(sdl);

   return NULL;
}

static enum gfx_ctx_api sdl2_ctx_get_api(void *data) { return GFX_CTX_OPENGL_API; }

static bool sdl2_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor)
{
   unsigned profile;

   if (api != GFX_CTX_OPENGL_API && api != GFX_CTX_OPENGL_ES_API)
      return false;

   profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;

   if (api == GFX_CTX_OPENGL_ES_API)
      profile = SDL_GL_CONTEXT_PROFILE_ES;

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);

   return true;
}

static void sdl2_ctx_swap_interval(void *data, int interval)
{
   SDL_GL_SetSwapInterval(interval);
}

static bool sdl2_ctx_set_video_mode(void *data,
      unsigned dims,
      bool fullscreen)
{
   unsigned width  = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   unsigned fsflag              = 0;
   gfx_ctx_sdl2_data_t *sdl     = (gfx_ctx_sdl2_data_t*)data;
   settings_t *settings         = config_get_ptr();
   bool windowed_fullscreen     = settings->bools.video_windowed_fullscreen;
   unsigned video_monitor_index = settings->uints.video_monitor_index;

   sdl->new_width               = width;
   sdl->new_height              = height;

   if (fullscreen)
   {
      if (windowed_fullscreen)
         fsflag                 = SDL_WINDOW_FULLSCREEN_DESKTOP;
      else
         fsflag                 = SDL_WINDOW_FULLSCREEN;
   }

   if (sdl->win)
   {
      SDL_SetWindowSize(sdl->win, width, height);

      if (fullscreen)
         SDL_SetWindowFullscreen(sdl->win, fsflag);
   }
   else
   {
      unsigned display          = video_monitor_index;
      sdl->win                  = SDL_CreateWindow("RetroArch",
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
            width, height,
            SDL_WINDOW_OPENGL | fsflag);
   }

   if (!sdl->win)
      goto error;

#if defined(_WIN32)
   sdl2_set_handles(sdl->win, RARCH_DISPLAY_WIN32);
#elif defined(HAVE_X11)
   sdl2_set_handles(sdl->win, RARCH_DISPLAY_X11);
#elif defined(HAVE_COCOA)
   sdl2_set_handles(sdl->win, RARCH_DISPLAY_OSX);
#endif

   if (sdl->ctx)
   {
      video_driver_cache_context_ack_set();
      RARCH_LOG("[SDL GL] Using cached GL context.\n");
   }
   else
   {
      if (!(sdl->ctx = SDL_GL_CreateContext(sdl->win)))
         goto error;
   }

   sdl->full                    = fullscreen;
   sdl->width                   = width;
   sdl->height                  = height;

   return true;

error:
   RARCH_WARN("[SDL GL] Failed to set video mode: %s.\n", SDL_GetError());
   return false;
}

static void sdl2_ctx_get_video_size(void *data,
      unsigned *dims)
{
   settings_t    *settings  = config_get_ptr();
   gfx_ctx_sdl2_data_t *sdl = (gfx_ctx_sdl2_data_t*)data;

   if (!sdl)
      return;

   *dims = VIDEO_SCALE_PACK(sdl->width, sdl->height);

   if (!sdl->win)
   {
      SDL_DisplayMode mode = {0};
      int i                = settings->uints.video_monitor_index;
      if (SDL_GetCurrentDisplayMode(i, &mode) < 0)
         RARCH_WARN("[SDL GL] Failed to get display #%i mode: %s.\n", i,
                    SDL_GetError());

      *dims = VIDEO_SCALE_PACK(mode.w, mode.h);
   }
}

static void sdl2_ctx_update_title(void *data)
{
   char title[128];
   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
      SDL_SetWindowTitle((SDL_Window*)
            video_driver_display_userdata_get(), title);
}

static void sdl2_ctx_check_window(void *data, bool *quit,
      bool *resize,unsigned *dims)
{
   SDL_Event event;
   gfx_ctx_sdl2_data_t *sdl = (gfx_ctx_sdl2_data_t*)data;

   SDL_PumpEvents();

   while (SDL_PeepEvents(&event, 1,
            SDL_GETEVENT, SDL_QUIT, SDL_WINDOWEVENT) > 0)
   {
      switch (event.type)
      {
         case SDL_QUIT:
         case SDL_APP_TERMINATING:
            *quit = true;
            break;
         case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
               sdl->resized    = true;
               sdl->new_width  = event.window.data1;
               sdl->new_height = event.window.data2;
            }
            break;
         default:
            break;
      }
   }

   if (sdl->resized)
   {
      *dims         = VIDEO_SCALE_PACK(sdl->new_width, sdl->new_height);
      *resize        = true;
      sdl->resized   = false;
   }
}

static bool sdl2_ctx_has_focus(void *data)
{
   gfx_ctx_sdl2_data_t *sdl = (gfx_ctx_sdl2_data_t*)data;
#ifdef WEBOS
   /* We do not receive mouse focus when non-magic remote is used. */
   unsigned flags           = (SDL_WINDOW_INPUT_FOCUS);
#else
   unsigned flags           = (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
#endif
   return (SDL_GetWindowFlags(sdl->win) & flags) == flags;
}

static void sdl2_ctx_swap_buffers(void *data)
{
   gfx_ctx_sdl2_data_t *sdl = (gfx_ctx_sdl2_data_t*)data;
   if (sdl)
      SDL_GL_SwapWindow(sdl->win);
}

static void sdl2_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static gfx_ctx_proc_t sdl2_ctx_get_proc_address(const char *name)
{
   void *addr = SDL_GL_GetProcAddress(name);
   return *((gfx_ctx_proc_t*)(&addr));
}

static void sdl2_ctx_show_mouse(void *data, bool state) { SDL_ShowCursor(state); }

static uint32_t sdl2_ctx_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static bool sdl2_ctx_suppress_screensaver(void *data, bool enable) { return false; }
static void sdl2_ctx_set_flags(void *data, uint32_t flags) { }

static void sdl2_ctx_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_sdl2_data_t *sdl = (gfx_ctx_sdl2_data_t*)data;
   if (!sdl || !sdl->win || !sdl->ctx)
      return;

   if (enable)
   {
      if (!sdl->shared_ctx)
      {
         SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
         SDL_GL_MakeCurrent(sdl->win, sdl->ctx);
         sdl->shared_ctx = SDL_GL_CreateContext(sdl->win);
         if (!sdl->shared_ctx)
         {
            RARCH_ERR("[SDL_GL]: Failed to create shared GL context: %s\n", SDL_GetError());
            return;
         }
      }
      SDL_GL_MakeCurrent(sdl->win, sdl->shared_ctx);
   }
   else
   {
      SDL_GL_MakeCurrent(sdl->win, sdl->ctx);
   }
}

/* A minimised or hidden window has nothing behind it to present to:
 * SDL_GL_SwapWindow() returns at once instead of blocking to vblank,
 * so with vsync as the only pacing the loop would spin. SDL keeps the
 * state, so there is none of ours to keep. */
static bool sdl2_ctx_presentable(void *data)
{
   gfx_ctx_sdl2_data_t *sdl = (gfx_ctx_sdl2_data_t*)data;
   if (sdl && sdl->win)
      return !(SDL_GetWindowFlags(sdl->win)
            & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
   return true;
}

const gfx_ctx_driver_t gfx_ctx_sdl2_gl =
{
   sdl2_ctx_init,
   sdl2_ctx_destroy,
   sdl2_ctx_get_api,
   sdl2_ctx_bind_api,
   sdl2_ctx_swap_interval,
   sdl2_ctx_set_video_mode,
   sdl2_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   sdl2_ctx_update_title,
   sdl2_ctx_check_window,
   NULL, /* set_resize */
   sdl2_ctx_has_focus,
   sdl2_ctx_suppress_screensaver,
   true, /* has_windowed */
   sdl2_ctx_swap_buffers,
   sdl2_ctx_input_driver,
   sdl2_ctx_get_proc_address,
   NULL,
   NULL,
   sdl2_ctx_show_mouse,
   "gl_sdl",
   sdl2_ctx_get_flags,
   sdl2_ctx_set_flags,
   sdl2_ctx_bind_hw_render,
   NULL,
   NULL,
   NULL, /* create_surface */
   NULL, /* destroy_surface */
   sdl2_ctx_presentable
};
