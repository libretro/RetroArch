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
#include "../../verbosity.h"
#include "../common/gl_common.h"

#include "SDL.h"

#ifdef HAVE_SDL2
#include "../common/sdl2_common.h"
#endif

static enum gfx_ctx_api sdl_api = GFX_CTX_OPENGL_API;
static unsigned       g_major   = 2;
static unsigned       g_minor = 1;

typedef struct gfx_ctx_sdl_data
{
   int  g_width;
   int  g_height;
   int  g_new_width;
   int  g_new_height;

   bool g_full;
   bool g_resized;

#ifdef HAVE_SDL2
   SDL_Window    *g_win;
   SDL_GLContext  g_ctx;
#else
   SDL_Surface *g_win;
#endif
} gfx_ctx_sdl_data_t;

static void sdl_ctx_destroy_resources(gfx_ctx_sdl_data_t *sdl)
{
   if (!sdl)
      return;

#ifdef HAVE_SDL2
   if (sdl->g_ctx)
      SDL_GL_DeleteContext(sdl->g_ctx);

   if (sdl->g_win)
      SDL_DestroyWindow(sdl->g_win);

   sdl->g_ctx = NULL;
#else
   if (sdl->g_win)
      SDL_FreeSurface(sdl->g_win);
#endif
   sdl->g_win = NULL;

   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void *sdl_ctx_init(video_frame_info_t *video_info, void *video_driver)
{
   gfx_ctx_sdl_data_t *sdl = (gfx_ctx_sdl_data_t*)
      calloc(1, sizeof(gfx_ctx_sdl_data_t));

   if (!sdl)
      return NULL;

#ifdef HAVE_X11
   XInitThreads();
#endif

   if (SDL_WasInit(0) == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         goto error;
   }
   else if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
      goto error;

   RARCH_LOG("[SDL_GL] SDL %i.%i.%i gfx context driver initialized.\n",
           SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);

   return sdl;

error:
   RARCH_WARN("[SDL_GL]: Failed to initialize SDL gfx context driver: %s\n",
              SDL_GetError());

   sdl_ctx_destroy_resources(sdl);

   if (sdl)
      free(sdl);

   return NULL;
}

static void sdl_ctx_destroy(void *data)
{
   gfx_ctx_sdl_data_t *sdl = (gfx_ctx_sdl_data_t*)data;

   if (!sdl)
      return;

   sdl_ctx_destroy_resources(sdl);
   free(sdl);
}

static enum gfx_ctx_api sdl_ctx_get_api(void *data)
{
   return sdl_api;
}

static bool sdl_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor)
{
#ifdef HAVE_SDL2
   unsigned profile;

   if (api != GFX_CTX_OPENGL_API && api != GFX_CTX_OPENGL_ES_API)
      return false;

   profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;

   if (api == GFX_CTX_OPENGL_ES_API)
      profile = SDL_GL_CONTEXT_PROFILE_ES;

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
#endif

   sdl_api = api;
   g_major = major;
   g_minor = minor;

#ifndef HAVE_SDL2
   if (api != GFX_CTX_OPENGL_API)
      return false;
#endif
   return true;
}

static void sdl_ctx_swap_interval(void *data, int interval)
{
   (void)data;
#ifdef HAVE_SDL2
   SDL_GL_SetSwapInterval(interval);
#else
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, interval);
#endif
}

static bool sdl_ctx_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   unsigned fsflag         = 0;
   gfx_ctx_sdl_data_t *sdl = (gfx_ctx_sdl_data_t*)data;

   sdl->g_new_width  = width;
   sdl->g_new_height = height;

#ifdef HAVE_SDL2

   if (fullscreen)
   {
      if (video_info->windowed_fullscreen)
         fsflag = SDL_WINDOW_FULLSCREEN_DESKTOP;
      else
         fsflag = SDL_WINDOW_FULLSCREEN;
   }

   if (sdl->g_win)
   {
      SDL_SetWindowSize(sdl->g_win, width, height);

      if (fullscreen)
         SDL_SetWindowFullscreen(sdl->g_win, fsflag);
   }
   else
   {
      unsigned display = video_info->monitor_index;

      sdl->g_win = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
                               SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
                               width, height, SDL_WINDOW_OPENGL | fsflag);
   }
#else
   if (fullscreen)
      fsflag = SDL_FULLSCREEN;

   sdl->g_win = SDL_SetVideoMode(width, height, 0, SDL_OPENGL | fsflag);
#endif

   if (!sdl->g_win)
      goto error;

#ifdef HAVE_SDL2
#if defined(_WIN32)
   sdl2_set_handles(sdl->g_win, RARCH_DISPLAY_WIN32);
#elif defined(HAVE_X11)
   sdl2_set_handles(sdl->g_win, RARCH_DISPLAY_X11);
#elif defined(HAVE_COCOA)
   sdl2_set_handles(sdl->g_win, RARCH_DISPLAY_OSX);
#endif

   if (sdl->g_ctx)
      video_driver_set_video_cache_context_ack();
   else
   {
      sdl->g_ctx = SDL_GL_CreateContext(sdl->g_win);

      if (!sdl->g_ctx)
         goto error;
   }
