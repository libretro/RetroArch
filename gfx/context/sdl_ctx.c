/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "../gfx_context.h"
#include "../gfx_common.h"
#include "../../general.h"

#ifdef HAVE_X11
#include "x11_common.h"
#endif

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#else
#include "SDL/SDL_syswm.h"
#endif

#include "SDL.h"

#include "../math/matrix.h"

// SDL 1.2 is portable, sure, but you still need some platform specific workarounds ;)
// Hopefully SDL 1.3 will solve this more cleanly :D
// Welcome to #ifdef HELL. :D
//

#define GL_SYM_WRAP(symbol, proc) if (!symbol) { \
   gfx_ctx_proc_t sym = gfx_ctx_get_proc_address(proc); \
   memcpy(&(symbol), &sym, sizeof(sym)); \
}

static bool g_fullscreen;
static unsigned g_interval;
static bool g_inited;

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol);

static void gfx_ctx_swap_interval(unsigned interval)
{
   g_interval = interval;

   bool success = true;
   if (g_inited)
   {
#if defined(_WIN32)
      static BOOL (APIENTRY *wgl_swap_interval)(int) = NULL;
      if (!wgl_swap_interval)
         GL_SYM_WRAP(wgl_swap_interval, "wglSwapIntervalEXT");
      if (wgl_swap_interval)
         success = wgl_swap_interval(g_interval);
#elif defined(__APPLE__) && defined(HAVE_OPENGL)
      GLint val = g_interval;
      success = CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &val) == 0;
#else
      static int (*glx_swap_interval)(int) = NULL;
      if (!glx_swap_interval)
         GL_SYM_WRAP(glx_swap_interval, "glXSwapInterval");
      if (!glx_swap_interval)
         GL_SYM_WRAP(glx_swap_interval, "glXSwapIntervalMESA");
      if (!glx_swap_interval)
         GL_SYM_WRAP(glx_swap_interval, "glXSwapIntervalSGI");
      if (glx_swap_interval)
         success = glx_swap_interval(g_interval) == 0;
      else
         RARCH_WARN("Could not find GLX VSync call.\n");
#endif
   }

   if (!success)
      RARCH_WARN("Failed to set swap interval.\n");
}

static void gfx_ctx_wm_set_caption(const char *str)
{
   SDL_WM_SetCaption(str, NULL);
}

static void gfx_ctx_update_window_title(void)
{
   char buf[128];
   if (gfx_get_fps(buf, sizeof(buf), false))
      gfx_ctx_wm_set_caption(buf);
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
   rarch_assert(video_info);
   *width  = video_info->current_w;
   *height = video_info->current_h;
}

static bool gfx_ctx_init(void)
{
   if (SDL_WasInit(SDL_INIT_VIDEO))
      return true;

   bool ret = SDL_Init(SDL_INIT_VIDEO) == 0;
   if (!ret)
      RARCH_ERR("Failed to init SDL video.\n");

   return ret;
}

static void gfx_ctx_destroy(void)
{
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
   g_inited = false;
}

static void sdl_set_handles(void)
{
#if defined(_WIN32)
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (SDL_GetWMInfo(&info) == 1)
   {
      driver.display_type  = RARCH_DISPLAY_WIN32;
      driver.video_display = 0;
      driver.video_window  = (uintptr_t)info.window;
   }
#elif defined(HAVE_X11)
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (SDL_GetWMInfo(&info) == 1)
   {
      driver.display_type  = RARCH_DISPLAY_X11;
      driver.video_display = (uintptr_t)info.info.x11.display;
      driver.video_window  = (uintptr_t)info.info.x11.window;
   }
#endif
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef __APPLE__ // Resizing on OSX is broken in 1.2 it seems :)
   static const int resizable = SDL_RESIZABLE;
#else
   static const int resizable = 0;
#endif

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, g_interval);

   if (!SDL_SetVideoMode(width, height, 0,
         SDL_OPENGL | (fullscreen ? SDL_FULLSCREEN : resizable)))
   {
      RARCH_ERR("Failed to create SDL window.\n");
      return false;
   }

   g_inited = true;

   int attr = 0;
   SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &attr);
   if (attr <= 0 && g_interval)
   {
      RARCH_WARN("SDL failed to setup VSync, attempting to recover using native calls.\n");
      gfx_ctx_swap_interval(g_interval);
   }

   g_fullscreen = fullscreen;

   attr = 0;
   SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &attr);
   if (attr <= 0)
      RARCH_WARN("GL double buffer has not been enabled.\n");

   // Remove that ugly mouse :D
   if (fullscreen)
      SDL_ShowCursor(SDL_DISABLE);

   sdl_set_handles();

   return true;
}

