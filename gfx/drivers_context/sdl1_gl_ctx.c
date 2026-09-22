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

#include <boolean.h>

#include "../../gfx/video_defines.h"
#include "../../gfx/video_driver.h"
#include "../../verbosity.h"

#include "SDL.h"

typedef struct gfx_ctx_sdl1_data
{
   SDL_Surface *win;

   int  width;
   int  height;
   int  new_width;
   int  new_height;

   bool full;
   bool resized;
   bool subsystem_inited;
} gfx_ctx_sdl1_data_t;

static void sdl1_ctx_destroy_resources(gfx_ctx_sdl1_data_t *sdl)
{
   if (!sdl)
      return;

   if (sdl->win)
      SDL_FreeSurface(sdl->win);
   sdl->win = NULL;
}

static void sdl1_ctx_destroy(void *data)
{
   gfx_ctx_sdl1_data_t *sdl = (gfx_ctx_sdl1_data_t*)data;

   if (!sdl)
      return;

   sdl1_ctx_destroy_resources(sdl);
   if (sdl->subsystem_inited)
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
   free(sdl);
}

static void *sdl1_ctx_init(void *video_driver)
{
   gfx_ctx_sdl1_data_t *sdl     = (gfx_ctx_sdl1_data_t*)
      calloc(1, sizeof(gfx_ctx_sdl1_data_t));
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   if (!sdl)
      return NULL;

#ifdef HAVE_X11
   XInitThreads();
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

   sdl1_ctx_destroy(sdl);

   return NULL;
}

static enum gfx_ctx_api sdl1_ctx_get_api(void *data) { return GFX_CTX_OPENGL_API; }

static bool sdl1_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor)
{
   if (api != GFX_CTX_OPENGL_API)
      return false;
   return true;
}

static void sdl1_ctx_swap_interval(void *data, int interval)
{
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, interval);
}

static bool sdl1_ctx_set_video_mode(void *data,
      unsigned dims,
      bool fullscreen)
{
   unsigned width  = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   unsigned fsflag          = 0;
   gfx_ctx_sdl1_data_t *sdl = (gfx_ctx_sdl1_data_t*)data;

   sdl->new_width           = width;
   sdl->new_height          = height;

   if (fullscreen)
      fsflag                = SDL_FULLSCREEN;

   sdl->win                 = SDL_SetVideoMode(width, height, 0, SDL_OPENGL | fsflag);

   if (!sdl->win)
      goto error;

   sdl->full                = fullscreen;
   sdl->width               = width;
   sdl->height              = height;

   return true;

error:
   RARCH_WARN("[SDL GL] Failed to set video mode: %s.\n", SDL_GetError());
   return false;
}

static void sdl1_ctx_get_video_size(void *data,
      unsigned *dims)
{
   gfx_ctx_sdl1_data_t *sdl = (gfx_ctx_sdl1_data_t*)data;

   if (!sdl)
      return;

   *dims = VIDEO_SCALE_PACK(sdl->width, sdl->height);

   if (!sdl->win)
   {
      SDL_Rect **modes      = SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
      SDL_Rect mode         = {0};
      if (!modes)
         RARCH_WARN("[SDL GL] Failed to detect available video modes: %s.\n",
                    SDL_GetError());
      else if (*modes)
         mode               = **modes;

      *dims = VIDEO_SCALE_PACK(mode.w, mode.h);
   }
}

static void sdl1_ctx_update_title(void *data)
{
   char title[128];
   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
      SDL_WM_SetCaption(title, NULL);
}

static void sdl1_ctx_check_window(void *data, bool *quit,
      bool *resize,unsigned *dims)
{
   SDL_Event event;
   gfx_ctx_sdl1_data_t *sdl = (gfx_ctx_sdl1_data_t*)data;

   SDL_PumpEvents();

   while (SDL_PeepEvents(&event, 1,
            SDL_GETEVENT, SDL_QUITMASK|SDL_VIDEORESIZEMASK) > 0)
   {
      switch (event.type)
      {
         case SDL_QUIT:
            *quit = true;
            break;
         case SDL_VIDEORESIZE:
            sdl->resized       = true;
            sdl->new_width     = event.resize.w;
            sdl->new_height    = event.resize.h;
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

static bool sdl1_ctx_has_focus(void *data)
{
   unsigned flags = (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
   return (SDL_GetAppState() & flags) == flags;
}

static void sdl1_ctx_swap_buffers(void *data)
{
   SDL_GL_SwapBuffers();
}

static void sdl1_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static gfx_ctx_proc_t sdl1_ctx_get_proc_address(const char *name)
{
   void *addr = SDL_GL_GetProcAddress(name);
   return *((gfx_ctx_proc_t*)(&addr));
}

static void sdl1_ctx_show_mouse(void *data, bool state) { SDL_ShowCursor(state); }

static uint32_t sdl1_ctx_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static bool sdl1_ctx_suppress_screensaver(void *data, bool enable) { return false; }
static void sdl1_ctx_set_flags(void *data, uint32_t flags) { }

/* SDL 1.2 has no shared GL context, but gl3.c calls this unguarded
 * whenever the core asks for hardware rendering, so the slot cannot be
 * NULL. */
static void sdl1_ctx_bind_hw_render(void *data, bool enable) { }

const gfx_ctx_driver_t gfx_ctx_sdl1_gl =
{
   sdl1_ctx_init,
   sdl1_ctx_destroy,
   sdl1_ctx_get_api,
   sdl1_ctx_bind_api,
   sdl1_ctx_swap_interval,
   sdl1_ctx_set_video_mode,
   sdl1_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   sdl1_ctx_update_title,
   sdl1_ctx_check_window,
   NULL, /* set_resize */
   sdl1_ctx_has_focus,
   sdl1_ctx_suppress_screensaver,
   true, /* has_windowed */
   sdl1_ctx_swap_buffers,
   sdl1_ctx_input_driver,
   sdl1_ctx_get_proc_address,
   NULL,
   NULL,
   sdl1_ctx_show_mouse,
   "gl_sdl",
   sdl1_ctx_get_flags,
   sdl1_ctx_set_flags,
   sdl1_ctx_bind_hw_render,
   NULL,
   NULL,
   NULL, /* create_surface */
   NULL, /* destroy_surface */
   NULL  /* presentable - SDL 1.2 has no way to ask, and is always presentable */
};
