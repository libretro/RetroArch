/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include "../gfx_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"

#include "SDL.h"

static enum gfx_ctx_api g_api = GFX_CTX_OPENGL_API;
static unsigned       g_major = 2;
static unsigned       g_minor = 1;

static int  g_width  = 0;
static int  g_height = 0;
static int  g_new_width = 0;
static int  g_new_height = 0;

static bool g_full   = false;
static bool g_resized = false;

static int g_frame_count = 0;

#ifdef HAVE_SDL2
static SDL_Window    *g_win = NULL;
static SDL_GLContext  g_ctx = NULL;
#else
static SDL_Surface *g_win = NULL;
#endif

static void sdl_ctx_destroy(void *data);
static bool sdl_ctx_init(void *data)
{
   (void)data;

#ifdef HAVE_SDL2
   if (g_ctx)
      return false;
#else
   if (g_win)
      return false;
#endif

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

   return true;

error:
   RARCH_WARN("[SDL_GL]: Failed to initialize SDL gfx context driver: %s\n",
              SDL_GetError());

   sdl_ctx_destroy(data);
   return false;
}

static void sdl_ctx_destroy(void *data)
{
   (void)data;
#ifdef HAVE_SDL2
   if (g_ctx)
      SDL_GL_DeleteContext(g_ctx);

   if (g_win)
      SDL_DestroyWindow(g_win);

   g_ctx = NULL;
#else
   if (g_win)
      SDL_FreeSurface(g_win);
#endif
   g_win = NULL;

   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static bool sdl_ctx_bind_api(void *data, enum gfx_ctx_api api, unsigned major,
                             unsigned minor)
{
   (void)data;

#ifdef HAVE_SDL2
   if (api != GFX_CTX_OPENGL_API && api != GFX_CTX_OPENGL_ES_API)
      return false;

   unsigned profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;

   if (api == GFX_CTX_OPENGL_ES_API)
      profile = SDL_GL_CONTEXT_PROFILE_ES;

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
#endif
   g_api   = api;
   g_major = major;
   g_minor = minor;

#ifdef HAVE_SDL2
   return true;
#else
   return api == GFX_CTX_OPENGL_API;
#endif
}

static void sdl_ctx_swap_interval(void *data, unsigned interval)
{
   (void)data;
#ifdef HAVE_SDL2
   SDL_GL_SetSwapInterval(interval);
#else
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, interval);
#endif
}

static bool sdl_ctx_set_video_mode(void *data, unsigned width, unsigned height,
                                   bool fullscreen)
{
   (void)data;

   g_new_width  = width;
   g_new_height = height;

   unsigned fsflag = 0;

#ifdef HAVE_SDL2

   if (fullscreen)
   {
      if (g_settings.video.windowed_fullscreen)
         fsflag = SDL_WINDOW_FULLSCREEN_DESKTOP;
      else
         fsflag = SDL_WINDOW_FULLSCREEN;
   }

   if (g_win)
   {
      SDL_SetWindowSize(g_win, width, height);

      if (fullscreen)
         SDL_SetWindowFullscreen(g_win, fsflag);
   }
   else
   {
      unsigned display = g_settings.video.monitor_index;
      g_win = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
                               SDL_WINDOWPOS_UNDEFINED_DISPLAY(display),
                               width, height, SDL_WINDOW_OPENGL | fsflag);
   }
#else
   if (fullscreen)
      fsflag = SDL_FULLSCREEN;

   g_win = SDL_SetVideoMode(width, height, 0, SDL_OPENGL | fsflag);
#endif

   if (!g_win)
      goto error;

#ifdef HAVE_SDL2
   if (g_ctx)
   {
      driver.video_cache_context_ack = true;
   }
   else
   {
      g_ctx = SDL_GL_CreateContext(g_win);

      if (!g_ctx)
         goto error;
   }
#endif

   g_full   = fullscreen;
   g_width  = width;
   g_height = height;

   return true;

error:
   RARCH_WARN("[SDL_GL]: Failed to set video mode: %s\n", SDL_GetError());
   return false;
}

static void sdl_ctx_get_video_size(void *data, unsigned *width, unsigned *height)
{
   if (!g_win)
   {
      int i = g_settings.video.monitor_index;

#ifdef HAVE_SDL2
      SDL_DisplayMode mode = {0};

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
   else
   {
      *width  = g_width;
      *height = g_height;
   }
}

static void sdl_ctx_update_window_title(void *data)
{
   (void)data;
   char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show;
   if (gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps)))
   {
#ifdef HAVE_SDL2
      SDL_SetWindowTitle(g_win, buf);
#else
      SDL_WM_SetCaption(buf, NULL);
#endif
   }

   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static void sdl_ctx_check_window(void *data, bool *quit, bool *resize,unsigned *width,
                            unsigned *height, unsigned frame_count)
{
   (void)data;

   SDL_Event event;
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
               g_resized = true;
               g_new_width  = event.window.data1;
               g_new_height = event.window.data2;
            }
#else
         case SDL_VIDEORESIZE:
            g_resized = true;
            g_new_width  = event.resize.w;
            g_new_height = event.resize.h;
#endif
            break;
         default:
            break;
      }
   }

   if (g_resized)
   {
      *width    = g_new_width;
      *height   = g_new_height;
      *resize   = true;
      g_resized = false;
   }

   g_frame_count = frame_count;
}

static void sdl_ctx_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static bool sdl_ctx_has_focus(void *data)
{
   (void)data;

#ifdef HAVE_SDL2
   unsigned flags = (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
   return (SDL_GetWindowFlags(g_win) & flags) == flags;
#else
   unsigned flags = (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
   return (SDL_GetAppState() & flags) == flags;
#endif
}

static bool sdl_ctx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void sdl_ctx_swap_buffers(void *data)
{
   (void)data;
#ifdef HAVE_SDL2
   SDL_GL_SwapWindow(g_win);
#else
   SDL_GL_SwapBuffers();
#endif
}

static void sdl_ctx_input_driver(void *data, const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
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

const gfx_ctx_driver_t gfx_ctx_sdl_gl =
{
   sdl_ctx_init,
   sdl_ctx_destroy,
   sdl_ctx_bind_api,
   sdl_ctx_swap_interval,
   sdl_ctx_set_video_mode,
   sdl_ctx_get_video_size,
   NULL, /* translate_aspect */
   sdl_ctx_update_window_title,
   sdl_ctx_check_window,
   sdl_ctx_set_resize,
   sdl_ctx_has_focus,
   sdl_ctx_has_windowed,
   sdl_ctx_swap_buffers,
   sdl_ctx_input_driver,
   sdl_ctx_get_proc_address,
#ifdef HAVE_EGL
   NULL,
   NULL,
#endif
   sdl_ctx_show_mouse,
   "sdl_gl",
   NULL /* bind_hw_render */
};