// SDL 1.2 has an awkward model where you need to "confirm" window resizing.
static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
#ifndef __APPLE__ // Resizing on OSX is broken in 1.2 it seems :)
   SDL_SetVideoMode(width, height, 0, SDL_OPENGL | (g_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE));
#else
   // Resize on OSX is broken.
   (void)width;
   (void)height;
#endif
}

static void gfx_ctx_swap_buffers(void)
{
   SDL_GL_SwapBuffers();
}

// 1.2 specific workaround for tiling WMs.
#if defined(HAVE_X11)
// This X11 is set on OSX for some reason.
static bool gfx_ctx_get_window_size(unsigned *width, unsigned *height)
{
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (SDL_GetWMInfo(&info) != 1)
      return false;

   XWindowAttributes target;

   info.info.x11.lock_func();
   XGetWindowAttributes(info.info.x11.display, info.info.x11.window,
         &target);
   info.info.x11.unlock_func();

   *width = target.width;
   *height = target.height;
   return true;
}
#endif

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   *quit   = false;
   *resize = false;

   SDL_Event event;
   while (SDL_PollEvent(&event))
   {
      switch (event.type)
      {
         case SDL_QUIT:
            *quit = true;
            break;

         case SDL_VIDEORESIZE:
            *resize = true;
            *width  = event.resize.w;
            *height = event.resize.h;
            break;

         case SDL_KEYDOWN:
         case SDL_KEYUP:
            if (g_extern.system.key_event)
            {
               SDL_EnableUNICODE(true);

               uint16_t mods = 0;

               if (event.key.keysym.mod)
               {
                  mods |= (event.key.keysym.mod & KMOD_CTRL)  ? RETROKMOD_CTRL : 0;
                  mods |= (event.key.keysym.mod & KMOD_ALT)   ? RETROKMOD_ALT : 0;
                  mods |= (event.key.keysym.mod & KMOD_SHIFT) ? RETROKMOD_SHIFT : 0;
                  mods |= (event.key.keysym.mod & KMOD_META)  ? RETROKMOD_META : 0;
                  mods |= (event.key.keysym.mod & KMOD_NUM)   ? RETROKMOD_NUMLOCK : 0;
                  mods |= (event.key.keysym.mod & KMOD_CAPS)  ? RETROKMOD_CAPSLOCK : 0;

                  // TODO: What is KMOD_MODE in SDL?
                  mods |= (event.key.keysym.mod & KMOD_MODE)  ? RETROKMOD_SCROLLOCK : 0;
               }

               // For now it seems that all RETROK_* constant values match the SDLK_* values.
               // Ultimately the table in sdl_input.c should be used in case this changes.
               // TODO: event.key.keysym.unicode is UTF-16
               g_extern.system.key_event(event.type == SDL_KEYDOWN, event.key.keysym.sym, event.key.keysym.unicode, mods);
            }
            break;
      }
   }

#if defined(HAVE_X11)
   if (!*resize && !g_fullscreen)
   {
      unsigned new_width, new_height;

      // Hack to workaround bugs in tiling WMs ... Very ugly.
      // We trigger a resize, to force a resize event to occur.
      // Some tiling WMs will immediately change window size, and not trigger an event in SDL to notify that the window
      // size has in fact, changed.
      // By forcing a dummy resize to original size, we can make sure events are triggered.
      if (gfx_ctx_get_window_size(&new_width, &new_height) &&
            ((new_width != *width || new_height != *height) || (frame_count == 10))) // 10 here is chosen arbitrarily.
      {
         *resize = true;
         *width  = new_width;
         *height = new_height;
         RARCH_LOG("GL: Verified window size: %u x %u\n", *width, *height);
      }
   }
#endif
}

static bool gfx_ctx_has_focus(void)
{
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   void *sdl_input = input_sdl.init();
   if (sdl_input)
   {
      *input = &input_sdl;
      *input_data = sdl_input;
   }
   else
      *input = NULL;
}

// Enforce void (*)(void) as it's not really legal to cast void* to fn-pointer.
// POSIX allows this, but strict C99 doesn't.
static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   // This will not fail on any system RetroArch would run on, but let's just be defensive.
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));

   gfx_ctx_proc_t ret;

   void *sym__ = SDL_GL_GetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   return api == GFX_CTX_OPENGL_API;
}

#ifdef HAVE_EGL
static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}
#endif

static void gfx_ctx_show_mouse(bool state)
{
   SDL_ShowCursor(state ? SDL_ENABLE : SDL_DISABLE);
}

const gfx_ctx_driver_t gfx_ctx_sdl_gl = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   NULL,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
#ifdef HAVE_EGL
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
#endif
   gfx_ctx_show_mouse,
   "sdl-gl",
};