#endif

   sdl->g_full   = fullscreen;
   sdl->g_width  = width;
   sdl->g_height = height;

   return true;

error:
   RARCH_WARN("[SDL_GL]: Failed to set video mode: %s\n", SDL_GetError());
   return false;
}

static void sdl_ctx_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   settings_t    *settings = config_get_ptr();
   gfx_ctx_sdl_data_t *sdl = (gfx_ctx_sdl_data_t*)data;

   if (!sdl)
      return;

   *width  = sdl->g_width;
   *height = sdl->g_height;

   if (!sdl->g_win)
   {
#ifdef HAVE_SDL2
      SDL_DisplayMode mode = {0};
      int i = settings->uints.video_monitor_index;

      if (SDL_GetCurrentDisplayMode(i, &mode) < 0)
         RARCH_WARN("[SDL_GL]: Failed to get display #%i mode: %s\n", i,
                    SDL_GetError());
#else
      SDL_Rect **modes = SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
      SDL_Rect mode = {0};

      if (!modes)
         RARCH_WARN("[SDL_GL]: Failed to detect available video modes: %s\n",
                    SDL_GetError());
      else if (*modes)
         mode = **modes;
#endif

      *width  = mode.w;
      *height = mode.h;
   }
}

static void sdl_ctx_update_title(void *data, void *data2)
{
   char title[128];
   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
   {
#ifdef HAVE_SDL2
      SDL_SetWindowTitle((SDL_Window*)video_driver_display_userdata_get(), title);
#else
      SDL_WM_SetCaption(title, NULL);
#endif
   }
}

static void sdl_ctx_check_window(void *data, bool *quit,
      bool *resize,unsigned *width,
      unsigned *height,
      bool is_shutdown)
{
   SDL_Event event;
   gfx_ctx_sdl_data_t *sdl = (gfx_ctx_sdl_data_t*)data;

   SDL_PumpEvents();

#ifdef HAVE_SDL2
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_WINDOWEVENT) > 0)
#else
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUITMASK|SDL_VIDEORESIZEMASK) > 0)
#endif
   {
      switch (event.type)
      {
         case SDL_QUIT:
#ifdef HAVE_SDL2
         case SDL_APP_TERMINATING:
#endif
            *quit = true;
            break;
#ifdef HAVE_SDL2
         case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
               sdl->g_resized = true;
               sdl->g_new_width  = event.window.data1;
               sdl->g_new_height = event.window.data2;
            }
#else
         case SDL_VIDEORESIZE:
            sdl->g_resized = true;
            sdl->g_new_width  = event.resize.w;
            sdl->g_new_height = event.resize.h;
#endif
            break;
         default:
            break;
      }
   }

   if (sdl->g_resized)
   {
      *width    = sdl->g_new_width;
      *height   = sdl->g_new_height;
      *resize   = true;
      sdl->g_resized = false;
   }
}

static bool sdl_ctx_has_focus(void *data)
{
   unsigned flags;

#ifdef HAVE_SDL2
   gfx_ctx_sdl_data_t *sdl = (gfx_ctx_sdl_data_t*)data;
   flags = (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
   return (SDL_GetWindowFlags(sdl->g_win) & flags) == flags;
#else
   flags = (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
   return (SDL_GetAppState() & flags) == flags;
#endif
}

static bool sdl_ctx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void sdl_ctx_swap_buffers(void *data, void *data2)
{
#ifdef HAVE_SDL2
   gfx_ctx_sdl_data_t *sdl = (gfx_ctx_sdl_data_t*)data;
   if (sdl)
      SDL_GL_SwapWindow(sdl->g_win);
#else
   SDL_GL_SwapBuffers();
#endif
}

static void sdl_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static gfx_ctx_proc_t sdl_ctx_get_proc_address(const char *name)
{
   return (gfx_ctx_proc_t)SDL_GL_GetProcAddress(name);
}

static void sdl_ctx_show_mouse(void *data, bool state)
{
   (void)data;
   SDL_ShowCursor(state);
}

static uint32_t sdl_ctx_get_flags(void *data)
{
   uint32_t flags = 0;

   return flags;
}

static void sdl_ctx_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_sdl_gl =
{
   sdl_ctx_init,
   sdl_ctx_destroy,
   sdl_ctx_get_api,
   sdl_ctx_bind_api,
   sdl_ctx_swap_interval,
   sdl_ctx_set_video_mode,
   sdl_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   sdl_ctx_update_title,
   sdl_ctx_check_window,
   NULL, /* set_resize */
   sdl_ctx_has_focus,
   sdl_ctx_suppress_screensaver,
   true, /* has_windowed */
   sdl_ctx_swap_buffers,
   sdl_ctx_input_driver,
   sdl_ctx_get_proc_address,
   NULL,
   NULL,
   sdl_ctx_show_mouse,
   "sdl_gl",
   sdl_ctx_get_flags,
   sdl_ctx_set_flags,
   NULL, /* bind_hw_render */
   NULL,
   NULL
};
